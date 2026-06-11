// Configuration / calibration / health / reset screens. Split out of
// interface.cpp so the main UI rendering (the high-traffic path) and the
// menu-style screens (lower-traffic but more numerous) live in separate
// files. Shared menu helpers are declared in interface_menu_helpers.h.

#include "interface.h"

#include <Arduino.h>

#include "cyclefuncs.h"
#include "formats.h"
#include "globals.h"
#include "hardware.h"
#include "helpers.h"
#include "interface_layout_constants.h"
#include "interface_menu_helpers.h"
#include "lenses.h"
#include "lens_logic.h"
#include "lidar_logic.h"
#include "lightmeter_logic.h"
#include "mrfconstants.h"

// Define BLACK/WHITE locally if not pulled in via GFX headers.
#ifndef BLACK
#define BLACK 0
#endif
#ifndef WHITE
#define WHITE 1
#endif

void drawConfigUI()
{
  beginConfigMenuScreen(F("Setup"));

  selectConfigMenuRow(CONFIG_ROOT_STEP_FILM_MENU, config_step == CONFIG_ROOT_STEP_FILM_MENU);
  u8g2.print(F(" Film: "));
  u8g2.print(film_formats[selected_format].name);
  u8g2.print(F(" > "));

  selectConfigMenuRow(CONFIG_ROOT_STEP_LENS_MENU, config_step == CONFIG_ROOT_STEP_LENS_MENU);
  u8g2.print(F(" Lens: "));
  u8g2.print(lenses[selected_lens].name);
  u8g2.print(F(" > "));

  selectConfigMenuRow(CONFIG_ROOT_STEP_METER_MENU, config_step == CONFIG_ROOT_STEP_METER_MENU);
  u8g2.print(F(" Meter: ISO"));
  u8g2.print(iso);
  u8g2.print(F(" > "));

  selectConfigMenuRow(CONFIG_ROOT_STEP_LIDAR_MENU, config_step == CONFIG_ROOT_STEP_LIDAR_MENU);
  u8g2.print(F(" LiDAR > "));

  selectConfigMenuRow(CONFIG_ROOT_STEP_DISPLAY_MENU, config_step == CONFIG_ROOT_STEP_DISPLAY_MENU);
  u8g2.print(F(" Display > "));

  selectConfigMenuRow(CONFIG_ROOT_STEP_HEALTH, config_step == CONFIG_ROOT_STEP_HEALTH);
  u8g2.print(F(" System Health > "));

  selectConfigMenuRow(CONFIG_ROOT_STEP_EXIT, config_step == CONFIG_ROOT_STEP_EXIT);
  u8g2.print(F(" Exit >> "));

  u8g2.setCursor(CONFIG_ITEM_X, CONFIG_FOOTER_Y);
  setMenuItemColors(false);
  u8g2.print(F(" IDENTIDEM.design MRF "));
  u8g2.print(FWVERSION);

  display.display();
}

void drawFilmConfigUI()
{
  beginConfigMenuScreen(F("Setup > Film"));

  int maxFrameForFormat = getFilmFormatMaxFrame(film_formats[selected_format]);
  int displayedFrame = constrain(film_counter, 0, maxFrameForFormat);

  selectConfigMenuRow(CONFIG_FILM_STEP_FORMAT, config_step == CONFIG_FILM_STEP_FORMAT);
  u8g2.print(F(" Format: "));
  u8g2.print(film_formats[selected_format].name);
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_FILM_STEP_CURRENT_FRAME, config_step == CONFIG_FILM_STEP_CURRENT_FRAME);
  u8g2.print(F(" Current frame: "));
  u8g2.print(displayedFrame);
  u8g2.print(F(" (0.."));
  u8g2.print(maxFrameForFormat);
  u8g2.print(F(") "));

  selectConfigMenuRow(CONFIG_FILM_STEP_RESET, config_step == CONFIG_FILM_STEP_RESET);
  u8g2.print(F(" Reset frame counter >> "));

  selectConfigMenuRow(CONFIG_FILM_STEP_TUNING_MENU, config_step == CONFIG_FILM_STEP_TUNING_MENU);
  u8g2.print(F(" Frame counter tuning > "));

  selectConfigMenuRow(CONFIG_FILM_STEP_BACK, config_step == CONFIG_FILM_STEP_BACK);
  u8g2.print(F(" Back << "));

  display.display();
}

