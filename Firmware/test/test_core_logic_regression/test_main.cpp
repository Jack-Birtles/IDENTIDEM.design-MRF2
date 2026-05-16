#include <string>
#include <cstring>

#include <unity.h>

#include "film_counter_logic.h"
#include "formats.h"
#include "calibration_logic.h"
#include "lens_logic.h"
#include "lidar_recovery_logic.h"
#include "lidar_logic.h"
#include "lightmeter_logic.h"
#include "mrfconstants.h"
#include "prefs_migration_logic.h"
#include "ui_signature_logic.h"

// Limit the test scope to the core logic modules only.
#include "../../src/calibration_logic.cpp"
#include "../../src/film_counter_logic.cpp"
#include "../../src/formats.cpp"
#include "../../src/lens_logic.cpp"
#include "../../src/lidar_recovery_logic.cpp"
#include "../../src/lenses.cpp"
#include "../../src/lidar_logic.cpp"
#include "../../src/lightmeter_logic.cpp"
#include "../../src/prefs_migration_logic.cpp"
#include "../../src/ui_signature_logic.cpp"

namespace
{
Lens makeTestLens()
{
  Lens lens = {
      999,
      "TEST",
      90.0f,
      {100, 200, 300, 400, 500, 600, 700},
      {1.0f, 1.2f, 1.5f, 2.0f, 3.0f, 5.0f, 10.0f},
      {0.0f, 2.8f, 4.0f, 5.6f, 8.0f, 11.0f, 16.0f, 22.0f, 32.0f},
      {0, 0, 0, 0},
      true};
  return lens;
}

const FilmFormat *findFormatByName(const char *formatName)
{
  for (size_t i = 0; i < NUM_FILM_FORMATS; i++)
  {
    if (strcmp(film_formats[i].name, formatName) == 0)
    {
      return &film_formats[i];
    }
  }
  return nullptr;
}

// A primary-only return with no secondary peak. Mirrors the most common
// DTS6012M measurement shape in the field. sunlightBase stays 0; tests that
// need a specific ambient level set it after the helper returns.
DTSMeasurement makeLidarMeasurement(uint16_t primaryDistance_mm,
                                    uint16_t primaryIntensity,
                                    DataQuality primaryQuality)
{
  DTSMeasurement m = {};
  m.primaryDistance_mm = primaryDistance_mm;
  m.primaryIntensity = primaryIntensity;
  m.primaryQuality = primaryQuality;
  m.secondaryDistance_mm = DTS_INVALID_DISTANCE;
  m.secondaryIntensity = 0;
  m.secondaryQuality = DataQuality::INVALID;
  return m;
}

DTSMeasurement makeLidarDualPeakMeasurement(uint16_t primaryDistance_mm,
                                            uint16_t primaryIntensity,
                                            DataQuality primaryQuality,
                                            uint16_t secondaryDistance_mm,
                                            uint16_t secondaryIntensity,
                                            DataQuality secondaryQuality)
{
  DTSMeasurement m = makeLidarMeasurement(primaryDistance_mm, primaryIntensity, primaryQuality);
  m.secondaryDistance_mm = secondaryDistance_mm;
  m.secondaryIntensity = secondaryIntensity;
  m.secondaryQuality = secondaryQuality;
  return m;
}
} // namespace

void setUp() {}
void tearDown() {}

void test_frame_counter_exact_and_interpolation()
{
  const FilmFormat *format = findFormatByName("6x7");
  TEST_ASSERT_NOT_NULL(format);

  FilmCounterEstimate exact = estimateFilmCounter(*format, 140);
  TEST_ASSERT_TRUE(exact.valid);
  TEST_ASSERT_EQUAL_INT(1, exact.frame);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, exact.progress);

  FilmCounterEstimate interpolated = estimateFilmCounter(*format, 150);
  TEST_ASSERT_TRUE(interpolated.valid);
  TEST_ASSERT_EQUAL_INT(1, interpolated.frame);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f / 34.0f, interpolated.progress);
}

void test_frame_counter_snap_and_roll_end()
{
  const FilmFormat *format = findFormatByName("6x7");
  TEST_ASSERT_NOT_NULL(format);

  FilmCounterEstimate snapped = estimateFilmCounter(*format, 173);
  TEST_ASSERT_TRUE(snapped.valid);
  TEST_ASSERT_EQUAL_INT(2, snapped.frame);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, snapped.progress);

  FilmCounterEstimate end = estimateFilmCounter(*format, 600);
  TEST_ASSERT_TRUE(end.valid);
  TEST_ASSERT_EQUAL_INT(FILM_COUNTER_END, end.frame);
}

void test_frame_counter_frame_offset_and_spacing()
{
  const FilmFormat *format = findFormatByName("6x7");
  TEST_ASSERT_NOT_NULL(format);

  FilmCounterEstimate shiftedStart = estimateFilmCounter(*format, 145, 5, 0);
  TEST_ASSERT_TRUE(shiftedStart.valid);
  TEST_ASSERT_EQUAL_INT(1, shiftedStart.frame);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, shiftedStart.progress);

  FilmCounterEstimate beforeShiftedStart = estimateFilmCounter(*format, 140, 5, 0);
  TEST_ASSERT_TRUE(beforeShiftedStart.valid);
  TEST_ASSERT_EQUAL_INT(0, beforeShiftedStart.frame);

  FilmCounterEstimate widerSpacingFrame2 = estimateFilmCounter(*format, 176, 0, 2);
  TEST_ASSERT_TRUE(widerSpacingFrame2.valid);
  TEST_ASSERT_EQUAL_INT(2, widerSpacingFrame2.frame);

  FilmCounterEstimate beforeWiderSpacingFrame2 = estimateFilmCounter(*format, 174, 0, 2);
  TEST_ASSERT_TRUE(beforeWiderSpacingFrame2.valid);
  TEST_ASSERT_EQUAL_INT(1, beforeWiderSpacingFrame2.frame);
}

