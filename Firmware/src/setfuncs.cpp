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
#include "lidar_recovery_actions.h"
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
  uint32_t fpsWindowStartMs = 0;       // measured-frame-rate rolling window start (bd n06)
  uint16_t fpsFrameCount = 0;          // accepted frames inside the current window
};
LidarRuntimeState lidarRuntime;

// Called whenever the sensor is (re-)enabled: boot retry, idle-standby wake,
// sleep wake. The first update() after an enable is inevitably NO_NEW_DATA, and
// a recovery state still carrying the pre-standby last_valid timestamp would
// pass LIDAR_RECOVERY_TIMEOUT_MS instantly — running resetState()+enableSensor()
// back-to-back with the wake enable (the v10.4.7 no-settle hazard) and bumping
// the Health "Recoveries" counter on every wake. Restart the timeout window and
// the frame-rate measurement window from "now" instead.
void noteLidarSensorEnabled()
{
  resetLidarRecoveryState(lidarRuntime.recovery, millis());
  lidarRuntime.fpsWindowStartMs = 0;
  lidarRuntime.fpsFrameCount = 0;
}

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
  // Sequence lives in lidar_recovery_actions.h so the native suite can pin
  // the v10.4.7 invariant (no setFrameRate outside boot) against a fake.
  applyLidarCalibrationProfileTo(lidar, LIDAR_LIBRARY_DISTANCE_SCALE,
                                 static_cast<int16_t>(lidar_distance_offset_mm));
}

void clearLidarDisplay(const char *placeholder)
{
  snprintf(distance_cm, sizeof(distance_cm), "%s", placeholder);
  lidar_quality_level = 0;
  lidar_distance_held = false; // Placeholder shown — nothing is being held.
  prev_distance = 0; // Reset so the next valid reading is not penalised against a stale value.
  // Invalidate the shared measurement too. The focus-assist ring and the
  // parallax fallback consume `distance` directly; leaving the last accepted
  // value in place lets the ring collapse to "in focus" against a measurement
  // the sensor is no longer making.
  distance = 0;
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
  if (lidarRuntime.fpsWindowStartMs == 0)
  {
    lidarRuntime.fpsWindowStartMs = now;
  }
  if (now - lidarRuntime.fpsWindowStartMs >= 1000UL)
  {
    // Normalise by the actual window length: the window closes on the first
    // poll past 1000 ms (1000-1025 ms at the 25 ms cadence, longer if polling
    // stalled), so reporting the raw count over-reads by a few percent.
    uint32_t window_ms = now - lidarRuntime.fpsWindowStartMs;
    lidar_frame_rate_measured = static_cast<uint16_t>(
        (static_cast<uint32_t>(lidarRuntime.fpsFrameCount) * 1000UL + window_ms / 2) / window_ms);
    lidarRuntime.fpsFrameCount = 0;
    lidarRuntime.fpsWindowStartMs = now;
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
    lidarRuntime.fpsFrameCount++;
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

    // Near-range correction anchor compensation: the 130->100 anchor was
    // measured at the default geometry offset, so tell the logic how far the
    // configured pref has moved from it (offsets step in whole cm).
    int offset_delta_cm = (lidar_distance_offset_mm - DEFAULT_LIDAR_DISTANCE_OFFSET_MM) / 10;
    LidarCandidate chosen = chooseBestLidarCandidate(measurement, prev_distance, has_lens_prior, lens_prior_cm, offset_delta_cm);
    if (!chosen.valid)
    {
      // Sensor frame unusable — break any subject-stable streak.
      lidarRuntime.stableStreakFrames = 0;
      // The frame itself proves the link is healthy: tell the recovery state
      // machine, or interleaved no-frame polls would escalate to TIMEOUT
      // recovery and reset a working sensor about every 1.5s while aimed at
      // sky/far/hard sun (climbing the Health screen Recoveries counter).
      updateLidarRecoveryState(lidarRuntime.recovery, LidarRecoveryEvent::NO_VALID_MEASUREMENT, now);
      unsigned long since_valid = now - lidarRuntime.recovery.last_valid_measurement_ms;
      if (since_valid > LIDAR_NO_DATA_TIMEOUT_MS)
      {
        clearLidarDisplay(lidarSignalLossPlaceholder(prev_distance, distance_cm));
      }
      else if (since_valid > LIDAR_HELD_INDICATION_MS)
      {
        // Inside the grace window the previous reading stays on screen; flag
        // it as held rather than letting it present as a live measurement.
        lidar_distance_held = true;
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
        // Same link-health note as the invalid-candidate path: a plausibility
        // hold can outlast the recovery timeout while the sensor streams fine.
        updateLidarRecoveryState(lidarRuntime.recovery, LidarRecoveryEvent::NO_VALID_MEASUREMENT, now);
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

  // No frame this poll. Benign between frames; once the gap grows past the
  // held-indication threshold the on-screen reading is stale — say so.
  if ((now - lidarRuntime.recovery.last_valid_measurement_ms) > LIDAR_HELD_INDICATION_MS)
  {
    lidar_distance_held = true;
  }

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
  bool recovered = performLidarRecoveryAttempt(lidar, applyLidarCalibrationProfile);
  noteLidarRecoveryAttemptResult(lidarRuntime.recovery, recovered, now);
}

void recoverLidarAfterBlockingUi()
{
  if (!hardware.lidarSensor || !lidarEnabled)
  {
    return;
  }

  // A long blocking UI section (calibration-complete celebration) starves the
  // LiDAR UART past its RX buffer. Drain what survived and clear the overflow
  // so the next scheduled poll starts clean instead of surfacing a spurious
  // BUFFER_OVERFLOW error; restart the recovery/fps windows for the same
  // reason the enable paths do.
  lidar.update();
  lidar.clearError();
  noteLidarSensorEnabled();
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

  bool lensChanged = (selected_lens != lensSnap.prevLens);
  if (lensChanged)
  {
    lensSnap.prevLens = selected_lens;
    lensSnap.prevIndex = -1;
  }

  // A lens change swaps the whole sensor->distance table, so the distance must
  // be recomputed even when the ADC reading is unchanged. Skipping this kept
  // the previous lens's mapping (display and LiDAR plausibility prior) alive
  // until the focus ring physically moved — indefinitely when the reading
  // stayed quiet.
  if (!lensChanged && lens_sensor_reading == prev_lens_sensor_reading)
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
    setLensDistanceFromCm(static_cast<int>(lroundf(lens.distance[snapIndex] * CM_PER_METER)));
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

  // Round rather than truncate (99.9% displayed 99%), and clamp both ends:
  // the MAX17048 can briefly report small negative or >100 values at power-on
  // or with no battery, which printed as "-0%".
  bat_per = constrain(static_cast<int>(lroundf(maxlipo.cellPercent())), 0, BATTERY_PERCENT_MAX);
}

void resetLightMeterSmoothing()
{
  lightMeterSmoothing = {};
}

void setLightMeter()
{
  if (!hardware.lightMeter)
  {
    lux = 0.0f;
    ev_readout = NAN;
    snprintf(shutter_speed, sizeof(shutter_speed), "No meter");
    // Sync the pending-settings markers or getLightMeterIntervalMs sees a
    // permanent aperture/iso change and latches the fast 100ms cadence
    // forever, redrawing "No meter" at 10Hz on builds without a BH1750.
    prev_aperture = aperture;
    prev_iso = iso;
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
  noteLidarSensorEnabled();
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
      noteLidarSensorEnabled();
    }
  }
}
// ---------------------