void drawFrameTuningConfigUI()
{
  beginConfigMenuScreen(F("Film > Tuning"));

  selectConfigMenuRow(CONFIG_FRAME_TUNING_STEP_FRAME_ONE_OFFSET,
                      config_step == CONFIG_FRAME_TUNING_STEP_FRAME_ONE_OFFSET);
  u8g2.print(F(" Frame 1 offset: "));
  printSignedInt(frame_one_offset);
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_FRAME_TUNING_STEP_FRAME_SPACING,
                      config_step == CONFIG_FRAME_TUNING_STEP_FRAME_SPACING);
  u8g2.print(F(" Frame spacing: "));
  printSignedInt(frame_spacing_offset);
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_FRAME_TUNING_STEP_BACK, config_step == CONFIG_FRAME_TUNING_STEP_BACK);
  u8g2.print(F(" Back << "));

  display.display();
}

void drawLensConfigUI()
{
  beginConfigMenuScreen(F("Setup > Lens"));

  selectConfigMenuRow(CONFIG_LENS_STEP_LENS, config_step == CONFIG_LENS_STEP_LENS);
  u8g2.print(F(" Lens:"));
  u8g2.print(lenses[selected_lens].name);
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_LENS_STEP_PARALLAX, config_step == CONFIG_LENS_STEP_PARALLAX);
  u8g2.print(F(" Parallax correction: "));
  u8g2.print(parallaxEnabled ? F("On") : F("Off"));
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_LENS_STEP_CALIB, config_step == CONFIG_LENS_STEP_CALIB);
  u8g2.print(F(" Lens Calibration > "));

  selectConfigMenuRow(CONFIG_LENS_STEP_BACK, config_step == CONFIG_LENS_STEP_BACK);
  u8g2.print(F(" Back << "));

  display.display();
}

void drawMeterConfigUI()
{
  beginConfigMenuScreen(F("Setup > Meter"));

  selectConfigMenuRow(CONFIG_METER_STEP_ISO, config_step == CONFIG_METER_STEP_ISO);
  u8g2.print(F(" ISO:"));
  u8g2.print(iso);
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_METER_STEP_EV_COMP, config_step == CONFIG_METER_STEP_EV_COMP);
  u8g2.print(F(" EV Comp:"));
  float evComp = static_cast<float>(exposure_comp_thirds) / 3.0f;
  if (evComp >= 0.0f)
  {
    u8g2.print(F("+"));
  }
  u8g2.print(evComp, 1);
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_METER_STEP_SMOOTHING, config_step == CONFIG_METER_STEP_SMOOTHING);
  u8g2.print(F(" Smoothing:"));
  u8g2.print(getMeterSmoothingLabel(meter_smoothing_mode));
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_METER_STEP_EV_READOUT, config_step == CONFIG_METER_STEP_EV_READOUT);
  u8g2.print(F(" EV Readout:"));
  u8g2.print(show_ev_readout ? F("On") : F("Off"));
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_METER_STEP_BACK, config_step == CONFIG_METER_STEP_BACK);
  u8g2.print(F(" Back << "));

  display.display();
}

void drawLidarConfigUI()
{
  beginConfigMenuScreen(F("Setup > LiDAR"));

  selectConfigMenuRow(CONFIG_LIDAR_STEP_OFFSET, config_step == CONFIG_LIDAR_STEP_OFFSET);
  u8g2.print(F(" Distance offset: "));
  u8g2.print(lidar_distance_offset_mm);
  u8g2.print(F("mm "));

  selectConfigMenuRow(CONFIG_LIDAR_STEP_IDLE_TIMEOUT, config_step == CONFIG_LIDAR_STEP_IDLE_TIMEOUT);
  u8g2.print(F(" Idle timeout: "));
  u8g2.print(getSleepTimeoutModeLabel(lidar_idle_timeout_mode));
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_LIDAR_STEP_DIAGNOSTICS, config_step == CONFIG_LIDAR_STEP_DIAGNOSTICS);
  u8g2.print(F(" Diagnostics >> "));

  selectConfigMenuRow(CONFIG_LIDAR_STEP_BACK, config_step == CONFIG_LIDAR_STEP_BACK);
  u8g2.print(F(" Back << "));

  display.display();
}