void test_format_3x6_supports_21_frames()
{
  const FilmFormat *format = findFormatByName("3x6");
  TEST_ASSERT_NOT_NULL(format);
  TEST_ASSERT_EQUAL_INT(21, getFilmFormatMaxFrame(*format));
  TEST_ASSERT_EQUAL_INT(23, getFilmFormatPointCount(*format));
  TEST_ASSERT_EQUAL_INT(99, format->frame[getFilmFormatPointCount(*format) - 1]);

  // 3x6 is a 120-film format. Catch regressions to the v10.4.7-and-earlier bug
  // where its sensor[] table was a copy of PANO (35mm film) with one entry
  // appended — leading delta ~37, end sentinel 800 — causing every-other-frame
  // exposure on 120. Real 120 formats have a leader-paper offset of ~130+.
  TEST_ASSERT_GREATER_OR_EQUAL_INT(120, format->sensor[1]);
  TEST_ASSERT_EQUAL_INT(550, format->sensor[getFilmFormatPointCount(*format) - 1]);
}

void test_encoder_filter_forward_hysteresis_and_debounce()
{
  EncoderFilterState state = {};
  resetEncoderFilterState(state, 100, 0);

  EncoderFilterDecision jitter = updateEncoderFilter(state, 101, 10, false);
  TEST_ASSERT_FALSE(jitter.accepted);

  EncoderFilterDecision beginMove = updateEncoderFilter(state, 103, 20, false);
  TEST_ASSERT_FALSE(beginMove.accepted);

  EncoderFilterDecision bounceBack = updateEncoderFilter(state, 101, 30, false);
  TEST_ASSERT_FALSE(bounceBack.accepted);

  EncoderFilterDecision rearmMove = updateEncoderFilter(state, 103, 40, false);
  TEST_ASSERT_FALSE(rearmMove.accepted);

  EncoderFilterDecision tooSoon = updateEncoderFilter(state, 104, 60, false);
  TEST_ASSERT_FALSE(tooSoon.accepted);

  EncoderFilterDecision accepted = updateEncoderFilter(state, 105, 80, false);
  TEST_ASSERT_TRUE(accepted.accepted);
  TEST_ASSERT_EQUAL_INT(105, accepted.accepted_position);
}

void test_encoder_filter_reverse_requires_rewind_mode()
{
  EncoderFilterState disabledRewind = {};
  resetEncoderFilterState(disabledRewind, 200, 0);

  EncoderFilterDecision reverseIgnored = updateEncoderFilter(disabledRewind, 195, 200, false);
  TEST_ASSERT_FALSE(reverseIgnored.accepted);

  EncoderFilterState enabledRewind = {};
  resetEncoderFilterState(enabledRewind, 300, 0);

  EncoderFilterDecision belowHysteresis = updateEncoderFilter(enabledRewind, 298, 10, true);
  TEST_ASSERT_FALSE(belowHysteresis.accepted);

  EncoderFilterDecision beginReverse = updateEncoderFilter(enabledRewind, 295, 20, true);
  TEST_ASSERT_FALSE(beginReverse.accepted);

  EncoderFilterDecision reverseTooSoon = updateEncoderFilter(enabledRewind, 294, 80, true);
  TEST_ASSERT_FALSE(reverseTooSoon.accepted);

  EncoderFilterDecision reverseAccepted = updateEncoderFilter(enabledRewind, 292, 150, true);
  TEST_ASSERT_TRUE(reverseAccepted.accepted);
  TEST_ASSERT_EQUAL_INT(292, reverseAccepted.accepted_position);
}

void test_lidar_timeout_recovery_and_backoff()
{
  LidarRecoveryState state = {};
  resetLidarRecoveryState(state, 0);

  LidarRecoveryDecision timeoutEarly =
      updateLidarRecoveryState(state, LidarRecoveryEvent::TIMEOUT, 1000);
  TEST_ASSERT_FALSE(timeoutEarly.attempt_recovery);

  LidarRecoveryDecision timeoutLate =
      updateLidarRecoveryState(state, LidarRecoveryEvent::TIMEOUT, 1700);
  TEST_ASSERT_TRUE(timeoutLate.clear_display);
  TEST_ASSERT_TRUE(timeoutLate.attempt_recovery);

  noteLidarRecoveryAttemptResult(state, false, 1700);
  TEST_ASSERT_TRUE(state.recovering);
  TEST_ASSERT_GREATER_THAN_UINT32(1700, state.next_recovery_attempt_ms);

  LidarRecoveryDecision waitBackoff =
      updateLidarRecoveryState(state, LidarRecoveryEvent::ERROR, state.next_recovery_attempt_ms - 1);
  TEST_ASSERT_FALSE(waitBackoff.attempt_recovery);

  LidarRecoveryDecision afterBackoff =
      updateLidarRecoveryState(state, LidarRecoveryEvent::ERROR, state.next_recovery_attempt_ms);
  TEST_ASSERT_TRUE(afterBackoff.attempt_recovery);

  noteLidarRecoveryAttemptResult(state, true, state.next_recovery_attempt_ms);
  TEST_ASSERT_FALSE(state.recovering);
  TEST_ASSERT_EQUAL_INT(0, state.consecutive_errors);
}

