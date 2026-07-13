#ifndef BOOT_ANIMATION_LOGIC_H
#define BOOT_ANIMATION_LOGIC_H

// Pure geometry for the external-display film-advance boot animation.
// No Arduino/Adafruit types so it can be unit-tested on the host.

// Eased left-edge x (px) of the version text for a given animation frame.
// The film feeds in from the right: frame 0 starts fully off-screen
// (x == displayWidth) and the sequence eases out to the centered x on the
// final frame (frame == totalFrames - 1). Monotonically non-increasing.
// frame is clamped to [0, totalFrames - 1]; totalFrames <= 1 returns centered.
int bootTextXForFrame(int frame, int totalFrames, int displayWidth, int textWidth);

// Wrapped x-offset (0..spacingPx-1) of the sprocket perforations for a frame,
// advancing by stepPx each frame so the film reads as moving left.
// spacingPx <= 0 returns 0.
int bootSprocketOffsetForFrame(int frame, int stepPx, int spacingPx);

#endif // BOOT_ANIMATION_LOGIC_H
