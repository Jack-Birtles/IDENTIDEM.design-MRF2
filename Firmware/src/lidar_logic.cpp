#include "lidar_logic.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "mrfconstants.h"

// File scope (not the anonymous namespace) so the diagnostics screen and unit
// tests can call it; the in-namespace helpers below also use it.
int computeSnrPermille(uint16_t intensity, uint16_t sunlight_base)
{
  if (sunlight_base == 0)
  {
    return -1;
  }

  return static_cast<int>((static_cast<unsigned long>(intensity) * 1000UL) /
                          static_cast<unsigned long>(sunlight_base));
}

int applyLidarCalibrationCm(int raw_cm)
{
  if (raw_cm <= 0)
  {
    return raw_cm;
  }

  if (raw_cm >= static_cast<int>(LIDAR_CAL_CUTOFF_CM))
  {
    return raw_cm;
  }

  if (LIDAR_CAL_REF_RAW_CM <= 0.0f || LIDAR_CAL_REF_TRUE_CM <= 0.0f || LIDAR_CAL_REF_RAW_CM >= LIDAR_CAL_CUTOFF_CM)
  {
    return raw_cm;
  }

  static float exponent = 0.0f;
  static bool exponent_init = false;
  if (!exponent_init)
  {
    exponent = logf(LIDAR_CAL_REF_TRUE_CM / LIDAR_CAL_REF_RAW_CM) /
               logf(LIDAR_CAL_REF_RAW_CM / LIDAR_CAL_CUTOFF_CM);
    exponent_init = true;
  }

  float raw_cm_f = static_cast<float>(raw_cm);
  float scaled = raw_cm_f * powf(raw_cm_f / LIDAR_CAL_CUTOFF_CM, exponent);
  int corrected_cm = static_cast<int>(roundf(scaled));

  // Interim guard until the measured piecewise table lands (bd 4p9): the single
  // reference-point power law over-corrects below the anchor (raw 65cm -> 14cm),
  // dropping real sub-metre subjects below the display floor. Never remove more
  // than (100 - LIDAR_CAL_MIN_OUTPUT_PCT)% of the raw distance. The measured
  // anchor (130 -> 100, 77%) sits well above the floor and is unaffected.
  int floor_cm = raw_cm * LIDAR_CAL_MIN_OUTPUT_PCT / 100;
  return max(corrected_cm, floor_cm);
}

