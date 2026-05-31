#include "frameline_layout_logic.h"

#include <math.h>

namespace
{
int clampMinOne(int v) { return v < 1 ? 1 : v; }
int clampMaxAndMinOne(int v, int upper) { return clampMinOne(v < upper ? v : upper); }
} // namespace

FramelineDimensions scaleFramelineToFormat(int baseWidth,
                                           int baseHeight,
                                           float formatWidthMm,
                                           float formatHeightMm,
                                           float baseFormatWidthMm,
                                           float baseFormatHeightMm)
{
  FramelineDimensions result = {baseWidth, baseHeight};

  if (formatWidthMm <= 0.0f || formatHeightMm <= 0.0f)
  {
    result.width = clampMaxAndMinOne(result.width, baseWidth);
    result.height = clampMaxAndMinOne(result.height, baseHeight);
    return result;
  }

  const float formatRatio = formatWidthMm / formatHeightMm;
  const float baseRatio = static_cast<float>(baseWidth) / static_cast<float>(baseHeight);

  bool allowOverflow = false;
  if (baseFormatWidthMm > 0.0f && baseFormatHeightMm > 0.0f)
  {
    const float baseFormatRatio = baseFormatWidthMm / baseFormatHeightMm;
    allowOverflow = formatRatio > baseFormatRatio && formatHeightMm >= baseFormatHeightMm;
  }

  if (allowOverflow)
  {
    result.height = baseHeight;
    result.width = static_cast<int>(roundf(baseHeight * formatRatio));
    result.width = clampMinOne(result.width);
    result.height = clampMaxAndMinOne(result.height, baseHeight);
    return result;
  }

  if (formatRatio >= baseRatio)
  {
    result.width = baseWidth;
    result.height = static_cast<int>(roundf(baseWidth / formatRatio));
  }
  else
  {
    result.height = baseHeight;
    result.width = static_cast<int>(roundf(baseHeight * formatRatio));
  }
  result.width = clampMaxAndMinOne(result.width, baseWidth);
  result.height = clampMaxAndMinOne(result.height, baseHeight);
  return result;
}
