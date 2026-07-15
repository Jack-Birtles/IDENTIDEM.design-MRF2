#ifndef PREFS_KEYS_H
#define PREFS_KEYS_H

#include <cstddef>

// Every NVS Preferences key used by the firmware. ESP32 NVS rejects keys
// longer than 15 characters and the Arduino Preferences wrapper surfaces
// that as a silent no-op (v10.3.5 shipped exactly that bug), so keys are
// centralized here and the native suite asserts the limit for each one.
// Add new keys to PREFS_ALL_STATIC_KEYS or the guard cannot see them.
constexpr size_t PREFS_NVS_MAX_KEY_LEN = 15;

constexpr const char *PREFS_KEY_SCHEMA = "schema";
constexpr const char *PREFS_KEY_LEGACY_LENSES = "lenses";
constexpr const char *PREFS_KEY_LENS_COUNT = "lc_count";

constexpr const char *PREFS_KEY_ISO = "iso";
constexpr const char *PREFS_KEY_ISO_INDEX = "iso_index";
constexpr const char *PREFS_KEY_APERTURE = "aperture";
constexpr const char *PREFS_KEY_APERTURE_INDEX = "aperture_index";
constexpr const char *PREFS_KEY_SELECTED_FORMAT = "selected_format";
constexpr const char *PREFS_KEY_SELECTED_LENS = "selected_lens";
constexpr const char *PREFS_KEY_PARALLAX = "parallax";
constexpr const char *PREFS_KEY_LENS_FOCUS_OFFSET = "lens_foc_off";
constexpr const char *PREFS_KEY_EV_COMP_THIRDS = "ev_comp_thirds";
constexpr const char *PREFS_KEY_METER_SMOOTHING = "meter_smooth";
constexpr const char *PREFS_KEY_SHOW_EV = "show_ev";
constexpr const char *PREFS_KEY_SLEEP_TIMEOUT_MODE = "sleep_to_mode";
constexpr const char *PREFS_KEY_LIDAR_IDLE_TIMEOUT = "lidar_idle_to";
constexpr const char *PREFS_KEY_LEVEL_TRIM_L10 = "lvl_trim_l10";
constexpr const char *PREFS_KEY_LEVEL_TRIM_PP10 = "lvl_trim_pp10";
constexpr const char *PREFS_KEY_LEVEL_TRIM_PN10 = "lvl_trim_pn10";
constexpr const char *PREFS_KEY_LEGACY_LEVEL_TRIM_L = "lvl_trim_l";
constexpr const char *PREFS_KEY_LEGACY_LEVEL_TRIM_PP = "lvl_trim_pp";
constexpr const char *PREFS_KEY_LEGACY_LEVEL_TRIM_PN = "lvl_trim_pn";
constexpr const char *PREFS_KEY_RETICLE_X = "reticle_x";
constexpr const char *PREFS_KEY_RETICLE_Y = "reticle_y";
constexpr const char *PREFS_KEY_LIDAR_OFFSET = "lidar_off";
constexpr const char *PREFS_KEY_BRIGHTNESS_AUTO = "bright_auto";
constexpr const char *PREFS_KEY_BRIGHTNESS_MANUAL_PCT = "bright_man_pct";
constexpr const char *PREFS_KEY_BRIGHTNESS_TOP_PCT = "bright_top_pct";
constexpr const char *PREFS_KEY_SHOW_HORIZON = "show_horizon";
constexpr const char *PREFS_KEY_FILM_COUNTER = "film_counter";
constexpr const char *PREFS_KEY_ENCODER_VALUE = "encoder_value";
constexpr const char *PREFS_KEY_PREV_ENCODER_VALUE = "prev_enc_val";
constexpr const char *PREFS_KEY_FRAME_ONE_OFFSET = "frame1_offset";
constexpr const char *PREFS_KEY_FRAME_SPACING = "frame_spacing";

// Per-lens calibration keys are generated from these printf patterns with the
// lens index; the guard test formats them with the highest real index.
constexpr const char *PREFS_KEY_PATTERN_LENS_READINGS = "lc_sr_%u";
constexpr const char *PREFS_KEY_PATTERN_LENS_CALIBRATED = "lc_ok_%u";

constexpr const char *PREFS_ALL_STATIC_KEYS[] = {
    PREFS_KEY_SCHEMA,
    PREFS_KEY_LEGACY_LENSES,
    PREFS_KEY_LENS_COUNT,
    PREFS_KEY_ISO,
    PREFS_KEY_ISO_INDEX,
    PREFS_KEY_APERTURE,
    PREFS_KEY_APERTURE_INDEX,
    PREFS_KEY_SELECTED_FORMAT,
    PREFS_KEY_SELECTED_LENS,
    PREFS_KEY_PARALLAX,
    PREFS_KEY_LENS_FOCUS_OFFSET,
    PREFS_KEY_EV_COMP_THIRDS,
    PREFS_KEY_METER_SMOOTHING,
    PREFS_KEY_SHOW_EV,
    PREFS_KEY_SLEEP_TIMEOUT_MODE,
    PREFS_KEY_LIDAR_IDLE_TIMEOUT,
    PREFS_KEY_LEVEL_TRIM_L10,
    PREFS_KEY_LEVEL_TRIM_PP10,
    PREFS_KEY_LEVEL_TRIM_PN10,
    PREFS_KEY_LEGACY_LEVEL_TRIM_L,
    PREFS_KEY_LEGACY_LEVEL_TRIM_PP,
    PREFS_KEY_LEGACY_LEVEL_TRIM_PN,
    PREFS_KEY_RETICLE_X,
    PREFS_KEY_RETICLE_Y,
    PREFS_KEY_LIDAR_OFFSET,
    PREFS_KEY_BRIGHTNESS_AUTO,
    PREFS_KEY_BRIGHTNESS_MANUAL_PCT,
    PREFS_KEY_BRIGHTNESS_TOP_PCT,
    PREFS_KEY_SHOW_HORIZON,
    PREFS_KEY_FILM_COUNTER,
    PREFS_KEY_ENCODER_VALUE,
    PREFS_KEY_PREV_ENCODER_VALUE,
    PREFS_KEY_FRAME_ONE_OFFSET,
    PREFS_KEY_FRAME_SPACING,
};

constexpr size_t PREFS_STATIC_KEY_COUNT =
    sizeof(PREFS_ALL_STATIC_KEYS) / sizeof(PREFS_ALL_STATIC_KEYS[0]);

#endif