namespace
{
struct QualityProfile
{
  int base_score;
  int prior_penalty_cap;
  float prior_weight;
  int quality_level; // 1..4 for poor..excellent, 0 for INVALID
};

// Indexed by DataQuality enum value (EXCELLENT=0 .. INVALID=4).
const QualityProfile QUALITY_PROFILES[] = {
    /* EXCELLENT */ {80, LIDAR_PRIOR_PENALTY_MAX_EXCELLENT, LIDAR_LENS_PRIOR_WEIGHT_EXCELLENT, 4},
    /* GOOD      */ {65, LIDAR_PRIOR_PENALTY_MAX_GOOD,      LIDAR_LENS_PRIOR_WEIGHT_GOOD,      3},
    /* FAIR      */ {45, LIDAR_PRIOR_PENALTY_MAX_FAIR,      LIDAR_LENS_PRIOR_WEIGHT_GOOD,      2},
    /* POOR      */ {25, LIDAR_PRIOR_PENALTY_MAX_POOR,      LIDAR_LENS_PRIOR_WEIGHT_GOOD,      1},
    /* INVALID   */ { 0, LIDAR_PRIOR_PENALTY_MAX_POOR,      LIDAR_LENS_PRIOR_WEIGHT_GOOD,      0},
};

const QualityProfile &profileFor(DataQuality quality)
{
  uint8_t idx = static_cast<uint8_t>(quality);
  const uint8_t invalid_idx = static_cast<uint8_t>(DataQuality::INVALID);
  if (idx > invalid_idx)
  {
    idx = invalid_idx;
  }
  return QUALITY_PROFILES[idx];
}

// Pick the value for the first tier whose max_cm covers `distance_cm`.
// The final tier's max_cm is ignored — it acts as the "very far" fallback.
template <typename T>
struct DistanceTier
{
  int max_cm;
  T value;
};

template <typename T, size_t N>
T lookupByDistance(int distance_cm, const DistanceTier<T> (&tiers)[N])
{
  for (size_t i = 0; i + 1 < N; i++)
  {
    if (distance_cm <= tiers[i].max_cm)
    {
      return tiers[i].value;
    }
  }
  return tiers[N - 1].value;
}

float priorRangeScaleForDistanceCm(int corrected_cm)
{
  static const DistanceTier<float> tiers[] = {
      {LIDAR_PRIOR_RANGE_NEAR_CM, LIDAR_PRIOR_RANGE_SCALE_NEAR},
      {LIDAR_PRIOR_RANGE_MID_CM, LIDAR_PRIOR_RANGE_SCALE_MID},
      {LIDAR_PRIOR_RANGE_FAR_CM, LIDAR_PRIOR_RANGE_SCALE_FAR},
      {0, LIDAR_PRIOR_RANGE_SCALE_VERY_FAR},
  };
  return lookupByDistance(corrected_cm, tiers);
}

int minIntensityThresholdForDistanceCm(int raw_cm)
{
  static const DistanceTier<int> tiers[] = {
      {LIDAR_FUSION_INTENSITY_NEAR_RANGE_CM, LIDAR_FUSION_MIN_INTENSITY},
      {LIDAR_FUSION_INTENSITY_MID_RANGE_CM, LIDAR_FUSION_MIN_INTENSITY_MID},
      {LIDAR_FUSION_INTENSITY_FAR_RANGE_CM, LIDAR_FUSION_MIN_INTENSITY_FAR},
      {0, LIDAR_FUSION_MIN_INTENSITY_MAX_RANGE},
  };
  return lookupByDistance(raw_cm, tiers);
}

int snrTargetPermilleForDistanceCm(int raw_cm)
{
  static const DistanceTier<int> tiers[] = {
      {LIDAR_FUSION_INTENSITY_NEAR_RANGE_CM, LIDAR_SNR_PERMILLE_TARGET_NEAR},
      {LIDAR_FUSION_INTENSITY_MID_RANGE_CM, LIDAR_SNR_PERMILLE_TARGET_MID},
      {LIDAR_FUSION_INTENSITY_FAR_RANGE_CM, LIDAR_SNR_PERMILLE_TARGET_FAR},
      {0, LIDAR_SNR_PERMILLE_TARGET_MAX_RANGE},
  };
  return lookupByDistance(raw_cm, tiers);
}

bool shouldRejectBySnrFloor(int raw_cm, uint16_t intensity, uint16_t sunlight_base)
{
  int snr_permille = computeSnrPermille(intensity, sunlight_base);
  if (snr_permille < 0)
  {
    return false;
  }

  int min_intensity = minIntensityThresholdForDistanceCm(raw_cm);
  int hard_reject_intensity = min_intensity * LIDAR_SNR_HARD_REJECT_INTENSITY_MULTIPLIER;
  return snr_permille < LIDAR_SNR_PERMILLE_HARD_REJECT &&
         static_cast<int>(intensity) < hard_reject_intensity;
}

int snrPenaltyForAmbientLight(int raw_cm,
                              uint16_t intensity,
                              uint16_t sunlight_base,
                              int penalty_max)
{
  int snr_permille = computeSnrPermille(intensity, sunlight_base);
  if (snr_permille < 0)
  {
    return 0;
  }

  int target_snr = snrTargetPermilleForDistanceCm(raw_cm);
  if (snr_permille >= target_snr)
  {
    return 0;
  }

  int deficit = target_snr - snr_permille;
  int penalty = (deficit + (LIDAR_SNR_PENALTY_DIVISOR - 1)) / LIDAR_SNR_PENALTY_DIVISOR;
  return min(penalty_max, penalty);
}

struct LidarReading
{
  bool valid;
  int raw_cm;
  int corrected_cm;
};

// Shared gating for both the primary and the fallback candidate builders:
// rejects invalid sensor readings, applies the SNR floor, and calibrates the
// distance. Each builder layers its own additional reject checks on top.
LidarReading prepareLidarReading(uint16_t raw_distance_mm,
                                 uint16_t intensity,
                                 uint16_t sunlight_base)
{
  LidarReading reading = {false, 0, 0};
  if (raw_distance_mm == DTS_INVALID_DISTANCE)
  {
    return reading;
  }

  int raw_cm = static_cast<int>(raw_distance_mm) / LIDAR_DISTANCE_DIVISOR;
  if (raw_cm <= 0)
  {
    return reading;
  }

  if (shouldRejectBySnrFloor(raw_cm, intensity, sunlight_base))
  {
    return reading;
  }

  int corrected_cm = applyLidarCalibrationCm(raw_cm);
  if (corrected_cm <= 0)
  {
    return reading;
  }

  reading.valid = true;
  reading.raw_cm = raw_cm;
  reading.corrected_cm = corrected_cm;
  return reading;
}

LidarCandidate buildFallbackLidarCandidate(uint16_t raw_distance_mm,
                                           uint16_t intensity,
                                           uint16_t sunlight_base,
                                           DataQuality quality,
                                           int previous_distance_cm)
{
  LidarCandidate candidate = {false, 0, 0, 0};
  LidarReading reading = prepareLidarReading(raw_distance_mm, intensity, sunlight_base);
  if (!reading.valid)
  {
    return candidate;
  }

  // Only allow distance-only fallback when there is at least some return signal
  // or the sensor reports non-invalid quality.
  if (intensity < LIDAR_FALLBACK_MIN_INTENSITY && quality == DataQuality::INVALID)
  {
    return candidate;
  }

  const int raw_cm = reading.raw_cm;
  const int corrected_cm = reading.corrected_cm;

  const QualityProfile &profile = profileFor(quality);
  int confidence = LIDAR_FALLBACK_BASE_CONFIDENCE + (profile.base_score / 8);
  if (previous_distance_cm > 0)
  {
    confidence -= min(LIDAR_TEMPORAL_PENALTY_MAX,
                      abs(corrected_cm - previous_distance_cm) / LIDAR_TEMPORAL_PENALTY_DIVISOR);
  }
  confidence -= snrPenaltyForAmbientLight(
      raw_cm, intensity, sunlight_base, LIDAR_SNR_FALLBACK_PENALTY_MAX);
  confidence = constrain(confidence, 0, LIDAR_FALLBACK_MAX_CONFIDENCE);
  if (confidence <= 0)
  {
    return candidate;
  }

  candidate.valid = true;
  candidate.distance_cm = corrected_cm;
  candidate.confidence = confidence;
  candidate.quality_level = max(1, profile.quality_level);
  return candidate;
}

LidarCandidate buildLidarCandidate(uint16_t raw_distance_mm,
                                   uint16_t intensity,
                                   uint16_t sunlight_base,
                                   DataQuality quality,
                                   bool secondary_candidate,
                                   int previous_distance_cm,
                                   bool has_lens_prior,
                                   int lens_prior_cm)
{
  LidarCandidate candidate = {false, 0, 0, 0};

  LidarReading reading = prepareLidarReading(raw_distance_mm, intensity, sunlight_base);
  if (!reading.valid)
  {
    return candidate;
  }

  if (intensity < minIntensityThresholdForDistanceCm(reading.raw_cm))
  {
    return candidate;
  }

  const int raw_cm = reading.raw_cm;
  const int corrected_cm = reading.corrected_cm;

  const QualityProfile &profile = profileFor(quality);
  int confidence = profile.base_score;
  confidence += min(20, static_cast<int>(intensity / 150));

  if (previous_distance_cm > 0)
  {
    confidence -= min(LIDAR_TEMPORAL_PENALTY_MAX,
                      abs(corrected_cm - previous_distance_cm) / LIDAR_TEMPORAL_PENALTY_DIVISOR);
  }
  confidence -= snrPenaltyForAmbientLight(raw_cm, intensity, sunlight_base, LIDAR_SNR_PENALTY_MAX);

  if (has_lens_prior)
  {
    int prior_error_cm = abs(corrected_cm - lens_prior_cm);
    int effective_error_cm = max(0, prior_error_cm - LIDAR_PRIOR_DEADBAND_CM);
    float prior_weight = profile.prior_weight * priorRangeScaleForDistanceCm(corrected_cm);
    int prior_penalty = static_cast<int>(roundf(static_cast<float>(effective_error_cm) * prior_weight));
    confidence -= min(profile.prior_penalty_cap, prior_penalty);
  }

  if (secondary_candidate)
  {
    confidence -= 2;
  }

  confidence = constrain(confidence, 0, 100);
  if (confidence == 0)
  {
    return candidate;
  }

  candidate.valid = true;
  candidate.distance_cm = corrected_cm;
  candidate.confidence = confidence;
  candidate.quality_level = profile.quality_level;
  return candidate;
}

LidarCandidate fuseLidarCandidates(const LidarCandidate &primary, const LidarCandidate &secondary)
{
  int weight_sum = max(1, primary.confidence + secondary.confidence);
  int weighted_distance_sum = (primary.distance_cm * primary.confidence) +
                              (secondary.distance_cm * secondary.confidence);
  int fused_distance_cm = static_cast<int>(roundf(static_cast<float>(weighted_distance_sum) /
                                                  static_cast<float>(weight_sum)));
  // Take the stronger candidate's confidence (not the average) so a weak but
  // agreeing secondary can only add the agreement bonus, never demote a confident
  // primary. Mirrors the max() used for the fused quality level just below.
  int fused_confidence = constrain(max(primary.confidence, secondary.confidence) +
                                       LIDAR_FUSION_CONF_BONUS,
                                   0,
                                   100);
  int fused_quality_level = max(primary.quality_level, secondary.quality_level);
  return {true, fused_distance_cm, fused_confidence, fused_quality_level};
}

// Fuse the pair when both are valid and agree within the fusion delta; otherwise
// return the stronger of the two (or the only valid one). Returns the empty
// primary when both are invalid, so the caller can fall through to a fallback.
LidarCandidate selectBestPair(const LidarCandidate &primary, const LidarCandidate &secondary)
{
  if (primary.valid && secondary.valid &&
      abs(primary.distance_cm - secondary.distance_cm) <= LIDAR_FUSION_AGREE_DELTA_CM)
  {
    return fuseLidarCandidates(primary, secondary);
  }
  if (!primary.valid || (secondary.valid && secondary.confidence > primary.confidence))
  {
    return secondary;
  }
  return primary;
}
} // namespace

