#include "helpers.h"

#include <Arduino.h>
#include <Preferences.h> // For Preferences object
#include <stddef.h>      // For offsetof
#include <stdint.h>      // For uint16_t, uint8_t
#include <stdio.h>       // For snprintf
#include <stdlib.h>      // For malloc/free
#include <string.h>      // For memcpy

#include "globals.h"
#include "formats.h"
#include "lens_spike_logic.h" // LensMovingAverageState
#include "lenses.h"       // Now includes NUM_LENSES
#include "mrfconstants.h" // For SMOOTHING_WINDOW_SIZE
#include "prefs_clamp_logic.h" // Loaded-prefs sanitization rules
#include "prefs_keys.h"   // All NVS keys; guarded by the native suite
#include "prefs_migration_logic.h"

static const char *PREFS_NAMESPACE = "mrf";

namespace
{
const unsigned long PREFS_FLUSH_DELAY_MS = 2000;
// Backstop: flush this long after the FIRST unflushed change even if activity
// (e.g. continuous film winding) keeps re-arming the quiet-period timer above,
// bounding the amount of unsaved state a battery pull could lose.
const unsigned long PREFS_MAX_DIRTY_AGE_MS = 10000;

bool prefsDirty = false;
uint8_t prefsDirtyMask = 0;
unsigned long prefsLastDirtyMs = 0;
unsigned long prefsFirstDirtyMs = 0;

// Lens-ADC moving-average state. The algorithm (with first-sample priming)
// lives in lens_spike_logic so the native tests cover it; only the instance
// lives here.
LensMovingAverageState lensMovingAvg;

void getLensReadingsKey(size_t lensIndex, char *buffer, size_t bufferSize)
{
  snprintf(buffer, bufferSize, PREFS_KEY_PATTERN_LENS_READINGS, static_cast<unsigned int>(lensIndex));
}

void getLensCalibratedKey(size_t lensIndex, char *buffer, size_t bufferSize)
{
  snprintf(buffer, bufferSize, PREFS_KEY_PATTERN_LENS_CALIBRATED, static_cast<unsigned int>(lensIndex));
}

void writeLensCalibrationPrefs()
{
  prefs.putUInt(PREFS_KEY_LENS_COUNT, static_cast<uint32_t>(NUM_LENSES));

  for (size_t lensIndex = 0; lensIndex < NUM_LENSES; lensIndex++)
  {
    char readingsKey[16] = {0};
    char calibratedKey[16] = {0};
    getLensReadingsKey(lensIndex, readingsKey, sizeof(readingsKey));
    getLensCalibratedKey(lensIndex, calibratedKey, sizeof(calibratedKey));

    prefs.putBytes(readingsKey, lenses[lensIndex].sensor_reading, sizeof(lenses[lensIndex].sensor_reading));
    prefs.putBool(calibratedKey, lenses[lensIndex].calibrated);
  }
}

void writeSettingsPrefs()
{
  prefs.putInt(PREFS_KEY_ISO, iso);
  prefs.putInt(PREFS_KEY_ISO_INDEX, iso_index);
  prefs.putFloat(PREFS_KEY_APERTURE, aperture);
  prefs.putInt(PREFS_KEY_APERTURE_INDEX, aperture_index);
  prefs.putInt(PREFS_KEY_SELECTED_FORMAT, selected_format);
  prefs.putInt(PREFS_KEY_SELECTED_LENS, selected_lens);
  prefs.putBool(PREFS_KEY_PARALLAX, parallaxEnabled);
  prefs.putInt(PREFS_KEY_LENS_FOCUS_OFFSET, lens_focus_offset);
  prefs.putInt(PREFS_KEY_EV_COMP_THIRDS, exposure_comp_thirds);
  prefs.putInt(PREFS_KEY_METER_SMOOTHING, meter_smoothing_mode);
  prefs.putBool(PREFS_KEY_SHOW_EV, show_ev_readout);
  prefs.putInt(PREFS_KEY_SLEEP_TIMEOUT_MODE, sleep_timeout_mode);
  prefs.putInt(PREFS_KEY_LIDAR_IDLE_TIMEOUT, lidar_idle_timeout_mode);
  prefs.putInt(PREFS_KEY_LEVEL_TRIM_L10, level_trim_landscape_deci_deg);
  prefs.putInt(PREFS_KEY_LEVEL_TRIM_PP10, level_trim_portrait_pos_deci_deg);
  prefs.putInt(PREFS_KEY_LEVEL_TRIM_PN10, level_trim_portrait_neg_deci_deg);
  prefs.putInt(PREFS_KEY_RETICLE_X, reticle_offset_x);
  prefs.putInt(PREFS_KEY_RETICLE_Y, reticle_offset_y);
  prefs.putInt(PREFS_KEY_LIDAR_OFFSET, lidar_distance_offset_mm);
  prefs.putBool(PREFS_KEY_BRIGHTNESS_AUTO, brightness_auto);
  prefs.putInt(PREFS_KEY_BRIGHTNESS_MANUAL_PCT, brightness_manual_pct);
  prefs.putInt(PREFS_KEY_BRIGHTNESS_TOP_PCT, brightness_auto_top_pct);
  prefs.putBool(PREFS_KEY_SHOW_HORIZON, show_horizon_line);
}

void writeFilmPrefs()
{
  prefs.putInt(PREFS_KEY_FILM_COUNTER, film_counter);
  prefs.putInt(PREFS_KEY_ENCODER_VALUE, encoder_value);
  // NVS keys are capped at 15 chars; "prev_encoder_value" (18) silently failed
  // every write and always read back the 0 default.
  prefs.putInt(PREFS_KEY_PREV_ENCODER_VALUE, prev_encoder_value);
  prefs.putInt(PREFS_KEY_FRAME_ONE_OFFSET, frame_one_offset);
  prefs.putInt(PREFS_KEY_FRAME_SPACING, frame_spacing_offset);
}

void writePrefsToOpenNamespace(uint8_t dirtyMask)
{
  if ((dirtyMask & PREFS_DIRTY_SETTINGS) != 0)
  {
    writeSettingsPrefs();
  }
  if ((dirtyMask & PREFS_DIRTY_FILM) != 0)
  {
    writeFilmPrefs();
  }
  if ((dirtyMask & PREFS_DIRTY_LENS_CAL) != 0)
  {
    writeLensCalibrationPrefs();
  }
  // Write the schema marker last. Each Preferences::put* commits individually
  // to NVS, so during a legacy migration (PREFS_DIRTY_ALL) a power loss after
  // an earlier schema-first write but before the lens blobs landed would leave
  // the next boot seeing a current schema with no lens data - the legacy blob
  // is never consulted again. Writing schema last means any interruption
  // before this point still reads as the old schema, so migration retries.
  prefs.putUShort(PREFS_KEY_SCHEMA, PREFS_SCHEMA_VERSION);
}

void writePrefsNow(uint8_t dirtyMask)
{
  if (dirtyMask == 0)
  {
    return;
  }

  prefs.begin(PREFS_NAMESPACE, false);
  writePrefsToOpenNamespace(dirtyMask);
  prefs.end();
}

void markPrefsClean()
{
  prefsDirty = false;
  prefsDirtyMask = 0;
  prefsSchemaVersionLoaded = PREFS_SCHEMA_VERSION;
  prefsSchemaValid = true;
  prefsLoadedLegacy = false;
}

void clampLoadedState()
{
  // The rules live in prefs_clamp_logic so the native suite can drive them
  // with corrupted inputs; this wrapper just moves globals in and out.
  LoadedPrefsState s = {};
  s.iso_index = iso_index;
  s.iso = iso;
  s.selected_lens = selected_lens;
  s.selected_format = selected_format;
  s.aperture_index = aperture_index;
  s.aperture = aperture;
  s.film_counter = film_counter;
  s.encoder_value = encoder_value;
  s.prev_encoder_value = prev_encoder_value;
  s.exposure_comp_thirds = exposure_comp_thirds;
  s.meter_smoothing_mode = meter_smoothing_mode;
  s.sleep_timeout_mode = sleep_timeout_mode;
  s.lidar_idle_timeout_mode = lidar_idle_timeout_mode;
  s.level_trim_landscape_deci_deg = level_trim_landscape_deci_deg;
  s.level_trim_portrait_pos_deci_deg = level_trim_portrait_pos_deci_deg;
  s.level_trim_portrait_neg_deci_deg = level_trim_portrait_neg_deci_deg;
  s.reticle_offset_x = reticle_offset_x;
  s.reticle_offset_y = reticle_offset_y;
  s.brightness_manual_pct = brightness_manual_pct;
  s.brightness_auto_top_pct = brightness_auto_top_pct;
  s.frame_one_offset = frame_one_offset;
  s.frame_spacing_offset = frame_spacing_offset;
  s.lens_focus_offset = lens_focus_offset;

  clampLoadedPrefsState(s);

  iso_index = s.iso_index;
  iso = s.iso;
  selected_lens = s.selected_lens;
  selected_format = s.selected_format;
  aperture_index = s.aperture_index;
  aperture = s.aperture;
  film_counter = s.film_counter;
  encoder_value = s.encoder_value;
  prev_encoder_value = s.prev_encoder_value;
  exposure_comp_thirds = s.exposure_comp_thirds;
  meter_smoothing_mode = s.meter_smoothing_mode;
  sleep_timeout_mode = s.sleep_timeout_mode;
  lidar_idle_timeout_mode = s.lidar_idle_timeout_mode;
  level_trim_landscape_deci_deg = s.level_trim_landscape_deci_deg;
  level_trim_portrait_pos_deci_deg = s.level_trim_portrait_pos_deci_deg;
  level_trim_portrait_neg_deci_deg = s.level_trim_portrait_neg_deci_deg;
  reticle_offset_x = s.reticle_offset_x;
  reticle_offset_y = s.reticle_offset_y;
  brightness_manual_pct = s.brightness_manual_pct;
  brightness_auto_top_pct = s.brightness_auto_top_pct;
  frame_one_offset = s.frame_one_offset;
  frame_spacing_offset = s.frame_spacing_offset;
  lens_focus_offset = s.lens_focus_offset;
}

void loadLensCalibrationSchemaV2()
{
  size_t storedLensCount = static_cast<size_t>(prefs.getUInt(PREFS_KEY_LENS_COUNT, 0));
  size_t loadCount = (storedLensCount < NUM_LENSES) ? storedLensCount : NUM_LENSES;

  for (size_t lensIndex = 0; lensIndex < loadCount; lensIndex++)
  {
    char readingsKey[16] = {0};
    char calibratedKey[16] = {0};
    getLensReadingsKey(lensIndex, readingsKey, sizeof(readingsKey));
    getLensCalibratedKey(lensIndex, calibratedKey, sizeof(calibratedKey));

    // Require an exact size match rather than clamping to the smaller of the
    // two. ESP32 Preferences::getBytes() refuses (returns 0, copies nothing)
    // when the destination is smaller than the stored blob, and a stored blob
    // shorter than expected would otherwise leave the tail zero-filled — both
    // cases would silently corrupt lenses[lensIndex].sensor_reading while
    // `calibrated` stays true below. Any shape mismatch keeps the compiled-in
    // table instead.
    size_t storedReadingBytes = prefs.getBytesLength(readingsKey);
    int loadedReadings[LENS_DISTANCE_POINT_COUNT] = {};
    if (storedReadingBytes == sizeof(loadedReadings) &&
        prefs.getBytes(readingsKey, loadedReadings, sizeof(loadedReadings)) == sizeof(loadedReadings))
    {
      memcpy(lenses[lensIndex].sensor_reading, loadedReadings, sizeof(lenses[lensIndex].sensor_reading));
    }

    lenses[lensIndex].calibrated = prefs.getBool(calibratedKey, lenses[lensIndex].calibrated);
  }
}

bool migrateLegacyLensPrefs()
{
  const size_t legacyBytes = prefs.getBytesLength(PREFS_KEY_LEGACY_LENSES);
  const size_t expectedBytes = expectedLegacyLensBlobSize(NUM_LENSES);
  if (legacyBytes == 0 || legacyBytes != expectedBytes)
  {
    return false;
  }

  uint8_t *buffer = static_cast<uint8_t *>(malloc(legacyBytes));
  if (!buffer)
  {
    return false;
  }

  size_t bytesRead = prefs.getBytes(PREFS_KEY_LEGACY_LENSES, buffer, legacyBytes);
  if (bytesRead != legacyBytes)
  {
    free(buffer);
    return false;
  }
  bool migrated = applyLegacyLensBlob(buffer, legacyBytes, lenses, NUM_LENSES);
  free(buffer);
  return migrated;
}
} // namespace