void test_calibration_validation_stable_and_monotonic()
{
  const int stableSamples[8] = {300, 301, 299, 300, 302, 298, 301, 350};
  int averagedReading = 0;
  bool stable = computeStableCalibrationReading(stableSamples, 8, 6, 5, 10, averagedReading);
  TEST_ASSERT_TRUE(stable);
  TEST_ASSERT_INT_WITHIN(2, 300, averagedReading);

  const int unstableSamples[8] = {300, 320, 280, 310, 260, 340, 300, 280};
  TEST_ASSERT_FALSE(computeStableCalibrationReading(unstableSamples, 8, 6, 5, 10, averagedReading));

  const int increasing[4] = {330, 320, 310, 300};
  TEST_ASSERT_TRUE(validateMonotonicCalibration(increasing, 4, 1));

  const int nonMonotonic[4] = {330, 320, 325, 300};
  TEST_ASSERT_FALSE(validateMonotonicCalibration(nonMonotonic, 4, 1));
}

void test_prefs_migration_mode_and_blob_apply()
{
  size_t expectedBytes = expectedLegacyLensBlobSize(2);
  TEST_ASSERT_EQUAL_UINT32(sizeof(Lens) * 2, expectedBytes);

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(PrefsLoadMode::LOAD_SCHEMA),
      static_cast<int>(selectPrefsLoadMode(2, 2, expectedBytes, expectedBytes)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(PrefsLoadMode::MIGRATE_LEGACY),
      static_cast<int>(selectPrefsLoadMode(0, 2, expectedBytes, expectedBytes)));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(PrefsLoadMode::LOAD_DEFAULTS),
      static_cast<int>(selectPrefsLoadMode(0, 2, expectedBytes - 1, expectedBytes)));

  Lens legacySource[2] = {
      {1001, "LEGACY-A", 1.0f, {1, 2, 3, 4, 5, 6, 7}, {0, 0, 0, 0, 0, 0, 0}, {0}, {0}, true},
      {1002, "LEGACY-B", 1.0f, {10, 20, 30, 40, 50, 60, 70}, {0, 0, 0, 0, 0, 0, 0}, {0}, {0}, false}};

  uint8_t legacyBlob[sizeof(legacySource)] = {};
  memcpy(legacyBlob, legacySource, sizeof(legacyBlob));

  Lens targets[2] = {
      {2001, "TARGET-A", 1.0f, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}, {0}, {0}, false},
      {2002, "TARGET-B", 1.0f, {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}, {0}, {0}, true}};

  TEST_ASSERT_TRUE(applyLegacyLensBlob(legacyBlob, sizeof(legacyBlob), targets, 2));
  TEST_ASSERT_EQUAL_INT(1, targets[0].sensor_reading[0]);
  TEST_ASSERT_EQUAL_INT(70, targets[1].sensor_reading[6]);
  TEST_ASSERT_TRUE(targets[0].calibrated);
  TEST_ASSERT_FALSE(targets[1].calibrated);

  TEST_ASSERT_FALSE(applyLegacyLensBlob(legacyBlob, sizeof(legacyBlob) - 1, targets, 2));
}

void test_lidar_candidate_selection_and_blend()
{
  DTSMeasurement measurement = makeLidarDualPeakMeasurement(
      1200, 180, DataQuality::FAIR,
      1000, 1200, DataQuality::EXCELLENT);

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_TRUE(candidate.valid);
  TEST_ASSERT_EQUAL_INT(4, candidate.quality_level);
  TEST_ASSERT_GREATER_THAN_INT(0, candidate.confidence);

  TEST_ASSERT_EQUAL_INT(188, blendLidarDistance(100, 200, 80)); // near-range high-conf blend: 100*0.12 + 200*0.88 = 188
  TEST_ASSERT_EQUAL_INT(115, blendLidarDistance(80, 120, 80));  // typical near-range high-conf: 80*0.12 + 120*0.88 = 115
  TEST_ASSERT_EQUAL_INT(210, blendLidarDistance(180, 210, 80)); // just-above-boundary at high conf: pass-through, no blend
  TEST_ASSERT_EQUAL_INT(150, blendLidarDistance(0, 150, 80));   // first reading (previous=0): pass-through regardless of distance
  TEST_ASSERT_EQUAL_INT(135, blendLidarDistance(100, 200, 60));
  TEST_ASSERT_EQUAL_INT(131, blendLidarDistance(100, 200, 40));
}

void test_lidar_plausibility_gate_rejects_overshoot()
{
  // Lens at 1.0m, LiDAR returns something far past +overshoot threshold: implausible.
  TEST_ASSERT_TRUE(isLidarReadingImplausible(800, 100));
  // Lens at 1.5m, LiDAR returns 4.0m: implausible (well past +200cm overshoot).
  TEST_ASSERT_TRUE(isLidarReadingImplausible(400, 150));
}