LidarCandidate chooseBestLidarCandidate(const DTSMeasurement &measurement,
                                        int previous_distance_cm,
                                        bool has_lens_prior,
                                        int lens_prior_cm)
{
  LidarCandidate primary = buildLidarCandidate(measurement.primaryDistance_mm,
                                               measurement.primaryIntensity,
                                               measurement.sunlightBase,
                                               measurement.primaryQuality,
                                               false,
                                               previous_distance_cm,
                                               has_lens_prior,
                                               lens_prior_cm);

  // Only build secondary candidate when a dual-peak target is present
  // (valid distance AND non-zero intensity, matching library hasSecondaryTarget() logic).
  bool has_secondary = measurement.secondaryDistance_mm != DTS_INVALID_DISTANCE &&
                       measurement.secondaryIntensity > 0;

  LidarCandidate secondary = {};
  if (has_secondary)
  {
    DataQuality secondary_quality = measurement.secondaryQuality;
    if (secondary_quality == DataQuality::INVALID)
    {
      secondary_quality = measurement.primaryQuality;
    }
    secondary = buildLidarCandidate(measurement.secondaryDistance_mm,
                                    measurement.secondaryIntensity,
                                    measurement.sunlightBase,
                                    secondary_quality,
                                    true,
                                    previous_distance_cm,
                                    has_lens_prior,
                                    lens_prior_cm);
  }

  if (primary.valid || secondary.valid)
  {
    return selectBestPair(primary, secondary);
  }

  // If quality/intensity gating rejects both candidates at long range, keep a
  // low-confidence distance fallback to avoid prolonged "..." dropouts.
  LidarCandidate fallbackPrimary = buildFallbackLidarCandidate(measurement.primaryDistance_mm,
                                                               measurement.primaryIntensity,
                                                               measurement.sunlightBase,
                                                               measurement.primaryQuality,
                                                               previous_distance_cm);
  if (!has_secondary)
  {
    return fallbackPrimary;
  }

  DataQuality fb_secondary_quality = measurement.secondaryQuality;
  if (fb_secondary_quality == DataQuality::INVALID)
  {
    fb_secondary_quality = measurement.primaryQuality;
  }
  LidarCandidate fallbackSecondary = buildFallbackLidarCandidate(measurement.secondaryDistance_mm,
                                                                 measurement.secondaryIntensity,
                                                                 measurement.sunlightBase,
                                                                 fb_secondary_quality,
                                                                 previous_distance_cm);
  return selectBestPair(fallbackPrimary, fallbackSecondary);
}

