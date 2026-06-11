#ifndef LIDAR_LOGIC_H
#define LIDAR_LOGIC_H

#include <stddef.h>
#include <stdint.h>
#include <DTS6012M_UART.h>

struct LidarCandidate
{
  bool valid;
  int distance_cm;
  int confidence;
  int quality_level; // 1..4 (poor..excellent), 0 when invalid
};

LidarCandidate chooseBestLidarCandidate(const DTSMeasurement &measurement,
                                        int previous_distance_cm,
                                        bool has_lens_prior,
                                        int lens_prior_cm);

int blendLidarDistance(int previous_distance_cm, int next_distance_cm, int confidence);

// SNR in permille: primary intensity relative to the sensor's ambient-light
// baseline (sunlightBase). Returns -1 when there is no baseline (sunlightBase
// == 0), signalling the caller to skip SNR-based logic. Exposed for the
// diagnostics screen and unit tests.
int computeSnrPermille(uint16_t intensity, uint16_t sunlight_base);

// True when the LiDAR reading is so far beyond the lens focus prior that it is
// almost certainly a beam-miss (LiDAR found distant background past the framed
// subject). Asymmetric: only flags overshoot, not undershoot. Returns false
// when: the lens is focused beyond the near-range gate threshold, no prior is
// available, or the sensor reports the reading at LIDAR_PLAUSIBILITY_TRUST_
// QUALITY_LEVEL or better (a confident return is trusted). The allowed overshoot
// scales with the prior, since parallax beam-miss error grows with distance.
bool isLidarReadingImplausible(int lidar_distance_cm, int lens_prior_cm, int quality_level);

// Running state for the plausibility-gate hold. The gate rejects readings that
// overshoot the lens prior and holds the previous value; this tracks how long
// the current overshoot has persisted and whether the rejected readings are
// settling on a consistent value (a deliberate re-aim) or are jumpy (a true
// beam-miss past the subject).
struct PlausibilityHoldState
{
  int rejectedFrames = 0;   // consecutive implausible frames in this hold
  int consistentFrames = 0; // consecutive implausible frames agreeing with the last
  int lastRejectedCm = 0;   // distance of the previous implausible reading (0 = none yet)
};

// Feed one implausible reading into the hold state and decide whether to release
// (accept) it. Releases when the rejected readings have settled within
// stable_delta_cm for stable_release_frames in a row (deliberate re-aim at a
// real far subject), or when max_hold_frames is reached (safety cap so a noisy
// beam-miss can never pin the readout to a stale value forever).
bool updatePlausibilityHold(PlausibilityHoldState &state,
                            int reading_cm,
                            int stable_delta_cm,
                            int stable_release_frames,
                            int max_hold_frames);

// Clear the hold state. Call when a plausible reading arrives.
void resetPlausibilityHold(PlausibilityHoldState &state);

// Add a confidence boost once the LiDAR has locked onto a stable subject
// (consecutive frames within LIDAR_STABLE_DELTA_CM of each other). Caller
// tracks the streak count; this function applies the boost and clamps the
// result so a marginal reading cannot be promoted to excellent.
int applyStableConfidenceBoost(int base_confidence, int stable_streak_frames);

// Hysteresis-based update for the "ambient IR is high enough to degrade the
// LiDAR" warning state. Caller passes the current state and the sensor's
// sunlight base; returns the next state. Stays on between EXIT and ENTER
// thresholds so the indicator does not flicker.
bool updateSunlightWarnState(bool currently_warning, uint16_t sunlight_base);

void formatDistanceDisplay(int corrected_cm, char *buffer, size_t bufferSize);

// Format the age of the last LiDAR telemetry frame for the diagnostics screen:
// "--" when no frame has been captured yet (telemetry_ms == 0 sentinel),
// "NNNms" under one second, "N.Ns" up to 99s, ">99s" beyond. Takes uint32_t so
// the unsigned subtraction survives a millis() wrap on target and host alike.
void formatLidarTelemetryAge(uint32_t now_ms, uint32_t telemetry_ms, char *buffer, size_t bufferSize);

#endif // LIDAR_LOGIC_H