void test_lidar_plausibility_gate_allows_undershoot_and_far_focus()
{
  // Lens at 1.0m, LiDAR returns 1.2m: well within overshoot delta — allowed.
  TEST_ASSERT_FALSE(isLidarReadingImplausible(120, 100));
  // Lens at 1.0m, LiDAR returns 0.6m: undershoot is allowed (e.g. something passed in front).
  TEST_ASSERT_FALSE(isLidarReadingImplausible(60, 100));
  // Lens focused beyond near-range gate boundary: gate disabled regardless of overshoot.
  TEST_ASSERT_FALSE(isLidarReadingImplausible(2000, 500));
  // No lens prior available: gate disabled.
  TEST_ASSERT_FALSE(isLidarReadingImplausible(800, 0));
  // Lens at boundary itself (200cm) still gated; lens just past it (201cm) not gated.
  TEST_ASSERT_TRUE(isLidarReadingImplausible(800, 200));
  TEST_ASSERT_FALSE(isLidarReadingImplausible(800, 201));
}

void test_lidar_plausibility_gate_boundary_at_overshoot_delta()
{
  // Lens at 100cm: 100+200=300 is the boundary. Reading 300cm is NOT implausible (strict greater-than).
  TEST_ASSERT_FALSE(isLidarReadingImplausible(300, 100));
  // 301cm IS implausible.
  TEST_ASSERT_TRUE(isLidarReadingImplausible(301, 100));
}

void test_lidar_stable_boost_under_min_frames_does_nothing()
{
  // Below the min-frames threshold: confidence unchanged regardless of base.
  TEST_ASSERT_EQUAL_INT(60, applyStableConfidenceBoost(60, 0));
  TEST_ASSERT_EQUAL_INT(60, applyStableConfidenceBoost(60, LIDAR_STABLE_MIN_FRAMES - 1));
}

void test_lidar_stable_boost_kicks_in_at_min_frames()
{
  // At and above the min-frames threshold: confidence rises by LIDAR_STABLE_CONFIDENCE_BOOST.
  TEST_ASSERT_EQUAL_INT(60 + LIDAR_STABLE_CONFIDENCE_BOOST,
                        applyStableConfidenceBoost(60, LIDAR_STABLE_MIN_FRAMES));
  TEST_ASSERT_EQUAL_INT(70 + LIDAR_STABLE_CONFIDENCE_BOOST,
                        applyStableConfidenceBoost(70, LIDAR_STABLE_MIN_FRAMES + 5));
}

void test_lidar_stable_boost_clamps_at_max()
{
  // Boost cannot push past the cap, so a high-base confidence does not become 'excellent' purely from stability.
  TEST_ASSERT_EQUAL_INT(LIDAR_STABLE_MAX_CONFIDENCE,
                        applyStableConfidenceBoost(90, LIDAR_STABLE_MIN_FRAMES));
  TEST_ASSERT_EQUAL_INT(LIDAR_STABLE_MAX_CONFIDENCE,
                        applyStableConfidenceBoost(99, LIDAR_STABLE_MIN_FRAMES));
}

void test_lidar_sunlight_warn_hysteresis()
{
  // Off and below enter: stays off.
  TEST_ASSERT_FALSE(updateSunlightWarnState(false, LIDAR_SUNLIGHT_WARN_ENTER - 1));
  // Off and at/above enter: switches on.
  TEST_ASSERT_TRUE(updateSunlightWarnState(false, LIDAR_SUNLIGHT_WARN_ENTER));
  TEST_ASSERT_TRUE(updateSunlightWarnState(false, LIDAR_SUNLIGHT_WARN_ENTER + 500));
  // On and between exit/enter: holds (the hysteresis band).
  TEST_ASSERT_TRUE(updateSunlightWarnState(true, LIDAR_SUNLIGHT_WARN_ENTER - 1));
  TEST_ASSERT_TRUE(updateSunlightWarnState(true, LIDAR_SUNLIGHT_WARN_EXIT));
  // On and below exit: switches off.
  TEST_ASSERT_FALSE(updateSunlightWarnState(true, LIDAR_SUNLIGHT_WARN_EXIT - 1));
  // Boundary: exactly enter triggers; exactly exit holds.
  TEST_ASSERT_FALSE(updateSunlightWarnState(false, LIDAR_SUNLIGHT_WARN_EXIT)); // off, below enter
  TEST_ASSERT_TRUE(updateSunlightWarnState(true, LIDAR_SUNLIGHT_WARN_EXIT));   // on, at exit
}

void test_lidar_rejects_invalid_quality_and_zero_intensity()
{
  // Below LIDAR_FUSION_MIN_INTENSITY and fallback floor; quality is INVALID.
  DTSMeasurement measurement = makeLidarMeasurement(1000, 0, DataQuality::INVALID);

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_FALSE(candidate.valid);
}