bool isLidarReadingImplausible(int lidar_distance_cm, int lens_prior_cm, int quality_level)
{
  if (lens_prior_cm <= 0 || lens_prior_cm > LIDAR_PLAUSIBILITY_LENS_NEAR_CM)
  {
    return false;
  }
  if (lidar_distance_cm <= 0)
  {
    return false;
  }
  // Parallax beam-miss error grows with distance, so scale the allowed overshoot
  // with the prior, never below the near-focus floor.
  int allowance_cm = max(LIDAR_PLAUSIBILITY_MAX_OVERSHOOT_CM,
                         lens_prior_cm * LIDAR_PLAUSIBILITY_OVERSHOOT_FACTOR_PCT / 100);
  // A confident (GOOD/EXCELLENT) return earns a wider allowance — it is more
  // likely locking a real far subject than missing the framed one. But the sensor
  // grades quality AFTER distance-normalization, so a far beam-miss onto a bright
  // background can still read GOOD; keep the allowance finite so a gross overshoot
  // is rejected regardless of grade.
  if (quality_level >= LIDAR_PLAUSIBILITY_TRUST_QUALITY_LEVEL)
  {
    allowance_cm = max(allowance_cm,
                       lens_prior_cm * LIDAR_PLAUSIBILITY_TRUST_OVERSHOOT_FACTOR_PCT / 100);
  }
  return lidar_distance_cm > lens_prior_cm + allowance_cm;
}