// Live LiDAR telemetry. Field testers aim at a target and read back exactly what
// the sensor returns, so range/lock failures can be diagnosed from data instead
// of guesses. Values are updated each frame in setDistance(); see the field-test
// protocol in Documentation/hardware-errata/lidar-field-test.md.
void drawLidarDiagnosticsUI()
{
  preparePrimaryDisplayTextMode();

  u8g2.setFont(u8g2_font_6x10_mf);
  u8g2.setCursor(HEALTH_TITLE_X, HEALTH_TITLE_Y);
  u8g2.print(F("LiDAR Diagnostics"));

  u8g2.setFont(u8g2_font_4x6_mf);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 0));
  u8g2.print(F("Raw: "));
  u8g2.print(lidar_raw_distance_mm);
  u8g2.print(F("mm  Disp: "));
  u8g2.print(distance_cm);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 1));
  u8g2.print(F("Intensity: "));
  u8g2.print(lidar_primary_intensity);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 2));
  u8g2.print(F("SunBase: "));
  u8g2.print(lidar_sunlight_base);
  u8g2.print(F("  SNR: "));
  u8g2.print(lidar_snr_permille);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 3));
  u8g2.print(F("Quality: "));
  u8g2.print(lidar_quality_level);
  u8g2.print(F("  Held: "));
  u8g2.print(lidar_distance_held ? F("Y") : F("N"));

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 4));
  u8g2.print(F("fps req:"));
  u8g2.print(LIDAR_FRAME_RATE_FPS);
  u8g2.print(F(" act:"));
  u8g2.print(lidar_frame_rate_actual);
  // Age of the telemetry above — a climbing value means the sensor has stopped
  // producing frames and the numbers on this screen are stale.
  char ageText[8];
  formatLidarTelemetryAge(static_cast<uint32_t>(millis()),
                          static_cast<uint32_t>(lidar_telemetry_ms),
                          ageText,
                          sizeof(ageText));
  u8g2.print(F(" Age:"));
  u8g2.print(ageText);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 5));
  u8g2.print(F("err:"));
  u8g2.print(last_lidar_error_code);
  u8g2.print(F("  Recov:"));
  u8g2.print(lidar_recovery_count);
  u8g2.print(F("  Sun:"));
  u8g2.print(lidar_high_sunlight ? F("Hi") : F("Ok"));

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_FOOTER_Y);
  u8g2.print(F(" (L/R) Back"));

  display.display();
}

void drawDisplayConfigUI()
{
  beginConfigMenuScreen(F("Setup > Display"));

  selectConfigMenuRow(CONFIG_DISPLAY_STEP_BRIGHTNESS_MODE,
                      config_step == CONFIG_DISPLAY_STEP_BRIGHTNESS_MODE);
  u8g2.print(F(" Bright mode: "));
  u8g2.print(brightness_auto ? F("Auto  ") : F("Manual"));

  selectConfigMenuRow(CONFIG_DISPLAY_STEP_BRIGHTNESS_VALUE,
                      config_step == CONFIG_DISPLAY_STEP_BRIGHTNESS_VALUE);
  if (brightness_auto)
  {
    u8g2.print(F(" Bright top: "));
    u8g2.print(brightness_auto_top_pct);
    u8g2.print(F("% "));
  }
  else
  {
    u8g2.print(F(" Bright level: "));
    u8g2.print(brightness_manual_pct);
    u8g2.print(F("% "));
  }

  selectConfigMenuRow(CONFIG_DISPLAY_STEP_HORIZON_ENABLE,
                      config_step == CONFIG_DISPLAY_STEP_HORIZON_ENABLE);
  u8g2.print(F(" Horizon line: "));
  u8g2.print(show_horizon_line ? F("On ") : F("Off"));

  selectConfigMenuRow(CONFIG_DISPLAY_STEP_SLEEP_TIMEOUT,
                      config_step == CONFIG_DISPLAY_STEP_SLEEP_TIMEOUT);
  u8g2.print(F(" Sleep timeout: "));
  u8g2.print(getSleepTimeoutModeLabel(sleep_timeout_mode));
  u8g2.print(F(" "));

  selectConfigMenuRow(CONFIG_DISPLAY_STEP_HORIZON_TRIM_MENU,
                      config_step == CONFIG_DISPLAY_STEP_HORIZON_TRIM_MENU);
  u8g2.print(F(" Horizon trim > "));

  selectConfigMenuRow(CONFIG_DISPLAY_STEP_RETICLE_ADJUST,
                      config_step == CONFIG_DISPLAY_STEP_RETICLE_ADJUST);
  u8g2.print(F(" Focus reticle > "));

  selectConfigMenuRow(CONFIG_DISPLAY_STEP_BACK, config_step == CONFIG_DISPLAY_STEP_BACK);
  u8g2.print(F(" Back << "));

  display.display();
}