void test_lidar_distance_display_formatting_covers_each_band()
{
  char formattedDistance[16] = {0};

  formatDistanceDisplay(4, formattedDistance, sizeof(formattedDistance));
  TEST_ASSERT_EQUAL_STRING("<15cm", formattedDistance);
  formatDistanceDisplay(75, formattedDistance, sizeof(formattedDistance));
  TEST_ASSERT_EQUAL_STRING("75cm", formattedDistance);
  formatDistanceDisplay(1050, formattedDistance, sizeof(formattedDistance));
  TEST_ASSERT_EQUAL_STRING("10.5m", formattedDistance);
  formatDistanceDisplay(1051, formattedDistance, sizeof(formattedDistance));
  TEST_ASSERT_EQUAL_STRING("Inf.", formattedDistance);
  formatDistanceDisplay(1900, formattedDistance, sizeof(formattedDistance));
  TEST_ASSERT_EQUAL_STRING("Inf.", formattedDistance);
  formatDistanceDisplay(150, formattedDistance, sizeof(formattedDistance));
  TEST_ASSERT_EQUAL_STRING("1.50m", formattedDistance); // near-range precision: 2dp below 2m
}

void test_lidar_low_confidence_tracks_beyond_previous_distance()
{
  // 6.0m target with low/harsh-light-like return, but still valid.
  DTSMeasurement measurement = makeLidarMeasurement(6000, 100, DataQuality::POOR);

  int filtered_distance_cm = 250;
  const int lens_prior_cm = 250;
  for (int i = 0; i < 8; i++)
  {
    LidarCandidate candidate =
        chooseBestLidarCandidate(measurement, filtered_distance_cm, true, lens_prior_cm);
    TEST_ASSERT_TRUE(candidate.valid);
    filtered_distance_cm =
        blendLidarDistance(filtered_distance_cm, candidate.distance_cm, candidate.confidence);
  }

  TEST_ASSERT_GREATER_THAN_INT(350, filtered_distance_cm);
}

void test_lidar_dynamic_intensity_threshold_accepts_mid_range()
{
  // 3.2m at exactly the mid-range minimum intensity.
  DTSMeasurement measurement = makeLidarMeasurement(
      3200, LIDAR_FUSION_MIN_INTENSITY_MID, DataQuality::GOOD);

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_TRUE(candidate.valid);
}

void test_lidar_far_fallback_prevents_dropouts()
{
  // 4.2m, just above the fallback intensity floor, with INVALID quality.
  DTSMeasurement measurement = makeLidarMeasurement(
      4200, LIDAR_FALLBACK_MIN_INTENSITY + 1, DataQuality::INVALID);

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 250, false, 0);
  TEST_ASSERT_TRUE(candidate.valid);
  TEST_ASSERT_GREATER_THAN_INT(0, candidate.confidence);
  TEST_ASSERT_EQUAL_INT(1, candidate.quality_level);
}

void test_lidar_candidate_fusion_when_returns_agree()
{
  // 300cm primary + 312cm secondary (within fusion-agree delta) — both valid.
  DTSMeasurement measurement = makeLidarDualPeakMeasurement(
      3000, 180, DataQuality::GOOD,
      3120, 170, DataQuality::FAIR);

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_TRUE(candidate.valid);
  TEST_ASSERT_INT_WITHIN(2, 305, candidate.distance_cm);
  TEST_ASSERT_GREATER_THAN_INT(55, candidate.confidence);
  TEST_ASSERT_EQUAL_INT(3, candidate.quality_level);
}

void test_lidar_lens_prior_is_range_weighted()
{
  DTSMeasurement nearMeasurement = makeLidarMeasurement(2000, 200, DataQuality::GOOD);

  DTSMeasurement farMeasurement = nearMeasurement;
  farMeasurement.primaryDistance_mm = 8000; // 800cm

  // Keep equal prior mismatch (100cm) in both ranges; near range should be penalized more.
  LidarCandidate nearCandidate = chooseBestLidarCandidate(nearMeasurement, 0, true, 100);
  LidarCandidate farCandidate = chooseBestLidarCandidate(farMeasurement, 0, true, 700);

  TEST_ASSERT_TRUE(nearCandidate.valid);
  TEST_ASSERT_TRUE(farCandidate.valid);
  TEST_ASSERT_LESS_THAN_INT(farCandidate.confidence, nearCandidate.confidence);
}

void test_lidar_sunlight_penalizes_confidence_without_dropping_valid_measurement()
{
  // 450cm subject under low ambient IR; sunlightBase is set below.
  DTSMeasurement lowAmbientMeasurement = makeLidarMeasurement(4500, 40, DataQuality::GOOD);
  lowAmbientMeasurement.sunlightBase = 25;

  DTSMeasurement highAmbientMeasurement = lowAmbientMeasurement;
  highAmbientMeasurement.sunlightBase = 650;

  LidarCandidate lowAmbientCandidate = chooseBestLidarCandidate(lowAmbientMeasurement, 0, false, 0);
  LidarCandidate highAmbientCandidate = chooseBestLidarCandidate(highAmbientMeasurement, 0, false, 0);

  TEST_ASSERT_TRUE(lowAmbientCandidate.valid);
  TEST_ASSERT_TRUE(highAmbientCandidate.valid);
  TEST_ASSERT_LESS_THAN_INT(lowAmbientCandidate.confidence, highAmbientCandidate.confidence);
}

void test_lidar_sunlight_hard_rejects_very_low_snr_measurement()
{
  // 700cm at the far-range minimum intensity, under heavy ambient IR.
  DTSMeasurement measurement = makeLidarMeasurement(
      7000, LIDAR_FUSION_MIN_INTENSITY_FAR, DataQuality::GOOD);
  measurement.sunlightBase = 1500;

  LidarCandidate candidate = chooseBestLidarCandidate(measurement, 0, false, 0);
  TEST_ASSERT_FALSE(candidate.valid);
}