bool updatePlausibilityHold(PlausibilityHoldState &state,
                            int reading_cm,
                            int stable_delta_cm,
                            int stable_release_frames,
                            int max_hold_frames)
{
  state.rejectedFrames++;
  if (state.lastRejectedCm > 0 && abs(reading_cm - state.lastRejectedCm) <= stable_delta_cm)
  {
    state.consistentFrames++;
  }
  else
  {
    state.consistentFrames = 1;
  }
  state.lastRejectedCm = reading_cm;

  bool settled = state.consistentFrames >= stable_release_frames;
  bool capped = state.rejectedFrames >= max_hold_frames;
  return settled || capped;
}

void resetPlausibilityHold(PlausibilityHoldState &state)
{
  state = PlausibilityHoldState{};
}

int applyStableConfidenceBoost(int base_confidence, int stable_streak_frames)
{
  if (stable_streak_frames < LIDAR_STABLE_MIN_FRAMES)
  {
    return base_confidence;
  }
  int boosted = base_confidence + LIDAR_STABLE_CONFIDENCE_BOOST;
  return min(boosted, LIDAR_STABLE_MAX_CONFIDENCE);
}

bool updateSunlightWarnState(bool currently_warning, uint16_t sunlight_base)
{
  if (currently_warning)
  {
    return sunlight_base >= LIDAR_SUNLIGHT_WARN_EXIT;
  }
  return sunlight_base >= LIDAR_SUNLIGHT_WARN_ENTER;
}

