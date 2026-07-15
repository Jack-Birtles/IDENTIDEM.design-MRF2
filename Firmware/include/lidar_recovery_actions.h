#ifndef LIDAR_RECOVERY_ACTIONS_H
#define LIDAR_RECOVERY_ACTIONS_H

#include <stdint.h>

#include <DTS6012M_UART.h>

// The LiDAR recovery attempt and calibration-profile application, templated
// on the sensor type so the native suite drives them with a call-recording
// fake. This is the single implementation of both sequences; the tests pin
// the v10.4.7 invariant that recovery never touches the frame rate.

// Re-apply library-side distance correction after sensor state changes.
// Frame rate is set once at boot in initializeLidarSensor(); the sensor
// retains it across enable/disable cycles. Re-sending setFrameRate
// immediately after enableSensor() in the recovery path destabilises some
// DTS6012M units and causes a self-perpetuating recovery loop (v10.4.7).
template <typename Sensor>
void applyLidarCalibrationProfileTo(Sensor &sensor, float distance_scale, int16_t distance_offset_mm)
{
  sensor.setDistanceScale(distance_scale);
  sensor.setDistanceOffset(distance_offset_mm);
}

// One recovery attempt: clear the error latch, reset library state, re-enable
// the sensor, then re-apply the calibration profile. The profile runs
// unconditionally (it is idempotent) so a partial recovery where
// enableSensor() fails transiently while the sensor keeps streaming can never
// leave the distance offset zeroed.
template <typename Sensor, typename ApplyCalibrationProfile>
bool performLidarRecoveryAttempt(Sensor &sensor, ApplyCalibrationProfile applyCalibrationProfile)
{
  sensor.clearError();
  DTSError resetStatus = sensor.resetState();
  DTSError enableStatus = static_cast<DTSError>(sensor.enableSensor());
  applyCalibrationProfile();
  return resetStatus == DTSError::NONE && enableStatus == DTSError::NONE;
}

#endif // LIDAR_RECOVERY_ACTIONS_H
