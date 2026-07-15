#include "prefs_clamp_logic.h"

#include "formats.h"
#include "lenses.h"
#include "mrfconstants.h"

namespace
{
int clampInt(int value, int minVal, int maxVal)
{
  if (value < minVal)
  {
    return minVal;
  }
  if (value > maxVal)
  {
    return maxVal;
  }
  return value;
}

int firstNonZeroApertureIndex(int lens_index)
{
  for (int i = 0; i < LENS_APERTURE_COUNT; i++)
  {
    if (lenses[lens_index].apertures[i] != 0)
    {
      return i;
    }
  }
  return 0;
}

int snapLevelTrimDeciDeg(int value)
{
  int clamped = clampInt(value, LEVEL_TRIM_MIN_DECI_DEG, LEVEL_TRIM_MAX_DECI_DEG);
  int normalized = clamped - LEVEL_TRIM_MIN_DECI_DEG;
  int snappedSteps = (normalized + (LEVEL_TRIM_STEP_DECI_DEG / 2)) / LEVEL_TRIM_STEP_DECI_DEG;
  return LEVEL_TRIM_MIN_DECI_DEG + (snappedSteps * LEVEL_TRIM_STEP_DECI_DEG);
}
} // namespace

void clampLoadedPrefsState(LoadedPrefsState &s)
{
  if (s.iso_index < 0 || s.iso_index >= static_cast<int>(sizeof(ISOS) / sizeof(ISOS[0])))
  {
    s.iso_index = DEFAULT_ISO_INDEX;
  }
  s.iso = ISOS[s.iso_index];

  if (s.selected_lens < 0 || s.selected_lens >= static_cast<int>(NUM_LENSES))
  {
    s.selected_lens = (DEFAULT_SELECTED_LENS >= 0 && DEFAULT_SELECTED_LENS < static_cast<int>(NUM_LENSES))
                          ? DEFAULT_SELECTED_LENS
                          : 0;
  }

  if (s.selected_format < 0 || s.selected_format >= static_cast<int>(NUM_FILM_FORMATS))
  {
    s.selected_format = (DEFAULT_SELECTED_FORMAT >= 0 && DEFAULT_SELECTED_FORMAT < static_cast<int>(NUM_FILM_FORMATS))
                            ? DEFAULT_SELECTED_FORMAT
                            : 0;
  }

  if (s.aperture_index < 0 || s.aperture_index >= LENS_APERTURE_COUNT ||
      lenses[s.selected_lens].apertures[s.aperture_index] == 0)
  {
    s.aperture_index = firstNonZeroApertureIndex(s.selected_lens);
  }
  s.aperture = lenses[s.selected_lens].apertures[s.aperture_index];

  if (s.film_counter < 0)
  {
    s.film_counter = 0;
  }
  if (s.encoder_value < 0)
  {
    s.encoder_value = 0;
  }
  if (s.prev_encoder_value < 0)
  {
    s.prev_encoder_value = 0;
  }

  s.exposure_comp_thirds = clampInt(s.exposure_comp_thirds,
                                    LIGHTMETER_EV_COMP_MIN_THIRDS,
                                    LIGHTMETER_EV_COMP_MAX_THIRDS);
  s.meter_smoothing_mode = clampInt(s.meter_smoothing_mode,
                                    LIGHTMETER_SMOOTHING_MODE_MIN,
                                    LIGHTMETER_SMOOTHING_MODE_MAX);
  s.sleep_timeout_mode = clampInt(s.sleep_timeout_mode,
                                  SLEEP_TIMEOUT_MODE_MIN,
                                  SLEEP_TIMEOUT_MODE_MAX);
  s.lidar_idle_timeout_mode = clampInt(s.lidar_idle_timeout_mode,
                                       SLEEP_TIMEOUT_MODE_MIN,
                                       SLEEP_TIMEOUT_MODE_MAX);

  s.level_trim_landscape_deci_deg = snapLevelTrimDeciDeg(s.level_trim_landscape_deci_deg);
  s.level_trim_portrait_pos_deci_deg = snapLevelTrimDeciDeg(s.level_trim_portrait_pos_deci_deg);
  s.level_trim_portrait_neg_deci_deg = snapLevelTrimDeciDeg(s.level_trim_portrait_neg_deci_deg);

  s.reticle_offset_x = clampInt(s.reticle_offset_x, RETICLE_OFFSET_MIN, RETICLE_OFFSET_MAX);
  s.reticle_offset_y = clampInt(s.reticle_offset_y, RETICLE_OFFSET_MIN, RETICLE_OFFSET_MAX);
  s.brightness_manual_pct = clampInt(s.brightness_manual_pct, BRIGHTNESS_MANUAL_MIN_PCT, BRIGHTNESS_PCT_MAX);
  s.brightness_auto_top_pct = clampInt(s.brightness_auto_top_pct, BRIGHTNESS_AUTO_TOP_MIN_PCT, BRIGHTNESS_PCT_MAX);

  s.frame_one_offset = clampInt(s.frame_one_offset, FRAME_TUNING_MIN, FRAME_TUNING_MAX);
  s.frame_spacing_offset = clampInt(s.frame_spacing_offset, FRAME_TUNING_MIN, FRAME_TUNING_MAX);
  s.lens_focus_offset = clampInt(s.lens_focus_offset, LENS_FOCUS_OFFSET_MIN, LENS_FOCUS_OFFSET_MAX);
}
