#ifndef PREFS_CLAMP_LOGIC_H
#define PREFS_CLAMP_LOGIC_H

// Sanitizes values loaded from NVS before anything indexes ISOS[], lenses[],
// film_formats[], or the meter smoothing tables with them. This is the only
// defense against corrupted or legacy preferences; the native suite drives it
// with corrupted inputs. helpers.cpp copies globals in and out.

struct LoadedPrefsState
{
  int iso_index;
  int iso;
  int selected_lens;
  int selected_format;
  int aperture_index;
  float aperture;
  int film_counter;
  int encoder_value;
  int prev_encoder_value;
  int exposure_comp_thirds;
  int meter_smoothing_mode;
  int sleep_timeout_mode;
  int lidar_idle_timeout_mode;
  int level_trim_landscape_deci_deg;
  int level_trim_portrait_pos_deci_deg;
  int level_trim_portrait_neg_deci_deg;
  int reticle_offset_x;
  int reticle_offset_y;
  int brightness_manual_pct;
  int brightness_auto_top_pct;
  int frame_one_offset;
  int frame_spacing_offset;
  int lens_focus_offset;
};

void clampLoadedPrefsState(LoadedPrefsState &state);

#endif // PREFS_CLAMP_LOGIC_H