// Helper functions
// ---------------------

void performFactoryReset()
{
  prefs.begin(PREFS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
  ESP.restart();
}

int getFirstNonZeroAperture()
{
  const int aperture_count = LENS_APERTURE_COUNT;
  for (int i = 0; i < aperture_count; i++)
  {
    if (lenses[selected_lens].apertures[i] != 0)
    {
      return i;
    }
  }
  return -1;
}

void loadPrefs()
{
  prefs.begin(PREFS_NAMESPACE, false);

  iso_index = prefs.getInt(PREFS_KEY_ISO_INDEX, DEFAULT_ISO_INDEX);
  iso = prefs.getInt(PREFS_KEY_ISO, DEFAULT_ISO);
  aperture_index = prefs.getInt(PREFS_KEY_APERTURE_INDEX, 0);
  aperture = prefs.getFloat(PREFS_KEY_APERTURE, 0.0f);
  lens_focus_offset = prefs.getInt(PREFS_KEY_LENS_FOCUS_OFFSET, DEFAULT_LENS_FOCUS_OFFSET);
  selected_lens = prefs.getInt(PREFS_KEY_SELECTED_LENS, DEFAULT_SELECTED_LENS);
  selected_format = prefs.getInt(PREFS_KEY_SELECTED_FORMAT, DEFAULT_SELECTED_FORMAT);
  parallaxEnabled = prefs.getBool(PREFS_KEY_PARALLAX, true);
  exposure_comp_thirds = prefs.getInt(PREFS_KEY_EV_COMP_THIRDS, DEFAULT_EXPOSURE_COMP_THIRDS);
  meter_smoothing_mode = prefs.getInt(PREFS_KEY_METER_SMOOTHING, DEFAULT_METER_SMOOTHING_MODE);
  show_ev_readout = prefs.getBool(PREFS_KEY_SHOW_EV, DEFAULT_SHOW_EV_READOUT);
  sleep_timeout_mode = prefs.getInt(PREFS_KEY_SLEEP_TIMEOUT_MODE, DEFAULT_SLEEP_TIMEOUT_MODE);
  lidar_idle_timeout_mode = prefs.getInt(PREFS_KEY_LIDAR_IDLE_TIMEOUT, DEFAULT_LIDAR_IDLE_TIMEOUT_MODE);
  int legacy_trim_l = prefs.getInt(PREFS_KEY_LEGACY_LEVEL_TRIM_L, DEFAULT_LEVEL_TRIM_LANDSCAPE_DECI_DEG / 10);
  int legacy_trim_pp = prefs.getInt(PREFS_KEY_LEGACY_LEVEL_TRIM_PP, DEFAULT_LEVEL_TRIM_PORTRAIT_POS_DECI_DEG / 10);
  int legacy_trim_pn = prefs.getInt(PREFS_KEY_LEGACY_LEVEL_TRIM_PN, DEFAULT_LEVEL_TRIM_PORTRAIT_NEG_DECI_DEG / 10);
  level_trim_landscape_deci_deg = prefs.getInt(PREFS_KEY_LEVEL_TRIM_L10, legacy_trim_l * 10);
  level_trim_portrait_pos_deci_deg = prefs.getInt(PREFS_KEY_LEVEL_TRIM_PP10, legacy_trim_pp * 10);
  level_trim_portrait_neg_deci_deg = prefs.getInt(PREFS_KEY_LEVEL_TRIM_PN10, legacy_trim_pn * 10);
  reticle_offset_x = prefs.getInt(PREFS_KEY_RETICLE_X, DEFAULT_RETICLE_OFFSET_X);
  reticle_offset_y = prefs.getInt(PREFS_KEY_RETICLE_Y, DEFAULT_RETICLE_OFFSET_Y);
  lidar_distance_offset_mm = prefs.getInt(PREFS_KEY_LIDAR_OFFSET, DEFAULT_LIDAR_DISTANCE_OFFSET_MM);
  lidar_distance_offset_mm = constrain(lidar_distance_offset_mm, LIDAR_DISTANCE_OFFSET_MIN_MM, LIDAR_DISTANCE_OFFSET_MAX_MM);
  brightness_auto = prefs.getBool(PREFS_KEY_BRIGHTNESS_AUTO, DEFAULT_BRIGHTNESS_AUTO);
  brightness_manual_pct = prefs.getInt(PREFS_KEY_BRIGHTNESS_MANUAL_PCT, DEFAULT_BRIGHTNESS_MANUAL_PCT);
  brightness_auto_top_pct = prefs.getInt(PREFS_KEY_BRIGHTNESS_TOP_PCT, DEFAULT_BRIGHTNESS_AUTO_TOP_PCT);
  show_horizon_line = prefs.getBool(PREFS_KEY_SHOW_HORIZON, DEFAULT_SHOW_HORIZON_LINE);
  film_counter = prefs.getInt(PREFS_KEY_FILM_COUNTER, 0);
  encoder_value = prefs.getInt(PREFS_KEY_ENCODER_VALUE, 0);
  prev_encoder_value = prefs.getInt(PREFS_KEY_PREV_ENCODER_VALUE, 0);
  frame_one_offset = prefs.getInt(PREFS_KEY_FRAME_ONE_OFFSET, DEFAULT_FRAME_ONE_OFFSET);
  frame_spacing_offset = prefs.getInt(PREFS_KEY_FRAME_SPACING, DEFAULT_FRAME_SPACING_OFFSET);

  uint16_t schemaVersion = prefs.getUShort(PREFS_KEY_SCHEMA, 0);
  size_t legacyBytes = prefs.getBytesLength(PREFS_KEY_LEGACY_LENSES);
  PrefsLoadMode loadMode = selectPrefsLoadMode(
      schemaVersion,
      PREFS_SCHEMA_VERSION,
      legacyBytes,
      expectedLegacyLensBlobSize(NUM_LENSES));

  prefsSchemaVersionLoaded = schemaVersion;
  prefsSchemaValid = (loadMode == PrefsLoadMode::LOAD_SCHEMA && schemaVersion == PREFS_SCHEMA_VERSION);
  prefsLoadedLegacy = false;

  bool migratedLegacy = false;
  if (loadMode == PrefsLoadMode::LOAD_SCHEMA)
  {
    loadLensCalibrationSchemaV2();
  }
  else if (loadMode == PrefsLoadMode::MIGRATE_LEGACY)
  {
    migratedLegacy = migrateLegacyLensPrefs();
  }

  clampLoadedState();

  if (migratedLegacy)
  {
    writePrefsToOpenNamespace(PREFS_DIRTY_ALL);
    prefs.remove(PREFS_KEY_LEGACY_LENSES);
    prefsSchemaVersionLoaded = PREFS_SCHEMA_VERSION;
    prefsSchemaValid = true;
    prefsLoadedLegacy = true;
  }

  prefs.end();
  prefsDirty = false;
  prefsDirtyMask = 0;
}

void savePrefs(bool force, uint8_t dirtyMask)
{
  if (dirtyMask == 0)
  {
    return;
  }

  if (!prefsDirty)
  {
    prefsFirstDirtyMs = millis();
  }
  prefsDirty = true;
  prefsDirtyMask |= dirtyMask;
  prefsLastDirtyMs = millis();

  if (force)
  {
    writePrefsNow(prefsDirtyMask);
    markPrefsClean();
  }
}

void flushPrefsIfDirty()
{
  if (!prefsDirty)
  {
    return;
  }

  unsigned long now = millis();
  bool quietPeriodElapsed = (now - prefsLastDirtyMs) >= PREFS_FLUSH_DELAY_MS;
  bool maxAgeElapsed = (now - prefsFirstDirtyMs) >= PREFS_MAX_DIRTY_AGE_MS;
  if (!quietPeriodElapsed && !maxAgeElapsed)
  {
    return;
  }

  writePrefsNow(prefsDirtyMask);
  markPrefsClean();
}

int calcMovingAvg(int sensorVal)
{
  return updateLensMovingAverage(lensMovingAvg, sensorVal);
}

void resetLensMovingAverageState()
{
  resetLensMovingAverageState(lensMovingAvg);
}

int_fast16_t getFocusRadius()
{
  // Compare distances in 5 cm steps to reduce focus ring granularity
  // without snapping too early.
  int lidar_5cm = (distance + 2) / 5;
  int lens_5cm  = (lens_distance_raw + 2) / 5;
  return constrain(abs(lidar_5cm - lens_5cm), FOCUS_RADIUS_MIN, FOCUS_RADIUS_MAX);
}
// ---------------------