int blendLidarDistance(int previous_distance_cm, int next_distance_cm, int confidence)
{
  if (previous_distance_cm <= 0)
  {
    return next_distance_cm;
  }

  if (confidence >= LIDAR_CONFIDENCE_HIGH)
  {
    if (next_distance_cm <= LIDAR_NEAR_HIGH_CONF_BLEND_MAX_CM)
    {
      float blended = static_cast<float>(previous_distance_cm) * LIDAR_NEAR_HIGH_CONF_BLEND +
                      static_cast<float>(next_distance_cm) * (1.0f - LIDAR_NEAR_HIGH_CONF_BLEND);
      return static_cast<int>(roundf(blended));
    }
    return next_distance_cm;
  }

  if (confidence >= LIDAR_CONFIDENCE_MEDIUM)
  {
    float blended = (static_cast<float>(previous_distance_cm) * (1.0f - LIDAR_MEDIUM_CONF_BLEND)) +
                    (static_cast<float>(next_distance_cm) * LIDAR_MEDIUM_CONF_BLEND);
    return static_cast<int>(roundf(blended));
  }

  // Avoid "stuck" readings in harsh light where confidence drops but still carries signal.
  float confidence_ratio = static_cast<float>(confidence) / static_cast<float>(LIDAR_CONFIDENCE_MEDIUM);
  float blend_weight = LIDAR_LOW_CONF_BLEND_MIN +
                       ((LIDAR_LOW_CONF_BLEND_MAX - LIDAR_LOW_CONF_BLEND_MIN) * confidence_ratio);
  blend_weight = constrain(blend_weight, LIDAR_LOW_CONF_BLEND_MIN, LIDAR_LOW_CONF_BLEND_MAX);
  float blended = (static_cast<float>(previous_distance_cm) * (1.0f - blend_weight)) +
                  (static_cast<float>(next_distance_cm) * blend_weight);
  int blended_cm = static_cast<int>(roundf(blended));
  int upper_bound = previous_distance_cm + LIDAR_LOW_CONF_MAX_STEP_CM;
  int lower_bound = previous_distance_cm - LIDAR_LOW_CONF_MAX_STEP_CM;
  return constrain(blended_cm, lower_bound, upper_bound);
}

void formatDistanceDisplay(int corrected_cm, char *buffer, size_t bufferSize)
{
  if (!buffer || bufferSize == 0)
  {
    return;
  }

  if (corrected_cm <= 0)
  {
    snprintf(buffer, bufferSize, "<%dcm", LIDAR_DISPLAY_MIN_CM);
    return;
  }
  if (corrected_cm > LIDAR_DISPLAY_INF_THRESHOLD_CM)
  {
    snprintf(buffer, bufferSize, "Inf.");
    return;
  }
  if (corrected_cm < LIDAR_DISPLAY_MIN_CM)
  {
    snprintf(buffer, bufferSize, "<%dcm", LIDAR_DISPLAY_MIN_CM);
    return;
  }
  if (corrected_cm < CM_PER_METER)
  {
    snprintf(buffer, bufferSize, "%dcm", corrected_cm);
    return;
  }
  int decimalPlaces = (corrected_cm < DISTANCE_NEAR_THRESHOLD_CM)
                          ? DISTANCE_DECIMAL_PLACES_NEAR
                          : DISTANCE_DECIMAL_PLACES;
  snprintf(buffer,
           bufferSize,
           "%.*fm",
           decimalPlaces,
           static_cast<float>(corrected_cm) / static_cast<float>(CM_PER_METER));
}

const char *lidarSignalLossPlaceholder(int prev_distance_cm, const char *current_display)
{
  // clearLidarDisplay() resets prev_distance to 0 on the first dropped frame, so
  // every later frame in a continuing dropout would recompute "..." from a zeroed
  // prior and the brief "Inf?" would never be seen. The shown string carries the
  // far state across that reset: once we are already marking a far dropout ("Inf?")
  // or held a genuine far measurement ("Inf."), keep marking it until a valid
  // reading replaces the display. Standby ("Zzz") and near dropouts ("...") are
  // not far states, so a fresh decision is taken from prev_distance.
  if (current_display &&
      (strcmp(current_display, "Inf?") == 0 || strcmp(current_display, "Inf.") == 0))
  {
    return "Inf?";
  }
  return prev_distance_cm >= LIDAR_FAR_SIGNAL_LOSS_CM ? "Inf?" : "...";
}

void formatLidarTelemetryAge(uint32_t now_ms, uint32_t telemetry_ms, char *buffer, size_t bufferSize)
{
  if (!buffer || bufferSize == 0)
  {
    return;
  }

  if (telemetry_ms == 0)
  {
    snprintf(buffer, bufferSize, "--");
    return;
  }

  uint32_t age_ms = now_ms - telemetry_ms;
  if (age_ms < 1000UL)
  {
    snprintf(buffer, bufferSize, "%lums", static_cast<unsigned long>(age_ms));
    return;
  }
  if (age_ms > 99000UL)
  {
    snprintf(buffer, bufferSize, ">99s");
    return;
  }
  snprintf(buffer, bufferSize, "%.1fs", static_cast<double>(age_ms) / 1000.0);
}