void drawHorizonTrimConfigUI()
{
  beginConfigMenuScreen(F("Display > Horizon"));

  selectConfigMenuRow(CONFIG_HORIZON_TRIM_STEP_LANDSCAPE,
                      config_step == CONFIG_HORIZON_TRIM_STEP_LANDSCAPE);
  u8g2.print(F(" Landscape: "));
  printSignedDeciDegrees(level_trim_landscape_deci_deg);
  u8g2.print(F("deg "));

  selectConfigMenuRow(CONFIG_HORIZON_TRIM_STEP_PORTRAIT_POS,
                      config_step == CONFIG_HORIZON_TRIM_STEP_PORTRAIT_POS);
  u8g2.print(F(" Portrait+: "));
  printSignedDeciDegrees(level_trim_portrait_pos_deci_deg);
  u8g2.print(F("deg "));

  selectConfigMenuRow(CONFIG_HORIZON_TRIM_STEP_PORTRAIT_NEG,
                      config_step == CONFIG_HORIZON_TRIM_STEP_PORTRAIT_NEG);
  u8g2.print(F(" Portrait-: "));
  printSignedDeciDegrees(level_trim_portrait_neg_deci_deg);
  u8g2.print(F("deg "));

  selectConfigMenuRow(CONFIG_HORIZON_TRIM_STEP_BACK, config_step == CONFIG_HORIZON_TRIM_STEP_BACK);
  u8g2.print(F(" Back << "));

  display.display();
}

void drawReticleAdjustUI()
{
  preparePrimaryDisplayTextMode();

  const int refX = SCREEN_WIDTH / 2;
  const int refY = (SCREEN_HEIGHT / 2) - MAIN_RETICLE_CENTER_Y_OFFSET;

  // Top header: current offsets, with a marker pointing to the active axis.
  u8g2.setFont(u8g2_font_6x10_mf);
  u8g2.setCursor(2, 12);
  u8g2.print(reticle_adjust_step == 0 ? F(">X:") : F(" X:"));
  u8g2.print(reticle_offset_x);
  u8g2.setCursor(64, 12);
  u8g2.print(reticle_adjust_step == 1 ? F(">Y:") : F(" Y:"));
  u8g2.print(reticle_offset_y);

  // Reference crosshair at the unmodified screen centre — gives the user a
  // fixed anchor to gauge how far the reticle dot has moved.
  display.drawLine(refX - 5, refY, refX + 5, refY, WHITE);
  display.drawLine(refX, refY - 5, refX, refY + 5, WHITE);

  // Reticle dot drawn over the crosshair so it visually "snaps" to centre when
  // both offsets are zero.
  const int cx = refX + reticle_offset_x;
  const int cy = refY + reticle_offset_y;
  display.fillCircle(cx, cy, MAIN_RETICLE_CENTER_RADIUS, WHITE);

  // Bottom: button hints — directional cue matches the current step.
  u8g2.setFont(u8g2_font_4x6_mf);
  u8g2.setCursor(2, 122);
  if (reticle_adjust_step == 0)
  {
    u8g2.print(F("(L)< X >(R)  hold:next"));
  }
  else
  {
    u8g2.print(F("(L)^ Y v(R)  hold:save"));
  }

  display.display();
}

