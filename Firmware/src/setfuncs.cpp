#include "setfuncs.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "activity.h"
#include "cyclefuncs.h"
#include "film_counter_logic.h"
#include "formatting_logic.h"
#include "globals.h"
#include "hardware.h"
#include "helpers.h"
#include "lens_logic.h"
#include "lens_spike_logic.h"
#include "lenses.h"
#include "lidar_logic.h"
#include "lidar_recovery_logic.h"
#include "lightmeter_logic.h"
#include "mrfconstants.h"
#include "formats.h"

namespace
{
struct LensSnapState
{
  int prevIndex = -1;
  int prevLens = -1;
};

struct LightMeterSmoothingState
{
  bool initialized = false;
  float smoothedLux = 0.0f;
};

LensSpikeFilterState lensSpikeFilter;
LensSnapState lensSnap;
LightMeterSmoothingState lightMeterSmoothing;

// LiDAR runtime tracking state. The three fields are independent — they
// don't share an invariant — but they are all touched by setDistance() in
// response to a single sensor frame, so keeping them in one struct makes
// the "this is LiDAR's runtime state" concept easy to grep and gives a
// single place to add a future related counter.
struct LidarRuntimeState
{
  LidarRecoveryState recovery = {};    // error / timeout backoff for sensor recovery attempts
  PlausibilityHoldState plausibilityHold = {}; // overshoot-hold tracking (re-aim vs beam-miss)
  int stableStreakFrames = 0;          // count of consecutive readings within stability delta
};
LidarRuntimeState lidarRuntime;

// Per-frame caches consulted only by this module's sensor pipeline.
// Previously declared extern in globals.h; kept file-scope so the global
// state surface stays focused on values shared across modules.
int prev_distance = 0;             // Last accepted LiDAR distance for blend/stability checks.
int prev_lens_sensor_reading = 0;  // Last lens ADC reading routed into setLensDistance().

bool getLensPriorCm(int &lens_prior_cm)
{
  if (!lenses[selected_lens].calibrated)
  {
    return false;
  }

  if (lens_distance_raw <= 0 || lens_distance_raw == LENS_INFINITY_RAW)
  {
    return false;
  }

  lens_prior_cm = lens_distance_raw;
  return true;
}

void setLensDistanceFromCm(int distance_cm)
{
  lens_distance_raw = distance_cm;
  cmToReadable(lens_distance_raw, DISTANCE_DECIMAL_PLACES, lens_distance_cm, sizeof(lens_distance_cm));
}

} // namespace

void applyLidarCalibrationProfile()
{
  // Re-apply library-side distance correction after sensor state changes.
  // Frame rate is set once at boot in initializeLidarSensor(); the sensor retains
  // it across enable/disable cycles. Re-sending setFrameRate immediately after
  // enableSensor() in the recovery path destabilises some DTS6012M units and
  // causes a self-perpetuating recovery loop (see v10.4.7 changelog).
  lidar.setDistanceScale(LIDAR_LIBRARY_DISTANCE_SCALE);
  lidar.setDistanceOffset(static_cast<int16_t>(lidar_distance_offset_mm));
}

void clearLidarDisplay(const char *placeholder)
{
  snprintf(distance_cm, sizeof(distance_cm), "%s", placeholder);
  lidar_quality_level = 0;
  lidar_distance_held = false; // Placeholder shown — nothing is being held.
  prev_distance = 0; // Reset so the next valid reading is not penalised against a stale value.
}

