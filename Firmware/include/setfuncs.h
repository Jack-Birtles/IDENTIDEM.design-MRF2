#ifndef SETFUNCS_H
#define SETFUNCS_H

// Functions to read values from sensors and set variables
// ---------------------
void setDistance();

int getLensSensorReading();

void setLensDistance();

void setFilmCounter();

void setVoltage();

void setLightMeter();

// Drop the smoothed-lux EMA state so the next reading starts fresh. Called on
// light-meter wake: ambient light can change arbitrarily while the camera
// sleeps, and blending pre-sleep lux into the first post-wake readings skews
// the displayed shutter speed for the ~1.5 s the EMA needs to converge.
void resetLightMeterSmoothing();

void toggleLidar(bool lidarStatus);

void retryLidarInit();

void applyLidarCalibrationProfile();

void clearLidarDisplay(const char *placeholder);

// Call after a deliberately blocking UI section (long delay() stretches) so
// the starved LiDAR UART is drained and its overflow state cleared before the
// next scheduled poll.
void recoverLidarAfterBlockingUi();
// ---------------------

#endif // SETFUNCS_H