void drawCalibProgressBar(int current, int total)
{
  const int barWidth = SCREEN_WIDTH - 2 * CALIB_ITEM_X;
  const int barHeight = 5;
  const int barX = CALIB_ITEM_X;
  const int barY = CALIB_PROGRESS_BAR_Y;

  display.drawRect(barX, barY, barWidth, barHeight, WHITE);
  int fillWidth = (total > 0) ? ((barWidth - 2) * current / total) : 0;
  if (fillWidth > 0)
  {
    display.fillRect(barX + 1, barY + 1, fillWidth, barHeight - 2, WHITE);
  }
}

void drawCalibCompleteUI()
{
  preparePrimaryDisplayTextMode();

  u8g2.setFont(u8g2_font_6x10_mf);
  u8g2.setCursor(16, 58);
  u8g2.print(F("Calibration"));
  u8g2.setCursor(28, 72);
  u8g2.print(F("complete!"));

  display.display();
}

void drawCalibUI()
{
  preparePrimaryDisplayTextMode();

  u8g2.setFont(u8g2_font_6x10_mf);
  u8g2.setCursor(CALIB_TITLE_X, CALIB_TITLE_Y);
  u8g2.print(F("Lens > Calibrate"));

  u8g2.setFont(u8g2_font_4x6_mf);

  u8g2.setCursor(CALIB_ITEM_X, CALIB_LENS_Y);
  setMenuItemColors(calib_step == 0);
  u8g2.print(F(" Lens:"));
  u8g2.print(lenses[calib_lens].name);
  u8g2.print(F(" "));

  const Lens &calibrationLens = lenses[calib_lens];
  int calibrationPointCount = getLensDistancePointCount(calibrationLens);
  if (calibrationPointCount <= 0)
  {
    calibrationPointCount = 1;
  }

  int calibrationIndex = max(0, min(current_calib_distance, calibrationPointCount - 1));
  float calibrationDistance = calibrationLens.distance[calibrationIndex];

  u8g2.setCursor(CALIB_ITEM_X, CALIB_DISTANCE_Y);
  setMenuItemColors(calib_step == 1);
  u8g2.print(F(" "));
  u8g2.print(calibrationDistance, DISTANCE_DECIMAL_PLACES);
  u8g2.print(F("m: "));
  u8g2.print(lens_sensor_reading);
  u8g2.print(F(" ("));
  u8g2.print(current_calib_distance + 1);
  u8g2.print(F("/"));
  u8g2.print(calibrationPointCount);
  u8g2.print(F(") "));

  setMenuItemColors(false);

  // Draw progress bar during capture step.
  if (calib_step == 1)
  {
    drawCalibProgressBar(current_calib_distance, calibrationPointCount);
  }

  if (calib_step == 0)
  {
    u8g2.setCursor(CALIB_ITEM_X, CALIB_HELP_Y1);
    u8g2.print(F(" (L) to Cycle"));
    u8g2.setCursor(CALIB_ITEM_X, CALIB_HELP_Y2);
    u8g2.print(F(" (R) to Select"));
  }
  else
  {
    u8g2.setCursor(CALIB_ITEM_X, CALIB_HELP_Y1);
    u8g2.print(F(" Set focus ring to distance,"));
    u8g2.setCursor(CALIB_ITEM_X, CALIB_HELP_Y2);
    u8g2.print(F(" then (L) to capture."));
    u8g2.setCursor(CALIB_ITEM_X, CALIB_HELP_Y3);
    u8g2.print(F(" (R) to Cancel"));

    // Show error for at least CALIB_ERROR_HOLD_MS, then auto-clear.
    if (calib_capture_status != CALIB_CAPTURE_STATUS_NONE)
    {
      if ((millis() - calib_capture_status_ms) >= CALIB_ERROR_HOLD_MS)
      {
        calib_capture_status = CALIB_CAPTURE_STATUS_NONE;
      }
      else if (calib_capture_status == CALIB_CAPTURE_STATUS_UNSTABLE)
      {
        u8g2.setCursor(CALIB_ITEM_X, CALIB_STATUS_Y1);
        u8g2.print(F(" Unstable reading"));
        u8g2.setCursor(CALIB_ITEM_X, CALIB_STATUS_Y2);
        u8g2.print(F(" Hold lens still and retry"));
      }
      else if (calib_capture_status == CALIB_CAPTURE_STATUS_NON_MONOTONIC)
      {
        u8g2.setCursor(CALIB_ITEM_X, CALIB_STATUS_Y1);
        u8g2.print(F(" Out of sequence"));
        u8g2.setCursor(CALIB_ITEM_X, CALIB_STATUS_Y2);
        u8g2.print(F(" Increase focus distance"));
      }
    }
  }

  display.display();
}