void test_lens_snap_and_distance_estimation()
{
  Lens lens = makeTestLens();

  TEST_ASSERT_EQUAL_INT(1, findLensSnapIndex(lens, 201));
  TEST_ASSERT_EQUAL_INT(4, findLensSnapIndex(lens, 503));
  TEST_ASSERT_EQUAL_INT(-1, findLensSnapIndex(lens, 102));

  LensDistanceEstimate below = estimateLensDistance(lens, 90);
  TEST_ASSERT_TRUE(below.valid);
  TEST_ASSERT_FALSE(below.is_infinity);
  TEST_ASSERT_EQUAL_INT(100, below.distance_cm);

  LensDistanceEstimate interpolated = estimateLensDistance(lens, 250);
  TEST_ASSERT_TRUE(interpolated.valid);
  TEST_ASSERT_FALSE(interpolated.is_infinity);
  TEST_ASSERT_EQUAL_INT(135, interpolated.distance_cm);

  LensDistanceEstimate infinity = estimateLensDistance(lens, 706);
  TEST_ASSERT_TRUE(infinity.valid);
  TEST_ASSERT_TRUE(infinity.is_infinity);
  TEST_ASSERT_EQUAL_INT(LENS_INFINITY_RAW, infinity.distance_cm);
}

void test_lens_logic_ignores_unused_distance_markers()
{
  Lens lens = {
      15056,
      "150/5.6",
      150.0f,
      {100, 200, 300, 400, 500, 0, 0},
      {2.0f, 2.5f, 3.0f, 5.0f, 10.0f, 0.0f, 0.0f},
      {5.6f, 8.0f, 11.0f, 16.0f, 22.0f, 32.0f, 45.0f, 0.0f, 0.0f},
      {0, 0, 0, 0},
      true};

  TEST_ASSERT_EQUAL_INT(5, getLensDistancePointCount(lens));
  TEST_ASSERT_EQUAL_INT(2, findLensSnapIndex(lens, 301));

  LensDistanceEstimate interpolated = estimateLensDistance(lens, 250);
  TEST_ASSERT_TRUE(interpolated.valid);
  TEST_ASSERT_FALSE(interpolated.is_infinity);
  TEST_ASSERT_EQUAL_INT(275, interpolated.distance_cm);

  LensDistanceEstimate infinity = estimateLensDistance(lens, 506);
  TEST_ASSERT_TRUE(infinity.valid);
  TEST_ASSERT_TRUE(infinity.is_infinity);
  TEST_ASSERT_EQUAL_INT(LENS_INFINITY_RAW, infinity.distance_cm);
}

