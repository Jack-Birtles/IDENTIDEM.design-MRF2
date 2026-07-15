#ifndef LIDAR_RECOVERY_LOGIC_H
#define LIDAR_RECOVERY_LOGIC_H

#include <Arduino.h>
#include <DTS6012M_UART.h>

enum class LidarRecoveryEvent
{
  VALID_MEASUREMENT = 0,
  NO_VALID_MEASUREMENT = 1,
  TIMEOUT = 2,
  ERROR = 3
};

// Translate an update() DTSError into a recovery event. NO_NEW_DATA (library
// v2.6.0+) is the normal "no complete frame this poll" case and must be treated
// like a benign TIMEOUT, never as a fault — see the .cpp for why.
LidarRecoveryEvent lidarRecoveryEventForUpdateError(DTSError update_error);

struct LidarRecoveryState
{
  bool initialized;
  bool recovering;
  int consecutive_errors;
  unsigned long last_valid_measurement_ms;
  // Last time any well-formed frame arrived, accepted or not. TIMEOUT
  // escalation keys off this: frames whose candidates are gated out (sky,
  // >18m, hard sun) prove the link is healthy, so they must not let no-frame
  // polls escalate to recovery. Display staleness keeps keying off
  // last_valid_measurement_ms.
  unsigned long last_frame_ms;
  unsigned long next_recovery_attempt_ms;
};

struct LidarRecoveryDecision
{
  bool clear_display;
  bool attempt_recovery;
};

void resetLidarRecoveryState(LidarRecoveryState &state, unsigned long now_ms);
LidarRecoveryDecision updateLidarRecoveryState(
    LidarRecoveryState &state, LidarRecoveryEvent event, unsigned long now_ms);
void noteLidarRecoveryAttemptResult(LidarRecoveryState &state, bool success, unsigned long now_ms);

#endif // LIDAR_RECOVERY_LOGIC_H
