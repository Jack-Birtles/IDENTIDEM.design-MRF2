#ifndef UI_SIGNATURE_LOGIC_H
#define UI_SIGNATURE_LOGIC_H

#include <stdint.h>
#include <stddef.h>

// FNV-1a hash primitives used by the UI redraw-skip cache. The cache compares
// signatures to decide whether anything user-visible has changed since the last
// draw; if not, the draw is skipped. The primitives are deterministic, so two
// equal snapshots always produce the same signature.

constexpr uint32_t HASH_OFFSET_BASIS = 2166136261u;
constexpr uint32_t HASH_PRIME = 16777619u;

uint32_t hashUint32(uint32_t hash, uint32_t value);
uint32_t hashInt(uint32_t hash, int value);
uint32_t hashBool(uint32_t hash, bool value);
uint32_t hashFloat(uint32_t hash, float value);
// Treats NULL as the empty string. Length-prefixed so "ab"+"c" and "a"+"bc"
// produce different hashes.
uint32_t hashCString(uint32_t hash, const char *value);

// Snapshots of the global state each builder hashes. The call site populates
// the snapshot from globals; the builder is pure. Keeping these as plain
// aggregates so test code can use designated initialisers.

struct MainUiSnapshot
{
  int ui_mode;
  int selected_lens;
  int selected_format;
  int iso;
  float aperture;
  bool show_ev_readout;
  float ev_readout;
  const char *shutter_speed;
  const char *distance_cm;
  const char *lens_distance_cm;
  int distance;
  int lens_distance_raw;
  int lidar_quality_level;
  bool lidar_high_sunlight;
  bool parallaxEnabled;
  int reticle_offset_x;
  int reticle_offset_y;
  bool show_horizon_line;
};

struct MenuUiSnapshot
{
  int ui_mode;
  int config_step;
  int calib_step;
  int selected_lens;
  int selected_format;
  int calib_lens;
  int current_calib_distance;
  int calib_capture_status;
  unsigned long calib_capture_status_ms;
  int lens_sensor_reading;
  int lens_focus_offset;
  int iso;
  float aperture;
  int exposure_comp_thirds;
  int meter_smoothing_mode;
  bool show_ev_readout;
  bool parallaxEnabled;
  int sleep_timeout_mode;
  int lidar_idle_timeout_mode;
  int lidar_distance_offset_mm;
  int level_trim_landscape_deci_deg;
  int level_trim_portrait_pos_deci_deg;
  int level_trim_portrait_neg_deci_deg;
  int reticle_offset_x;
  int reticle_offset_y;
  int reticle_adjust_step;
  bool brightness_auto;
  int brightness_manual_pct;
  int brightness_auto_top_pct;
  bool show_horizon_line;
  int frame_one_offset;
  int frame_spacing_offset;
  int film_counter;
  int last_lidar_error_code;
  int lidar_recovery_count;
  bool lidarEnabled;
  bool adsReady;
  bool mpuReady;
  bool mainDisplayReady;
  bool externalDisplayReady;
  bool batteryGaugeReady;
  bool lightMeterReady;
  bool statusPixelReady;
  bool encoderReady;
  bool lidarSensorReady;
  bool prefsSchemaValid;
  bool prefsLoadedLegacy;
  int prefsSchemaVersionLoaded;
};

struct ExternalUiSnapshot
{
  int selected_format;
  int selected_lens;
  int bat_per;
  int film_counter;
  float frame_progress;
  bool sleepMode;
};

uint32_t buildMainUiSignature(const MainUiSnapshot &snapshot);
uint32_t buildMenuUiSignature(const MenuUiSnapshot &snapshot);
uint32_t buildExternalUiSignature(const ExternalUiSnapshot &snapshot);

#endif // UI_SIGNATURE_LOGIC_H