void test_150mm_profile_uses_custom_distance_scale()
{
  const Lens *lens150 = nullptr;
  for (size_t i = 0; i < NUM_LENSES; i++)
  {
    if (lenses[i].id == 15056)
    {
      lens150 = &lenses[i];
      break;
    }
  }

  TEST_ASSERT_NOT_NULL(lens150);
  TEST_ASSERT_EQUAL_INT(5, getLensDistancePointCount(*lens150));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.0f, lens150->distance[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.5f, lens150->distance[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.0f, lens150->distance[2]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f, lens150->distance[3]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f, lens150->distance[4]);
}

void test_250mm_profiles_use_custom_distance_scales()
{
  const Lens *lens250f5 = nullptr;
  const Lens *lens250f8 = nullptr;
  for (size_t i = 0; i < NUM_LENSES; i++)
  {
    if (lenses[i].id == 25005)
    {
      lens250f5 = &lenses[i];
    }
    else if (lenses[i].id == 25008)
    {
      lens250f8 = &lenses[i];
    }
  }

  TEST_ASSERT_NOT_NULL(lens250f5);
  TEST_ASSERT_NOT_NULL(lens250f8);

  TEST_ASSERT_EQUAL_INT(10, getLensDistancePointCount(*lens250f5));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.5f, lens250f5->distance[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 4.0f, lens250f5->distance[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f, lens250f5->distance[2]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 7.0f, lens250f5->distance[3]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 8.0f, lens250f5->distance[4]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f, lens250f5->distance[5]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 15.0f, lens250f5->distance[6]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 20.0f, lens250f5->distance[7]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 30.0f, lens250f5->distance[8]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 50.0f, lens250f5->distance[9]);

  TEST_ASSERT_EQUAL_INT(9, getLensDistancePointCount(*lens250f8));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.5f, lens250f8->distance[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 4.0f, lens250f8->distance[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 5.0f, lens250f8->distance[2]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 7.0f, lens250f8->distance[3]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10.0f, lens250f8->distance[4]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 15.0f, lens250f8->distance[5]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 20.0f, lens250f8->distance[6]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 30.0f, lens250f8->distance[7]);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 50.0f, lens250f8->distance[8]);
}

void test_calibration_median_spread_tolerates_gentle_drift()
{
  // 8 samples drifting gently upward: total spread 14 exceeds the old
  // min-to-max limit of 10, but max deviation from median (504) is only 8,
  // which the median-based check should accept.
  const int driftSamples[8] = {500, 502, 504, 504, 506, 506, 508, 510};
  int averagedReading = 0;
  bool stable = computeStableCalibrationReading(driftSamples, 8, 6, 5, 10, averagedReading);
  TEST_ASSERT_TRUE(stable);
  TEST_ASSERT_INT_WITHIN(3, 505, averagedReading);
}

void test_calibration_rejects_truly_unstable_readings()
{
  // Max deviation from median exceeds the spread limit.
  const int wildSamples[8] = {300, 310, 320, 300, 310, 320, 300, 310};
  int averagedReading = 0;
  bool stable = computeStableCalibrationReading(wildSamples, 8, 6, 5, 10, averagedReading);
  TEST_ASSERT_FALSE(stable);
}

void test_lightmeter_dark_bright_fraction_and_seconds()
{
  char formattedShutter[16] = {0};
  formatShutterSpeed(0.0f, 8.0f, 400, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("Dark!", formattedShutter);
  formatShutterSpeed(50000.0f, 2.0f, 100, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("Bright!", formattedShutter);
  // K=12.5: speed = 64*12.5/(320*400) = 0.00625 → rounds to 0.006 → 1/250
  formatShutterSpeed(320.0f, 8.0f, 400, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("1/250 sec.", formattedShutter);
  // K=12.5: speed = 4*12.5/(0.5*50) = 2.0s → 2 sec
  formatShutterSpeed(0.5f, 2.0f, 50, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("2 sec.", formattedShutter);
  // K=12.5: speed = 4*12.5/(0.4*100) = 1.25s → rounds to 1.5 sec
  formatShutterSpeed(0.4f, 2.0f, 100, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("1.5 sec.", formattedShutter);
  // K=12.5: speed = 4*12.5/(0.5*100) = 1.0s → 1 sec
  formatShutterSpeed(0.5f, 2.0f, 100, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("1 sec.", formattedShutter);
  // K=12.5: speed = 4*12.5/(0.001*100) = 500s → 8m20s
  formatShutterSpeed(0.001f, 2.0f, 100, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("8m20s", formattedShutter);
  // absurdly dark → capped at 25m0s
  formatShutterSpeed(0.00001f, 2.0f, 100, formattedShutter, sizeof(formattedShutter));
  TEST_ASSERT_EQUAL_STRING("25m0s", formattedShutter);
}

void test_lidar_secondary_quality_inherits_primary_when_invalid()
{
  // chooseBestLidarCandidate remaps a secondary peak's INVALID quality onto
  // the primary's quality before scoring. Verify that a peak with explicit
  // EXCELLENT and a peak whose quality remaps to EXCELLENT produce the same
  // fused candidate.
  DTSMeasurement explicitExcellent = makeLidarDualPeakMeasurement(
      3000, 180, DataQuality::EXCELLENT,
      3120, 170, DataQuality::EXCELLENT);
  DTSMeasurement inheritsFromPrimary = makeLidarDualPeakMeasurement(
      3000, 180, DataQuality::EXCELLENT,
      3120, 170, DataQuality::INVALID);

  LidarCandidate explicitCandidate = chooseBestLidarCandidate(explicitExcellent, 0, false, 0);
  LidarCandidate inheritedCandidate = chooseBestLidarCandidate(inheritsFromPrimary, 0, false, 0);

  TEST_ASSERT_TRUE(explicitCandidate.valid);
  TEST_ASSERT_TRUE(inheritedCandidate.valid);
  TEST_ASSERT_EQUAL_INT(explicitCandidate.distance_cm, inheritedCandidate.distance_cm);
  TEST_ASSERT_EQUAL_INT(explicitCandidate.confidence, inheritedCandidate.confidence);
  TEST_ASSERT_EQUAL_INT(explicitCandidate.quality_level, inheritedCandidate.quality_level);
}

void test_ui_signature_hash_primitives_are_deterministic_and_distinct()
{
  // Determinism: same inputs produce same output.
  TEST_ASSERT_EQUAL_UINT32(hashInt(HASH_OFFSET_BASIS, 42), hashInt(HASH_OFFSET_BASIS, 42));
  TEST_ASSERT_EQUAL_UINT32(hashBool(HASH_OFFSET_BASIS, true), hashBool(HASH_OFFSET_BASIS, true));
  TEST_ASSERT_EQUAL_UINT32(hashFloat(HASH_OFFSET_BASIS, 1.5f), hashFloat(HASH_OFFSET_BASIS, 1.5f));
  TEST_ASSERT_EQUAL_UINT32(hashCString(HASH_OFFSET_BASIS, "abc"), hashCString(HASH_OFFSET_BASIS, "abc"));

  // Distinctness: any change in input changes the hash.
  TEST_ASSERT_NOT_EQUAL(hashInt(HASH_OFFSET_BASIS, 0), hashInt(HASH_OFFSET_BASIS, 1));
  TEST_ASSERT_NOT_EQUAL(hashBool(HASH_OFFSET_BASIS, false), hashBool(HASH_OFFSET_BASIS, true));
  TEST_ASSERT_NOT_EQUAL(hashFloat(HASH_OFFSET_BASIS, 1.0f), hashFloat(HASH_OFFSET_BASIS, 1.0000001f));
  TEST_ASSERT_NOT_EQUAL(hashCString(HASH_OFFSET_BASIS, "ab"), hashCString(HASH_OFFSET_BASIS, "ba"));

  // Length is part of the hash, so "ab"+"c" and "a"+"bc" do not collide.
  uint32_t ab_then_c = hashCString(hashCString(HASH_OFFSET_BASIS, "ab"), "c");
  uint32_t a_then_bc = hashCString(hashCString(HASH_OFFSET_BASIS, "a"), "bc");
  TEST_ASSERT_NOT_EQUAL(ab_then_c, a_then_bc);
}

void test_ui_signature_hash_cstring_treats_null_as_empty()
{
  TEST_ASSERT_EQUAL_UINT32(hashCString(HASH_OFFSET_BASIS, nullptr),
                           hashCString(HASH_OFFSET_BASIS, ""));
}

namespace
{
ExternalUiSnapshot baselineExternalSnapshot()
{
  return {/* selected_format */ 1,
          /* selected_lens   */ 2,
          /* bat_per         */ 75,
          /* film_counter    */ 5,
          /* frame_progress  */ 0.25f,
          /* sleepMode       */ false};
}
} // namespace

void test_ui_signature_external_changes_when_any_field_changes()
{
  const ExternalUiSnapshot base = baselineExternalSnapshot();
  const uint32_t baseline = buildExternalUiSignature(base);

  TEST_ASSERT_EQUAL_UINT32(baseline, buildExternalUiSignature(baselineExternalSnapshot()));

  ExternalUiSnapshot mutated = base;
  mutated.selected_format = 9;
  TEST_ASSERT_NOT_EQUAL(baseline, buildExternalUiSignature(mutated));

  mutated = base;
  mutated.selected_lens = 9;
  TEST_ASSERT_NOT_EQUAL(baseline, buildExternalUiSignature(mutated));

  mutated = base;
  mutated.bat_per = 50;
  TEST_ASSERT_NOT_EQUAL(baseline, buildExternalUiSignature(mutated));

  mutated = base;
  mutated.film_counter = 6;
  TEST_ASSERT_NOT_EQUAL(baseline, buildExternalUiSignature(mutated));

  mutated = base;
  mutated.frame_progress = 0.5f;
  TEST_ASSERT_NOT_EQUAL(baseline, buildExternalUiSignature(mutated));

  mutated = base;
  mutated.sleepMode = true;
  TEST_ASSERT_NOT_EQUAL(baseline, buildExternalUiSignature(mutated));
}

int main(int, char **)
{
  UNITY_BEGIN();
  RUN_TEST(test_frame_counter_exact_and_interpolation);
  RUN_TEST(test_frame_counter_snap_and_roll_end);
  RUN_TEST(test_frame_counter_frame_offset_and_spacing);
  RUN_TEST(test_format_3x6_supports_21_frames);
  RUN_TEST(test_encoder_filter_forward_hysteresis_and_debounce);
  RUN_TEST(test_encoder_filter_reverse_requires_rewind_mode);
  RUN_TEST(test_lidar_timeout_recovery_and_backoff);
  RUN_TEST(test_calibration_validation_stable_and_monotonic);
  RUN_TEST(test_calibration_median_spread_tolerates_gentle_drift);
  RUN_TEST(test_calibration_rejects_truly_unstable_readings);
  RUN_TEST(test_prefs_migration_mode_and_blob_apply);
  RUN_TEST(test_lidar_candidate_selection_and_blend);
  RUN_TEST(test_lidar_plausibility_gate_rejects_overshoot);
  RUN_TEST(test_lidar_plausibility_gate_allows_undershoot_and_far_focus);
  RUN_TEST(test_lidar_plausibility_gate_boundary_at_overshoot_delta);
  RUN_TEST(test_lidar_stable_boost_under_min_frames_does_nothing);
  RUN_TEST(test_lidar_stable_boost_kicks_in_at_min_frames);
  RUN_TEST(test_lidar_stable_boost_clamps_at_max);
  RUN_TEST(test_lidar_sunlight_warn_hysteresis);
  RUN_TEST(test_lidar_rejects_invalid_quality_and_zero_intensity);
  RUN_TEST(test_lidar_distance_display_formatting_covers_each_band);
  RUN_TEST(test_lidar_low_confidence_tracks_beyond_previous_distance);
  RUN_TEST(test_lidar_dynamic_intensity_threshold_accepts_mid_range);
  RUN_TEST(test_lidar_far_fallback_prevents_dropouts);
  RUN_TEST(test_lidar_candidate_fusion_when_returns_agree);
  RUN_TEST(test_lidar_lens_prior_is_range_weighted);
  RUN_TEST(test_lidar_sunlight_penalizes_confidence_without_dropping_valid_measurement);
  RUN_TEST(test_lidar_sunlight_hard_rejects_very_low_snr_measurement);
  RUN_TEST(test_lens_snap_and_distance_estimation);
  RUN_TEST(test_lens_logic_ignores_unused_distance_markers);
  RUN_TEST(test_150mm_profile_uses_custom_distance_scale);
  RUN_TEST(test_250mm_profiles_use_custom_distance_scales);
  RUN_TEST(test_lightmeter_dark_bright_fraction_and_seconds);
  RUN_TEST(test_lidar_secondary_quality_inherits_primary_when_invalid);
  RUN_TEST(test_ui_signature_hash_primitives_are_deterministic_and_distinct);
  RUN_TEST(test_ui_signature_hash_cstring_treats_null_as_empty);
  RUN_TEST(test_ui_signature_external_changes_when_any_field_changes);
  return UNITY_END();
}