// Functions to read values from sensors and set variables
// ---------------------
void setDistance()
{
  if (!hardware.lidarSensor || !lidarEnabled)
  {
    lidarRuntime.recovery = {};
    lidar_high_sunlight = false;
    lidar_distance_held = false;
    return;
  }

  const unsigned long now = millis();

  // Measured frame rate (bd n06): the sensor's getFrameRate() self-report reads 0
  // on this hardware, so count accepted frames over a rolling ~1s window instead.
  // This is the only trustworthy confirmation that setFrameRate() actually changed
  // the delivery rate. Valid only while the main loop polls faster than the rate.
  static uint32_t fps_window_start = 0;
  static uint16_t fps_frame_count = 0;
  if (fps_window_start == 0)
  {
    fps_window_start = now;
  }
  if (now - fps_window_start >= 1000UL)
  {
    lidar_frame_rate_measured = fps_frame_count;
    fps_frame_count = 0;
    fps_window_start = now;
  }

  DTSResult lidarUpdateResult = lidar.update();
  // NO_NEW_DATA is the benign "no frame this poll" case; don't let it overwrite the
  // last meaningful status shown on the diagnostics screen (so err: reads 0 while
  // frames flow, and a real code such as TIMEOUT/CRC when something is actually wrong).
  if (static_cast<DTSError>(lidarUpdateResult) != DTSError::NO_NEW_DATA)
  {
    last_lidar_error_code = static_cast<int>(static_cast<DTSError>(lidarUpdateResult));
  }
  if (lidar.newDataAvailable())
  {
    fps_frame_count++;
    DTSMeasurement measurement = lidar.getMeasurement();
    lidar_high_sunlight = updateSunlightWarnState(lidar_high_sunlight, measurement.sunlightBase);

    // Live telemetry for the diagnostics screen — the latest raw sensor frame,
    // captured before the median filter overwrites the primary distance below.
    lidar_raw_distance_mm = measurement.primaryDistance_mm;
    lidar_primary_intensity = measurement.primaryIntensity;
    lidar_sunlight_base = measurement.sunlightBase;
    lidar_snr_permille = computeSnrPermille(measurement.primaryIntensity, measurement.sunlightBase);
    lidar_telemetry_ms = now;

    // Use the library's median-filtered distance to reduce jitter at near/mid range.
    uint16_t filtered_mm = lidar.getFilteredDistance();
    if (filtered_mm != DTS_INVALID_DISTANCE && measurement.primaryDistance_mm != DTS_INVALID_DISTANCE)
    {
      measurement.primaryDistance_mm = filtered_mm;
    }

    int lens_prior_cm = 0;
    bool has_lens_prior = getLensPriorCm(lens_prior_cm);

    LidarCandidate chosen = chooseBestLidarCandidate(measurement, prev_distance, has_lens_prior, lens_prior_cm);
    if (!chosen.valid)
    {
      // Sensor frame unusable — break any subject-stable streak.
      lidarRuntime.stableStreakFrames = 0;
      // Keep main-branch behavior: do not force recovery on filtered/noisy frames.
      if ((now - lidarRuntime.recovery.last_valid_measurement_ms) > LIDAR_NO_DATA_TIMEOUT_MS)
      {
        clearLidarDisplay(lidarSignalLossPlaceholder(prev_distance, distance_cm));
      }
      return;
    }

    // Plausibility gate: when the lens is focused within 3m, reject LiDAR
    // readings that significantly overshoot the lens prior — almost certainly a
    // beam-miss past the framed subject. Hold the previous valid value instead.
    // After a short streak of rejections, fall through so the user can
    // deliberately re-focus past the previous LiDAR target without being stuck.
    if (has_lens_prior && isLidarReadingImplausible(chosen.distance_cm, lens_prior_cm, chosen.quality_level))
    {
      lidarRuntime.stableStreakFrames = 0; // Rejection means we are not tracking a stable subject.
      bool release = updatePlausibilityHold(lidarRuntime.plausibilityHold,
                                            chosen.distance_cm,
                                            LIDAR_PLAUSIBILITY_STABLE_DELTA_CM,
                                            LIDAR_PLAUSIBILITY_STABLE_RELEASE_FRAMES,
                                            LIDAR_PLAUSIBILITY_FALLTHROUGH_FRAMES);
      lidar_distance_held = !release;
      if (!release)
      {
        return; // Hold the previous valid reading until the overshoot settles or caps out.
      }
      // Released: the user has deliberately re-aimed past the previous target.
    }
    else
    {
      resetPlausibilityHold(lidarRuntime.plausibilityHold);
      lidar_distance_held = false;
    }

    // Subject-stable confidence boost: count consecutive readings within the
    // stability delta and add a confidence boost once the streak passes the
    // minimum-frames threshold.
    if (prev_distance > 0 && abs(chosen.distance_cm - prev_distance) <= LIDAR_STABLE_DELTA_CM)
    {
      lidarRuntime.stableStreakFrames = min(lidarRuntime.stableStreakFrames + 1, LIDAR_STABLE_MIN_FRAMES + 1);
    }
    else
    {
      lidarRuntime.stableStreakFrames = 1; // start a new streak from this reading
    }
    chosen.confidence = applyStableConfidenceBoost(chosen.confidence, lidarRuntime.stableStreakFrames);

    updateLidarRecoveryState(lidarRuntime.recovery, LidarRecoveryEvent::VALID_MEASUREMENT, now);
    lidar_quality_level = chosen.quality_level;

    distance = static_cast<int16_t>(blendLidarDistance(prev_distance, chosen.distance_cm, chosen.confidence));
    if (distance != prev_distance || strcmp(distance_cm, "...") == 0 || strcmp(distance_cm, "Inf.") == 0 || strcmp(distance_cm, "Inf?") == 0 || strcmp(distance_cm, "Zzz") == 0)
    {
      formatDistanceDisplay(distance, distance_cm, sizeof(distance_cm));
      prev_distance = distance;
    }
    return;
  }

  DTSError lidarUpdateError = static_cast<DTSError>(lidarUpdateResult);
  LidarRecoveryEvent event = lidarRecoveryEventForUpdateError(lidarUpdateError);
  LidarRecoveryDecision recoveryDecision = updateLidarRecoveryState(lidarRuntime.recovery, event, now);

  if (recoveryDecision.clear_display)
  {
    // Preserve a far dropout across a stray timeout/CRC frame: while aimed at the
    // sky the sensor mostly streams invalid frames (handled above), but an odd
    // unparseable frame lands here and must not poison "Inf?" back to "...".
    clearLidarDisplay(lidarSignalLossPlaceholder(prev_distance, distance_cm));
  }

  if (!recoveryDecision.attempt_recovery)
  {
    return;
  }

  lidar_recovery_count++;
  lidar.clearError();
  DTSError resetStatus = lidar.resetState();
  DTSError enableStatus = static_cast<DTSError>(lidar.enableSensor());
  // resetState() clears the library scale + distance offset. Re-apply the
  // calibration profile unconditionally (it is idempotent) so a partial recovery
  // where enableSensor() fails transiently while the sensor keeps streaming can
  // never leave the offset zeroed — otherwise every later reading is short by the
  // configured offset (default 40 cm) until a fully successful recovery happens.
  applyLidarCalibrationProfile();
  bool recovered = (resetStatus == DTSError::NONE && enableStatus == DTSError::NONE);
  noteLidarRecoveryAttemptResult(lidarRuntime.recovery, recovered, now);
}

