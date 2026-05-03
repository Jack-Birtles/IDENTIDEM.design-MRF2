#ifndef LIDAR_LOGIC_H
#define LIDAR_LOGIC_H

#include <stddef.h>
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

// True when the LiDAR reading is so far beyond the lens focus prior that it is
// almost certainly a beam-miss (LiDAR found distant background past the framed
// subject). Asymmetric: only flags overshoot, not undershoot. Returns false if
// the lens is focused beyond the near-range gate threshold.
bool isLidarReadingImplausible(int lidar_distance_cm, int lens_prior_cm);

// Add a confidence boost once the LiDAR has locked onto a stable subject
// (consecutive frames within LIDAR_STABLE_DELTA_CM of each other). Caller
// tracks the streak count; this function applies the boost and clamps the
// result so a marginal reading cannot be promoted to excellent.
int applyStableConfidenceBoost(int base_confidence, int stable_streak_frames);

void formatDistanceDisplay(int corrected_cm, char *buffer, size_t bufferSize);

#endif // LIDAR_LOGIC_H
