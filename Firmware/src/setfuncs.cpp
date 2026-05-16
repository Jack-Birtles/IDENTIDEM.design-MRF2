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
LidarRecoveryState lidarRecoveryState = {};
int consecutive_implausible_lidar_frames = 0;
int lidar_stable_streak_frames = 0;

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
  prev_distance = 0; // Reset so the next valid reading is not penalised against a stale value.
}

// Functions to read values from sensors and set variables
// ---------------------
void setDistance()
{
  if (!lidarSensorReady || !lidarEnabled)
  {
    lidarRecoveryState = {};
    lidar_high_sunlight = false;
    return;
  }

  const unsigned long now = millis();

  DTSResult lidarUpdateResult = lidar.update();
  last_lidar_error_code = static_cast<int>(static_cast<DTSError>(lidarUpdateResult));
  if (lidar.newDataAvailable())
  {
    DTSMeasurement measurement = lidar.getMeasurement();
    lidar_high_sunlight = updateSunlightWarnState(lidar_high_sunlight, measurement.sunlightBase);

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
      lidar_stable_streak_frames = 0;
      // Keep main-branch behavior: do not force recovery on filtered/noisy frames.
      if ((now - lidarRecoveryState.last_valid_measurement_ms) > LIDAR_NO_DATA_TIMEOUT_MS)
      {
        clearLidarDisplay(prev_distance >= LIDAR_FAR_SIGNAL_LOSS_CM ? "Inf." : "...");
      }
      return;
    }

    // Plausibility gate: when the lens is focused close, reject LiDAR readings
    // that significantly overshoot the lens prior — almost certainly a beam-miss
    // past the framed subject. Hold the previous valid value instead. After a
    // streak of rejections, fall through so the user can deliberately re-focus
    // past the previous LiDAR target without being stuck.
    if (has_lens_prior && isLidarReadingImplausible(chosen.distance_cm, lens_prior_cm))
    {
      consecutive_implausible_lidar_frames++;
      lidar_stable_streak_frames = 0; // Rejection means we are not tracking a stable subject.
      if (consecutive_implausible_lidar_frames < LIDAR_PLAUSIBILITY_FALLTHROUGH_FRAMES)
      {
        return;
      }
      // Fall through: streak exceeded, accept the reading.
    }
    else
    {
      consecutive_implausible_lidar_frames = 0;
    }

    // Subject-stable confidence boost: count consecutive readings within the
    // stability delta and add a confidence boost once the streak passes the
    // minimum-frames threshold.
    if (prev_distance > 0 && abs(chosen.distance_cm - prev_distance) <= LIDAR_STABLE_DELTA_CM)
    {
      lidar_stable_streak_frames = min(lidar_stable_streak_frames + 1, LIDAR_STABLE_MIN_FRAMES + 1);
    }
    else
    {
      lidar_stable_streak_frames = 1; // start a new streak from this reading
    }
    chosen.confidence = applyStableConfidenceBoost(chosen.confidence, lidar_stable_streak_frames);

    updateLidarRecoveryState(lidarRecoveryState, LidarRecoveryEvent::VALID_MEASUREMENT, now);
    lidar_quality_level = chosen.quality_level;

    distance = static_cast<int16_t>(blendLidarDistance(prev_distance, chosen.distance_cm, chosen.confidence));
    if (distance != prev_distance || strcmp(distance_cm, "...") == 0 || strcmp(distance_cm, "Inf.") == 0 || strcmp(distance_cm, "Zzz") == 0)
    {
      formatDistanceDisplay(distance, distance_cm, sizeof(distance_cm));
      prev_distance = distance;
    }
    return;
  }

  DTSError lidarUpdateError = static_cast<DTSError>(lidarUpdateResult);
  LidarRecoveryEvent event = (lidarUpdateError == DTSError::TIMEOUT)
                                 ? LidarRecoveryEvent::TIMEOUT
                                 : LidarRecoveryEvent::ERROR;
  LidarRecoveryDecision recoveryDecision = updateLidarRecoveryState(lidarRecoveryState, event, now);

  if (recoveryDecision.clear_display)
  {
    clearLidarDisplay("...");
  }

  if (!recoveryDecision.attempt_recovery)
  {
    return;
  }

  lidar_recovery_count++;
  lidar.clearError();
  DTSError resetStatus = lidar.resetState();
  DTSError enableStatus = static_cast<DTSError>(lidar.enableSensor());
  bool recovered = (resetStatus == DTSError::NONE && enableStatus == DTSError::NONE);
  if (recovered)
  {
    applyLidarCalibrationProfile();
  }
  noteLidarRecoveryAttemptResult(lidarRecoveryState, recovered, now);
}

// Borrows moving average code from
// https://github.com/makeabilitylab/arduino/blob/master/Filters/MovingAverageFilter/MovingAverageFilter.ino
int getLensSensorReading()
{
  if (!adsReady)
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
    sensorVal += LENS_ADC_MAIN_OFFSET;
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

  if (!encoderReady)
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
  if (!batteryGaugeReady)
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
  if (!lightMeterReady)
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
  if (lidarSensorReady)
  {
    return;
  }

  DTSResult result = lidar.begin(LIDAR_BAUD_RATE, RXD2, TXD2);
  if (result != DTSError::NONE)
  {
    lidarSensorReady = false;
    lidarEnabled = false;
    last_lidar_error_code = static_cast<int>(static_cast<DTSError>(result));
    return;
  }

  lidarSensorReady = true;
  lidarEnabled = true;
  applyLidarCalibrationProfile();
  last_lidar_error_code = 0;
}

void toggleLidar(bool lidarStatusParam)
{
  if (!lidarSensorReady)
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