// Borrows moving average code from
// https://github.com/makeabilitylab/arduino/blob/master/Filters/MovingAverageFilter/MovingAverageFilter.ino
int getLensSensorReading()
{
  if (!hardware.ads)
  {
    return lensSpikeFilter.stableReading;
  }

  if (LENS_ADC_QUIET_DELAY_MS > 0)
  {
    delay(LENS_ADC_QUIET_DELAY_MS);
  }

  long adcSampleTotal = 0;
  for (int i = 0; i < LENS_ADC_SAMPLE_COUNT; i++)
  {
    adcSampleTotal += theads.readADC_SingleEnded(LENS_ADC_PIN);
    if (LENS_ADC_SAMPLE_DELAY_US > 0)
    {
      delayMicroseconds(LENS_ADC_SAMPLE_DELAY_US);
    }
  }

  int sensorVal = static_cast<int>(adcSampleTotal / LENS_ADC_SAMPLE_COUNT);
  if (ui_mode == UiMode::Main)
  {
    // Fixed Main-vs-Calib compensation plus the user's focus fine-tune. Calib
    // mode captures without either, so the stored table stays reference-clean.
    sensorVal += LENS_ADC_MAIN_OFFSET + lens_focus_offset;
  }

  int smoothedReading = calcMovingAvg(sensorVal);
  return updateLensSpikeFilter(lensSpikeFilter, smoothedReading);
}

void setLensDistance()
{
  const Lens &lens = lenses[selected_lens];

  if (selected_lens != lensSnap.prevLens)
  {
    lensSnap.prevLens = selected_lens;
    lensSnap.prevIndex = -1;
  }

  if (lens_sensor_reading == prev_lens_sensor_reading)
  {
    return;
  }

  int prevReading = prev_lens_sensor_reading;
  prev_lens_sensor_reading = lens_sensor_reading;

  bool activityDetected = abs(lens_sensor_reading - prevReading) > LENS_ACTIVITY_THRESHOLD;
  int snapIndex = -1;

  if (lens.calibrated)
  {
    snapIndex = findLensSnapIndex(lens, lens_sensor_reading);
    if (snapIndex >= 0 && snapIndex != lensSnap.prevIndex)
    {
      activityDetected = true;
    }
    lensSnap.prevIndex = snapIndex;
  }
  else
  {
    lensSnap.prevIndex = -1;
  }

  if (activityDetected)
  {
    registerActivity();
  }

  if (lens.calibrated && snapIndex >= 0)
  {
    setLensDistanceFromCm(static_cast<int>(lens.distance[snapIndex] * CM_PER_METER));
    return;
  }

  // An uncalibrated lens has no sensor->distance mapping, so estimating would
  // return a bogus "Inf." for any real reading. Show a neutral placeholder
  // instead of a believable-looking distance.
  if (!lens.calibrated)
  {
    lens_distance_raw = 0;
    snprintf(lens_distance_cm, sizeof(lens_distance_cm), "--");
    return;
  }

  LensDistanceEstimate estimate = estimateLensDistance(lens, lens_sensor_reading);
  if (!estimate.valid)
  {
    return;
  }

  if (estimate.is_infinity)
  {
    lens_distance_raw = LENS_INFINITY_RAW;
    snprintf(lens_distance_cm, sizeof(lens_distance_cm), "Inf.");
    return;
  }

  setLensDistanceFromCm(estimate.distance_cm);
}