void drawResetConfirmUI()
{
  beginConfigMenuScreen(F("Reset Count?"));

  u8g2.setCursor(CONFIG_ITEM_X, CALIB_HELP_Y1);
  u8g2.print(F(" (L) Cancel"));
  u8g2.setCursor(CONFIG_ITEM_X, CALIB_HELP_Y2);
  u8g2.print(F(" (R) Reset"));

  display.display();
}

void drawFactoryResetConfirmUI()
{
  beginConfigMenuScreen(F("Factory Reset?"));

  u8g2.setCursor(CONFIG_ITEM_X, CALIB_HELP_Y1);
  u8g2.print(F(" All settings will be"));
  u8g2.setCursor(CONFIG_ITEM_X, CALIB_HELP_Y2);
  u8g2.print(F(" erased. Device reboots."));

  u8g2.setCursor(CONFIG_ITEM_X, CONFIG_FOOTER_Y - 10);
  u8g2.print(F(" (L) Cancel"));
  u8g2.setCursor(CONFIG_ITEM_X, CONFIG_FOOTER_Y);
  u8g2.print(F(" (R) Reset all settings"));

  display.display();
}

void drawHealthUI()
{
  preparePrimaryDisplayTextMode();

  u8g2.setFont(u8g2_font_6x10_mf);
  u8g2.setCursor(HEALTH_TITLE_X, HEALTH_TITLE_Y);
  u8g2.print(F("System Health"));

  u8g2.setFont(u8g2_font_4x6_mf);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 0));
  u8g2.print(F("FW: "));
  u8g2.print(FWVERSION);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 1));
  u8g2.print(F("Prefs: "));
  if (prefsSchemaValid)
  {
    u8g2.print(F("v"));
    u8g2.print(prefsSchemaVersionLoaded);
    u8g2.print(F(" OK"));
  }
  else if (prefsLoadedLegacy)
  {
    u8g2.print(F("Legacy migrated"));
  }
  else
  {
    u8g2.print(F("Defaults"));
  }

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 2));
  u8g2.print(F("LiDAR: "));
  u8g2.print(hardware.lidarSensor ? F("OK ") : F("InitErr "));
  u8g2.print(lidarEnabled ? F("On") : F("Off"));
  u8g2.print(F(" err:"));
  u8g2.print(last_lidar_error_code);
  u8g2.print(F(" v"));
  u8g2.print(lidar_sensor_fw_version);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 3));
  u8g2.print(F("Recoveries: "));
  u8g2.print(lidar_recovery_count);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 4));
  u8g2.print(F("HW D"));
  u8g2.print(hardware.mainDisplay ? 1 : 0);
  u8g2.print(F(" X"));
  u8g2.print(hardware.externalDisplay ? 1 : 0);
  u8g2.print(F(" A"));
  u8g2.print(hardware.ads ? 1 : 0);
  u8g2.print(F(" M"));
  u8g2.print(hardware.mpu ? 1 : 0);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_ITEM_Y_START + (HEALTH_ITEM_Y_STEP * 5));
  u8g2.print(F("HW L"));
  u8g2.print(hardware.lightMeter ? 1 : 0);
  u8g2.print(F(" B"));
  u8g2.print(hardware.batteryGauge ? 1 : 0);
  u8g2.print(F(" E"));
  u8g2.print(hardware.encoder ? 1 : 0);
  u8g2.print(F(" P"));
  u8g2.print(hardware.statusPixel ? 1 : 0);

  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_FOOTER_Y - 8);
  if (!hardware.lidarSensor)
  {
    u8g2.print(F(" (L) Back  (R) Retry LiDAR"));
  }
  else
  {
    u8g2.print(F(" (L/R) Back"));
  }
  u8g2.setCursor(HEALTH_ITEM_X, HEALTH_FOOTER_Y);
  u8g2.print(F(" (R long) Factory Reset"));

  display.display();
}
