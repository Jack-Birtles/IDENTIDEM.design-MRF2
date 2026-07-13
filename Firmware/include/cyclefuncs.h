#ifndef CYCLEFUNCS_H
#define CYCLEFUNCS_H

enum class CycleDirection
{
  Up,
  Down
};

// Functions to cycle values
// ---------------------
void cycleApertures(CycleDirection direction);
void cycleISOs();
void cycleLenses();
// Clamp aperture_index/aperture to the currently selected_lens's valid range.
// Call after selected_lens changes through any path other than cycleLenses()
// (which already does this internally).
void clampApertureToSelectedLens();
void cycleCalibLenses();
void cycleFormats();
void cycleCurrentFrame();
void cycleFrameOneOffset();
void cycleLensFocusOffset();
void cycleFrameSpacingOffset();
void cycleExposureCompensation(CycleDirection direction);
void cycleMeterSmoothing();
void toggleEvReadout();
void cycleSleepTimeoutMode();
void cycleLidarIdleTimeoutMode();
void cycleLidarDistanceOffset();
void cycleLevelTrimLandscape();
void cycleLevelTrimPortraitPos();
void cycleLevelTrimPortraitNeg();
void toggleHorizonLine();
void cycleBrightnessMode();
void cycleBrightnessValue();
const char *getSleepTimeoutModeLabel(int timeout_mode);
unsigned long getSleepTimeoutModeMs(int timeout_mode);
// ---------------------

#endif // CYCLEFUNCS_H