void setFilmCounter()
{
  static EncoderFilterState encoderFilterState = {};
  static bool rawPositionInitialized = false;
  static int lastRawEncoderPosition = 0;
  const unsigned long now = millis();

  if (!hardware.encoder)
  {
    return;
  }

  if (!encoderFilterState.initialized || encoderFilterState.stable_position != prev_encoder_value)
  {
    resetEncoderFilterState(encoderFilterState, prev_encoder_value, now);
    rawPositionInitialized = false;
  }

  int encoder_position = encoder.getEncoderPosition();

  if (!rawPositionInitialized)
  {
    rawPositionInitialized = true;
    lastRawEncoderPosition = encoder_position;
  }
  else
  {
    int rawDelta = encoder_position - lastRawEncoderPosition;
    if (abs(rawDelta) >= FILM_COUNTER_ACTIVITY_MIN_DELTA)
    {
      registerActivity();
      lastRawEncoderPosition = encoder_position;
    }
  }

  EncoderFilterDecision decision =
      updateEncoderFilter(encoderFilterState, encoder_position, now, FILM_COUNTER_ALLOW_REWIND);
  if (!decision.accepted)
  {
    return;
  }

  registerActivity();

  encoder_value = decision.accepted_position;
  prev_encoder_value = encoder_value;

  FilmCounterEstimate estimate = estimateFilmCounter(
      film_formats[selected_format],
      encoder_value,
      frame_one_offset,
      frame_spacing_offset);
  if (!estimate.valid)
  {
    return;
  }

  film_counter = estimate.frame;
  frame_progress = estimate.progress;
  savePrefs(false, PREFS_DIRTY_FILM);
}

void setVoltage()
{
  if (!hardware.batteryGauge)
  {
    return;
  }

  bat_per = maxlipo.cellPercent();
  if (bat_per > BATTERY_PERCENT_MAX)
  {
    bat_per = BATTERY_PERCENT_MAX;
  }

}

void setLightMeter()
{
  if (!hardware.lightMeter)
  {
    lux = 0.0f;
    ev_readout = NAN;
    snprintf(shutter_speed, sizeof(shutter_speed), "No meter");
    return;
  }

  float rawLux = lightMeter.readLightLevel() * LIGHTMETER_LUX_CAL_SCALE;
  if (rawLux < 0.0f)
  {
    rawLux = 0.0f;
  }

  float alpha = getMeterSmoothingAlpha(meter_smoothing_mode);
  if (!lightMeterSmoothing.initialized || alpha >= 1.0f)
  {
    lightMeterSmoothing.smoothedLux = rawLux;
    lightMeterSmoothing.initialized = true;
  }
  else
  {
    lightMeterSmoothing.smoothedLux =
        (rawLux * alpha) + (lightMeterSmoothing.smoothedLux * (1.0f - alpha));
  }

  float exposureCompEv = static_cast<float>(exposure_comp_thirds) / 3.0f;
  lux = applyExposureCompensationToLux(lightMeterSmoothing.smoothedLux, exposureCompEv);
  ev_readout = calculateEV100(lightMeterSmoothing.smoothedLux);

  if (aperture == 0 && lux > 0)
  {
    cycleApertures(CycleDirection::Up);
  }

  formatShutterSpeed(lux, aperture, iso, shutter_speed, sizeof(shutter_speed));
  prev_iso = iso;
  prev_aperture = aperture;
}

void retryLidarInit()
{
  if (hardware.lidarSensor)
  {
    return;
  }

  DTSResult result = lidar.begin(LIDAR_BAUD_RATE, RXD2, TXD2);
  if (result != DTSError::NONE)
  {
    hardware.lidarSensor = false;
    lidarEnabled = false;
    last_lidar_error_code = static_cast<int>(static_cast<DTSError>(result));
    return;
  }

  hardware.lidarSensor = true;
  lidarEnabled = true;
  applyLidarCalibrationProfile();
  last_lidar_error_code = 0;
}

void toggleLidar(bool lidarStatusParam)
{
  if (!hardware.lidarSensor)
  {
    lidarEnabled = false;
    return;
  }

  if (lidarStatusParam == lidarEnabled)
  {
    return;
  }

  DTSError status = lidarStatusParam ? static_cast<DTSError>(lidar.enableSensor())
                                     : static_cast<DTSError>(lidar.disableSensor());
  if (status == DTSError::NONE)
  {
    // Match the boot and retry paths: flip lidarEnabled *first*, then re-apply
    // the calibration profile, so every caller observes the same ordering.
    lidarEnabled = lidarStatusParam;
    if (lidarStatusParam)
    {
      applyLidarCalibrationProfile();
    }
  }
}
// ---------------------
