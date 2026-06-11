#ifndef GLOBALS_H
#define GLOBALS_H

#include <Preferences.h> // For Preferences object type
#include <stdint.h>
#include "mrfconstants.h" // For CALIB_DISTANCE_COUNT

// All cross-module mutable state for the running firmware. Group by domain
// (Lightmeter / Lens / LiDAR / Battery / UI mode / Calibration / Frame counter
// / Sleep / Health diagnostics). Module-private caches that are touched by
// exactly one .cpp file (e.g. moving-average buffers, prev-reading caches)
// live as file-scope statics in that .cpp instead.

enum class UiMode : uint8_t
{
  Main,
  Config,
  ConfigFilm,
  ConfigFrameTuning,  // Film > Frame counter tuning sub-submenu.
  ConfigLens,
  ConfigMeter,
  ConfigLidar,        // LiDAR submenu (distance offset, idle timeout).
  ConfigDisplay,      // Display submenu (formerly "UI Settings").
  ConfigHorizonTrim,  // Display > Horizon trim sub-submenu.
  Calib,
  ResetConfirm,
  Health,
  FactoryResetConfirm,
  ReticleAdjust,
  LidarDiagnostics    // Setup > LiDAR > Diagnostics live telemetry screen.
};

extern Preferences prefs;

// ---------------------------------------------------------------------------
// Lightmeter
// ---------------------------------------------------------------------------
extern int prev_iso;
extern int iso;
extern float prev_aperture;
extern float aperture;
extern float lux;
extern float ev_readout;
extern char shutter_speed[16];
extern int iso_index;
extern int aperture_index;
extern int exposure_comp_thirds;
extern int meter_smoothing_mode;
extern bool show_ev_readout;

// ---------------------------------------------------------------------------
// Lens distance
// ---------------------------------------------------------------------------
// (prev_lens_sensor_reading is a setfuncs-internal cache; not exposed here.)
extern int lens_sensor_reading;
extern int lens_distance_raw;
extern char lens_distance_cm[16];

// ---------------------------------------------------------------------------
// LiDAR distance
// ---------------------------------------------------------------------------
// (prev_distance is a setfuncs-internal cache; not exposed here.)
extern int16_t distance;            // Distance to object in centimetres.
extern char distance_cm[16];
extern int lidar_quality_level;     // 0..4 (none, poor..excellent).
extern bool lidarEnabled;
extern bool lidar_high_sunlight;
extern bool lidar_distance_held;    // True when the plausibility gate is holding a stale reading.
extern int lidar_distance_offset_mm;

// Live LiDAR telemetry for the diagnostics screen (last accepted measurement).
extern uint16_t lidar_raw_distance_mm;   // Raw primary distance from the sensor (pre-calibration).
extern uint16_t lidar_primary_intensity; // Primary return intensity.
extern uint16_t lidar_sunlight_base;     // Ambient-light baseline reported by the sensor.
extern int lidar_snr_permille;           // computeSnrPermille() of the last measurement (-1 if no baseline).
extern unsigned long lidar_telemetry_ms; // millis() when the telemetry above was captured (0 = no frame yet).
extern uint16_t lidar_frame_rate_actual; // Frame rate read back from the sensor at boot (0 if unread).

// ---------------------------------------------------------------------------
// Battery gauge
// ---------------------------------------------------------------------------
extern int bat_per;

// ---------------------------------------------------------------------------
// UI / system settings (persisted)
// ---------------------------------------------------------------------------
extern int sleep_timeout_mode;
extern int lidar_idle_timeout_mode;
extern int level_trim_landscape_deci_deg;
extern int level_trim_portrait_pos_deci_deg;
extern int level_trim_portrait_neg_deci_deg;
extern int reticle_offset_x;
extern int reticle_offset_y;
extern bool brightness_auto;
extern int brightness_manual_pct;
extern int brightness_auto_top_pct;
extern bool show_horizon_line;
extern bool parallaxEnabled;

// ---------------------------------------------------------------------------
// UI mode / navigation state
// ---------------------------------------------------------------------------
extern UiMode ui_mode;
extern int config_step;
extern int calib_step;
extern int reticle_adjust_step;     // 0 = horizontal, 1 = vertical.
extern int selected_lens;
extern int selected_format;
extern int calib_lens;

// ---------------------------------------------------------------------------
// Calibration capture
// ---------------------------------------------------------------------------
extern int calib_distance_set[CALIB_DISTANCE_COUNT];
extern int current_calib_distance;
extern int calib_capture_status;
extern unsigned long calib_capture_status_ms;

// ---------------------------------------------------------------------------
// Frame counter
// ---------------------------------------------------------------------------
extern int film_counter;
extern int prev_encoder_value;
extern int encoder_value;
extern float frame_progress;
extern float prev_frame_progress;
extern int frame_one_offset;
extern int frame_spacing_offset;

// ---------------------------------------------------------------------------
// Sleep / activity
// ---------------------------------------------------------------------------
extern unsigned long lastActivityTime;
extern bool sleepMode;

// ---------------------------------------------------------------------------
// Health / diagnostics
// ---------------------------------------------------------------------------
extern bool prefsSchemaValid;
extern bool prefsLoadedLegacy;
extern uint16_t prefsSchemaVersionLoaded;
extern int last_lidar_error_code;
extern int lidar_recovery_count;
extern char lidar_sensor_fw_version[20];

// Per-peripheral readiness flags set during hardware.cpp init. Grouped here
// so adding a new sensor is one struct field and one initialiser, and so
// future Health-screen / boot-report code has a natural place to iterate.
struct HardwareReadiness
{
  bool ads = false;
  bool mpu = false;
  bool mainDisplay = false;
  bool externalDisplay = false;
  bool batteryGauge = false;
  bool lightMeter = false;
  bool statusPixel = false;
  bool encoder = false;
  bool lidarSensor = false;
};
extern HardwareReadiness hardware;

#endif // GLOBALS_H
