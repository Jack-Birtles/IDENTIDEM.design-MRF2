#include "ui_signature_logic.h"

#include <string.h>

uint32_t hashUint32(uint32_t hash, uint32_t value)
{
  hash ^= value;
  hash *= HASH_PRIME;
  return hash;
}

uint32_t hashInt(uint32_t hash, int value)
{
  return hashUint32(hash, static_cast<uint32_t>(value));
}

uint32_t hashBool(uint32_t hash, bool value)
{
  return hashUint32(hash, value ? 1u : 0u);
}

uint32_t hashFloat(uint32_t hash, float value)
{
  uint32_t bits = 0;
  memcpy(&bits, &value, sizeof(bits));
  return hashUint32(hash, bits);
}

uint32_t hashCString(uint32_t hash, const char *value)
{
  const char *raw = value ? value : "";
  size_t len = strlen(raw);
  hash = hashUint32(hash, static_cast<uint32_t>(len));
  for (size_t i = 0; i < len; i++)
  {
    hash = hashUint32(hash, static_cast<uint8_t>(raw[i]));
  }
  return hash;
}

uint32_t buildMainUiSignature(const MainUiSnapshot &s)
{
  uint32_t hash = HASH_OFFSET_BASIS;
  hash = hashInt(hash, s.ui_mode);
  hash = hashInt(hash, s.selected_lens);
  hash = hashInt(hash, s.selected_format);
  hash = hashInt(hash, s.iso);
  hash = hashFloat(hash, s.aperture);
  hash = hashBool(hash, s.show_ev_readout);
  hash = hashFloat(hash, s.ev_readout);
  hash = hashCString(hash, s.shutter_speed);
  hash = hashCString(hash, s.distance_cm);
  hash = hashCString(hash, s.lens_distance_cm);
  hash = hashInt(hash, s.distance);
  hash = hashInt(hash, s.lens_distance_raw);
  hash = hashInt(hash, s.lidar_quality_level);
  hash = hashBool(hash, s.lidar_high_sunlight);
  hash = hashBool(hash, s.parallaxEnabled);
  hash = hashInt(hash, s.reticle_offset_x);
  hash = hashInt(hash, s.reticle_offset_y);
  hash = hashBool(hash, s.show_horizon_line);
  return hash;
}

uint32_t buildMenuUiSignature(const MenuUiSnapshot &s)
{
  uint32_t hash = HASH_OFFSET_BASIS;
  hash = hashInt(hash, s.ui_mode);
  hash = hashInt(hash, s.config_step);
  hash = hashInt(hash, s.calib_step);
  hash = hashInt(hash, s.selected_lens);
  hash = hashInt(hash, s.selected_format);
  hash = hashInt(hash, s.calib_lens);
  hash = hashInt(hash, s.current_calib_distance);
  hash = hashInt(hash, s.calib_capture_status);
  hash = hashUint32(hash, static_cast<uint32_t>(s.calib_capture_status_ms));
  hash = hashInt(hash, s.lens_sensor_reading);
  hash = hashInt(hash, s.iso);
  hash = hashFloat(hash, s.aperture);
  hash = hashInt(hash, s.exposure_comp_thirds);
  hash = hashInt(hash, s.meter_smoothing_mode);
  hash = hashBool(hash, s.show_ev_readout);
  hash = hashBool(hash, s.parallaxEnabled);
  hash = hashInt(hash, s.sleep_timeout_mode);
  hash = hashInt(hash, s.lidar_idle_timeout_mode);
  hash = hashInt(hash, s.level_trim_landscape_deci_deg);
  hash = hashInt(hash, s.level_trim_portrait_pos_deci_deg);
  hash = hashInt(hash, s.level_trim_portrait_neg_deci_deg);
  hash = hashInt(hash, s.reticle_offset_x);
  hash = hashInt(hash, s.reticle_offset_y);
  hash = hashInt(hash, s.reticle_adjust_step);
  hash = hashBool(hash, s.brightness_auto);
  hash = hashInt(hash, s.brightness_manual_pct);
  hash = hashInt(hash, s.brightness_auto_top_pct);
  hash = hashBool(hash, s.show_horizon_line);
  hash = hashInt(hash, s.frame_one_offset);
  hash = hashInt(hash, s.frame_spacing_offset);
  hash = hashInt(hash, s.film_counter);
  hash = hashInt(hash, s.last_lidar_error_code);
  hash = hashInt(hash, s.lidar_recovery_count);
  hash = hashBool(hash, s.lidarEnabled);
  hash = hashBool(hash, s.adsReady);
  hash = hashBool(hash, s.mpuReady);
  hash = hashBool(hash, s.mainDisplayReady);
  hash = hashBool(hash, s.externalDisplayReady);
  hash = hashBool(hash, s.batteryGaugeReady);
  hash = hashBool(hash, s.lightMeterReady);
  hash = hashBool(hash, s.statusPixelReady);
  hash = hashBool(hash, s.encoderReady);
  hash = hashBool(hash, s.lidarSensorReady);
  hash = hashBool(hash, s.prefsSchemaValid);
  hash = hashBool(hash, s.prefsLoadedLegacy);
  hash = hashInt(hash, s.prefsSchemaVersionLoaded);
  return hash;
}

uint32_t buildExternalUiSignature(const ExternalUiSnapshot &s)
{
  uint32_t hash = HASH_OFFSET_BASIS;
  hash = hashInt(hash, s.selected_format);
  hash = hashInt(hash, s.selected_lens);
  hash = hashInt(hash, s.bat_per);
  hash = hashInt(hash, s.film_counter);
  hash = hashInt(hash, static_cast<int>(s.frame_progress * 1000.0f));
  hash = hashBool(hash, s.sleepMode);
  return hash;
}
