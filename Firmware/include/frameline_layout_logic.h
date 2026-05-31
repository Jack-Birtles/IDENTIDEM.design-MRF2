#ifndef FRAMELINE_LAYOUT_LOGIC_H
#define FRAMELINE_LAYOUT_LOGIC_H

struct FramelineDimensions
{
  int width;
  int height;
};

// Scale a lens's base frameline (in display pixels) to match the current film
// format's aspect ratio, given the physical mm dimensions of both the current
// format and the reference (lens-calibrated) base format.
//
// Rule:
//   * When the current format is wider than the base AND at least as tall, the
//     frameline is allowed to overflow horizontally: keep the base height,
//     scale the width to match the current format's aspect ratio (the result
//     may exceed baseWidth).
//   * Otherwise the frameline is fit inside the base box, preserving the
//     current format's aspect ratio. The constraint is on width when the
//     format is wider than the base in pixel ratio, on height otherwise.
//   * Each axis is clamped to at least 1. Height is always clamped to
//     baseHeight; width is clamped to baseWidth only in the non-overflow case.
//
// If either format's mm dimensions are non-positive, the base dimensions are
// returned unchanged.
FramelineDimensions scaleFramelineToFormat(int baseWidth,
                                           int baseHeight,
                                           float formatWidthMm,
                                           float formatHeightMm,
                                           float baseFormatWidthMm,
                                           float baseFormatHeightMm);

#endif // FRAMELINE_LAYOUT_LOGIC_H
