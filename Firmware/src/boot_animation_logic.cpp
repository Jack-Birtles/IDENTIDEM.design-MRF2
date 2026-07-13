#include "boot_animation_logic.h"

int bootTextXForFrame(int frame, int totalFrames, int displayWidth, int textWidth)
{
  const int centered = (displayWidth - textWidth) / 2;
  if (totalFrames <= 1)
  {
    return centered;
  }
  if (frame < 0)
  {
    frame = 0;
  }
  if (frame > totalFrames - 1)
  {
    frame = totalFrames - 1;
  }

  const long last = totalFrames - 1;
  const int start = displayWidth;            // fully off the right edge
  const long span = (long)(centered - start); // negative: text moves left

  // Ease-out: eased = 1 - (1 - p)^2, with p = frame / last.
  // easedNumerator / (last*last) == eased, kept in integer math.
  const long remaining = last - frame;
  const long easedNumerator = last * last - remaining * remaining;

  return start + (int)(span * easedNumerator / (last * last));
}

int bootSprocketOffsetForFrame(int frame, int stepPx, int spacingPx)
{
  if (spacingPx <= 0)
  {
    return 0;
  }
  long off = ((long)frame * stepPx) % spacingPx;
  if (off < 0)
  {
    off += spacingPx;
  }
  return (int)off;
}
