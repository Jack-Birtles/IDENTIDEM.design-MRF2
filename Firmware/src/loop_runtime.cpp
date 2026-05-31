#include "loop_runtime.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_sleep.h"

#include "activity.h"
#include "cyclefuncs.h"
#include "globals.h"
#include "hardware.h"
#include "helpers.h"
#include "inputs.h"
#include "interface.h"
#include "mrfconstants.h"
#include "setfuncs.h"
#include "ui_signature_logic.h"

namespace
{
struct LoopScheduler
{
  bool initialized = false;
  unsigned long lastInputMs = 0;
  unsigned long lastFilmCounterMs = 0;
  unsigned long lastSleepCheckMs = 0;
  unsigned long lastLidarMs = 0;
  unsigned long lastLensMs = 0;
  unsigned long lastMeterMs = 0;
  unsigned long lastBatteryMs = 0;
  unsigned long lastUiMs = 0;
  unsigned long lastPrefsFlushMs = 0;
  unsigned long lastBrightnessMs = 0;
};

struct UiRenderCache
{
  bool initialized = false;
  UiMode lastMode = UiMode::Main;
  uint32_t mainSignature = 0;
  uint32_t menuSignature = 0;
  uint32_t externalSignature = 0;
  unsigned long lastMainDrawMs = 0;
  unsigned long lastHealthDrawMs = 0;
};

struct DisplayFadeState
{
  bool active = false;
  int brightness = 0xFF;
  unsigned long lastStepMs = 0;
};

struct LoopRuntimeState
{
  LoopScheduler scheduler;
  UiRenderCache uiRenderCache;
  DisplayFadeState fade;
  bool sleepServicesActive = false;
  bool sleepWakeBaselinesInitialized = false;
  bool lightMeterSleeping = false;
  bool lidarIdleStandbyActive = false;
  int sleepWakeEncoderBaseline = 0;
  int sleepWakeLensBaseline = 0;
  unsigned long lastFilmMovementMs = 0;
  unsigned long lastLensMovementMs = 0;
  unsigned long lastMeterChangeMs = 0;
};

LoopRuntimeState loopState;

MainUiSnapshot captureMainUiSnapshot()
{
  return {
      static_cast<int>(ui_mode),
      selected_lens,
      selected_format,
      iso,
      aperture,
      show_ev_readout,
      ev_readout,
      shutter_speed,
      distance_cm,
      lens_distance_cm,
      distance,
      lens_distance_raw,
      lidar_quality_level,
      lidar_high_sunlight,
      parallaxEnabled,
      reticle_offset_x,
      reticle_offset_y,
      show_horizon_line,
  };
}

MenuUiSnapshot captureMenuUiSnapshot()
{
  return {
      static_cast<int>(ui_mode),
      config_step,
      calib_step,
      selected_lens,
      selected_format,
      calib_lens,
      current_calib_distance,
      calib_capture_status,
      calib_capture_status_ms,
      lens_sensor_reading,
      iso,
      aperture,
      exposure_comp_thirds,
      meter_smoothing_mode,
      show_ev_readout,
      parallaxEnabled,
      sleep_timeout_mode,
      lidar_idle_timeout_mode,
      level_trim_landscape_deci_deg,
      level_trim_portrait_pos_deci_deg,
      level_trim_portrait_neg_deci_deg,
      reticle_offset_x,
      reticle_offset_y,
      reticle_adjust_step,
      brightness_auto,
      brightness_manual_pct,
      brightness_auto_top_pct,
      show_horizon_line,
      frame_one_offset,
      frame_spacing_offset,
      film_counter,
      last_lidar_error_code,
      lidar_recovery_count,
      lidarEnabled,
      hardware.ads,
      hardware.mpu,
      hardware.mainDisplay,
      hardware.externalDisplay,
      hardware.batteryGauge,
      hardware.lightMeter,
      hardware.statusPixel,
      hardware.encoder,
      hardware.lidarSensor,
      prefsSchemaValid,
      prefsLoadedLegacy,
      static_cast<int>(prefsSchemaVersionLoaded),
  };
}

ExternalUiSnapshot captureExternalUiSnapshot()
{
  return {
      selected_format,
      selected_lens,
      bat_per,
      film_counter,
      frame_progress,
      sleepMode,
  };
}

bool shouldDrawPrimaryUi(unsigned long nowMs)
{
  UiRenderCache &cache = loopState.uiRenderCache;
  if (ui_mode == UiMode::Main)
  {
    uint32_t signature = buildMainUiSignature(captureMainUiSnapshot());
    bool changed = !cache.initialized || cache.lastMode != ui_mode || signature != cache.mainSignature;
    bool refreshDue = (nowMs - cache.lastMainDrawMs) >= LOOP_UI_MAIN_REFRESH_MS;
    if (!changed && !refreshDue)
    {
      return false;
    }

    cache.mainSignature = signature;
    cache.lastMainDrawMs = nowMs;
    return true;
  }

  uint32_t signature = buildMenuUiSignature(captureMenuUiSnapshot());
  bool changed = !cache.initialized || cache.lastMode != ui_mode || signature != cache.menuSignature;
  bool healthRefreshDue = (ui_mode == UiMode::Health) &&
                          ((nowMs - cache.lastHealthDrawMs) >= LOOP_UI_HEALTH_REFRESH_MS);
  if (!changed && !healthRefreshDue)
  {
    return false;
  }

  cache.menuSignature = signature;
  if (ui_mode == UiMode::Health)
  {
    cache.lastHealthDrawMs = nowMs;
  }
  return true;
}

bool shouldDrawExternalUi()
{
  UiRenderCache &cache = loopState.uiRenderCache;
  uint32_t signature = buildExternalUiSignature(captureExternalUiSnapshot());
  bool changed = !cache.initialized || signature != cache.externalSignature;
  if (changed)
  {
    cache.externalSignature = signature;
  }
  return changed;
}

void updateUiRenderCacheMode()
{
  loopState.uiRenderCache.initialized = true;
  loopState.uiRenderCache.lastMode = ui_mode;
}

// Adaptive polling: stay on the fast cadence while a domain has been active
// recently, then drop to the idle cadence after the hold window expires.
struct AdaptivePollingProfile
{
  unsigned long fast_ms;
  unsigned long idle_ms;
  unsigned long hold_ms;
};

unsigned long selectAdaptiveInterval(unsigned long nowMs,
                                     unsigned long lastActivityMs,
                                     const AdaptivePollingProfile &profile)
{
  if ((nowMs - lastActivityMs) < profile.hold_ms)
  {
    return profile.fast_ms;
  }
  return profile.idle_ms;
}

constexpr AdaptivePollingProfile FILM_COUNTER_POLLING_PROFILE = {
    LOOP_FILM_COUNTER_INTERVAL_MS,
    LOOP_FILM_COUNTER_IDLE_INTERVAL_MS,
    LOOP_FILM_COUNTER_ACTIVE_HOLD_MS};
constexpr AdaptivePollingProfile LENS_POLLING_PROFILE = {
    LOOP_LENS_INTERVAL_MS,
    LOOP_LENS_IDLE_INTERVAL_MS,
    LOOP_LENS_ACTIVE_HOLD_MS};
constexpr AdaptivePollingProfile LIGHTMETER_POLLING_PROFILE = {
    LOOP_LIGHTMETER_INTERVAL_MS,
    LOOP_LIGHTMETER_IDLE_INTERVAL_MS,
    LOOP_LIGHTMETER_ACTIVE_HOLD_MS};

unsigned long getFilmCounterIntervalMs(unsigned long nowMs)
{
  return selectAdaptiveInterval(nowMs, loopState.lastFilmMovementMs, FILM_COUNTER_POLLING_PROFILE);
}

unsigned long getLensIntervalMs(unsigned long nowMs)
{
  return selectAdaptiveInterval(nowMs, loopState.lastLensMovementMs, LENS_POLLING_PROFILE);
}

unsigned long getLightMeterIntervalMs(unsigned long nowMs)
{
  // Keep meter updates snappy whenever user-adjusted settings are pending.
  if (aperture != prev_aperture || iso != prev_iso)
  {
    return LIGHTMETER_POLLING_PROFILE.fast_ms;
  }
  return selectAdaptiveInterval(nowMs, loopState.lastMeterChangeMs, LIGHTMETER_POLLING_PROFILE);
}

bool sendLightMeterCommand(uint8_t command)
{
  if (!hardware.lightMeter)
  {
    return false;
  }

  Wire.beginTransmission(LIGHTMETER_I2C_ADDR);
  Wire.write(command);
  return Wire.endTransmission() == 0;
}

void powerDownLightMeterForSleep()
{
  if (loopState.lightMeterSleeping)
  {
    return;
  }

  if (sendLightMeterCommand(LIGHTMETER_CMD_POWER_DOWN))
  {
    loopState.lightMeterSleeping = true;
    return;
  }

  hardware.lightMeter = false;
  loopState.lightMeterSleeping = false;
}

void wakeLightMeterFromSleep()
{
  if (!loopState.lightMeterSleeping)
  {
    return;
  }

  bool poweredOn = sendLightMeterCommand(LIGHTMETER_CMD_POWER_ON);
  if (!poweredOn)
  {
    hardware.lightMeter = false;
    loopState.lightMeterSleeping = false;
    return;
  }

  bool configured = lightMeter.configure(BH1750::CONTINUOUS_HIGH_RES_MODE);
  if (!configured)
  {
    configured = lightMeter.begin();
  }

  if (!configured)
  {
    hardware.lightMeter = false;
    loopState.lightMeterSleeping = false;
    return;
  }

  hardware.lightMeter = true;
  loopState.lightMeterSleeping = false;
}

// Sleep-wake activity baselines are owned by finaliseSleepServices(): it is
// the single site that captures fresh encoder and lens positions when the
// device enters its first light-sleep tick, and exitSleepServices() is the
// single site that clears sleepWakeBaselinesInitialized when waking. The
// poll functions below rely on baselines being populated; the runtime loop
// only calls them after the sleepWakeBaselinesInitialized gate at line ~675
// has been crossed, so the poll functions never see uninitialized state.
void initializeSleepWakeBaselines()
{
  loopState.sleepWakeEncoderBaseline = hardware.encoder ? encoder.getEncoderPosition() : 0;
  loopState.sleepWakeLensBaseline = hardware.ads ? theads.readADC_SingleEnded(LENS_ADC_PIN) : 0;
  loopState.sleepWakeBaselinesInitialized = true;
}

void pollSleepWakeEncoder()
{
  if (!hardware.encoder)
  {
    return;
  }

  int currentEncoder = encoder.getEncoderPosition();
  if (abs(currentEncoder - loopState.sleepWakeEncoderBaseline) >= SLEEP_WAKE_ENCODER_DELTA)
  {
    registerActivity();
    loopState.sleepWakeEncoderBaseline = currentEncoder;
  }
}

void pollSleepWakeLens()
{
  if (!hardware.ads)
  {
    return;
  }

  int currentLensReading = theads.readADC_SingleEnded(LENS_ADC_PIN);
  if (abs(currentLensReading - loopState.sleepWakeLensBaseline) >= SLEEP_WAKE_LENS_DELTA)
  {
    registerActivity();
    loopState.sleepWakeLensBaseline = currentLensReading;
  }
}

void resetWakeSchedulerAndActivityBaselines(unsigned long nowMs)
{
  // Force immediate sensor/UI refresh right after wake.
  loopState.scheduler.lastLidarMs = 0;
  loopState.scheduler.lastLensMs = 0;
  loopState.scheduler.lastMeterMs = 0;
  loopState.scheduler.lastBatteryMs = 0;
  loopState.scheduler.lastUiMs = 0;
  loopState.lastFilmMovementMs = nowMs;
  loopState.lastLensMovementMs = nowMs;
  loopState.lastMeterChangeMs = nowMs;
}

void beginFadeOutMainDisplay(unsigned long nowMs)
{
  if (!hardware.mainDisplay)
  {
    return;
  }

  loopState.fade.active = true;
  loopState.fade.brightness = 0xFF;
  loopState.fade.lastStepMs = nowMs;
}

bool isFadeComplete()
{
  return !loopState.fade.active;
}

void stepFadeOutMainDisplay(unsigned long nowMs)
{
  if (!loopState.fade.active || !hardware.mainDisplay)
  {
    return;
  }

  if ((nowMs - loopState.fade.lastStepMs) < FADE_STEP_INTERVAL_MS)
  {
    return;
  }

  loopState.fade.lastStepMs = nowMs;
  loopState.fade.brightness -= FADE_STEP_DECREMENT;
  if (loopState.fade.brightness < 0)
  {
    loopState.fade.brightness = 0;
  }

  // SH1107 contrast command: 0x81 followed by contrast value (0x00..0xFF).
  display.oled_command(0x81);
  display.oled_command(static_cast<uint8_t>(loopState.fade.brightness));

  if (loopState.fade.brightness <= 0)
  {
    loopState.fade.active = false;
  }
}

uint8_t computeTargetBrightnessByte()
{
  if (brightness_auto)
  {
    float topFraction = static_cast<float>(brightness_auto_top_pct) / 100.0f;
    float minFraction = static_cast<float>(BRIGHTNESS_AUTO_MIN_PCT) / 100.0f * topFraction;
    uint8_t topByte = static_cast<uint8_t>(topFraction * 0xFF);
    uint8_t minByte = static_cast<uint8_t>(minFraction * 0xFF);
    float clampedLux = constrain(lux, 0.0f, BRIGHTNESS_AUTO_LUX_MAX);
    float scaled = clampedLux / BRIGHTNESS_AUTO_LUX_MAX;
    return static_cast<uint8_t>(minByte + scaled * (topByte - minByte));
  }
  return static_cast<uint8_t>(brightness_manual_pct * 0xFF / 100);
}

void applyDisplayBrightness()
{
  if (!hardware.mainDisplay || loopState.fade.active)
  {
    return;
  }
  display.oled_command(OLED_CMD_SET_CONTRAST);
  display.oled_command(computeTargetBrightnessByte());
}

void restoreMainDisplayBrightness()
{
  if (!hardware.mainDisplay)
  {
    return;
  }
  display.oled_command(OLED_CMD_SET_CONTRAST);
  display.oled_command(computeTargetBrightnessByte());
}

void beginSleepFade(unsigned long nowMs)
{
  toggleLidar(false);
  loopState.lidarIdleStandbyActive = false;
  loopState.uiRenderCache.initialized = false;
  powerDownLightMeterForSleep();
  drawSleepUI();
  beginFadeOutMainDisplay(nowMs);
}

void finaliseSleepServices()
{
  // Keep external sleep text visible while turning the main display fully off.
  if (hardware.mainDisplay)
  {
    display.oled_command(OLED_CMD_DISPLAY_OFF);
  }

  if (hardware.statusPixel)
  {
    sspixel.setPixelColor(NEOPIXEL_INDEX, sspixel.Color(NEOPIXEL_OFF_R, NEOPIXEL_OFF_G, NEOPIXEL_OFF_B));
    sspixel.show();
  }

  initializeSleepWakeBaselines();

  if (hardware.mpu)
  {
    mpu.enableSleep(true);
  }

  // Scale CPU down after all I2C work is complete.
  setCpuFrequencyMhz(CPU_FREQ_SLEEP_MHZ);

  // Configure button GPIOs as light-sleep wakeup sources.
  gpio_wakeup_enable(static_cast<gpio_num_t>(BUTTON_LEFT_PIN),  GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(static_cast<gpio_num_t>(BUTTON_RIGHT_PIN), GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
}

void exitSleepServices()
{
  // Restore full CPU speed before any I2C communications.
  setCpuFrequencyMhz(CPU_FREQ_ACTIVE_MHZ);

  // Remove light-sleep wakeup sources before returning to active polling.
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  gpio_wakeup_disable(static_cast<gpio_num_t>(BUTTON_LEFT_PIN));
  gpio_wakeup_disable(static_cast<gpio_num_t>(BUTTON_RIGHT_PIN));

  if (hardware.mainDisplay)
  {
    display.oled_command(OLED_CMD_DISPLAY_ON);
    restoreMainDisplayBrightness();
  }

  if (hardware.mpu)
  {
    mpu.enableSleep(false);
  }

  wakeLightMeterFromSleep();
  toggleLidar(true);
  loopState.lidarIdleStandbyActive = false;
  loopState.uiRenderCache.initialized = false;
  loopState.sleepWakeBaselinesInitialized = false;
  loopState.fade.active = false;
  resetWakeSchedulerAndActivityBaselines(millis());
}


void updateLidarIdleStandby(unsigned long nowMs)
{
  if (!hardware.lidarSensor)
  {
    loopState.lidarIdleStandbyActive = false;
    return;
  }

  // Keep LiDAR active when not in Main UI; wake immediately on any activity.
  bool canStandby = (ui_mode == UiMode::Main);
  unsigned long idleDurationMs = getIdleDurationMs(nowMs);
  unsigned long idleStandbyTimeoutMs = getSleepTimeoutModeMs(lidar_idle_timeout_mode);

  if (idleStandbyTimeoutMs == 0)
  {
    if (loopState.lidarIdleStandbyActive)
    {
      toggleLidar(true);
      loopState.lidarIdleStandbyActive = !lidarEnabled;
      if (!loopState.lidarIdleStandbyActive)
      {
        loopState.scheduler.lastLidarMs = 0; // Force immediate re-acquisition after wake.
      }
    }
    return;
  }

  if (!loopState.lidarIdleStandbyActive)
  {
    if (canStandby && lidarEnabled && idleDurationMs >= idleStandbyTimeoutMs)
    {
      toggleLidar(false);
      loopState.lidarIdleStandbyActive = !lidarEnabled;
      if (loopState.lidarIdleStandbyActive)
      {
        clearLidarDisplay("Zzz");
      }
    }
    return;
  }

  if (!canStandby || idleDurationMs < idleStandbyTimeoutMs)
  {
    toggleLidar(true);
    loopState.lidarIdleStandbyActive = !lidarEnabled;
    if (!loopState.lidarIdleStandbyActive)
    {
      loopState.scheduler.lastLidarMs = 0; // Force immediate re-acquisition after wake.
    }
  }
}

void drawPrimaryUiForCurrentMode()
{
  switch (ui_mode)
  {
  case UiMode::Main:
    drawMainUI();
    break;
  case UiMode::Config:
    drawConfigUI();
    break;
  case UiMode::ConfigFilm:
    drawFilmConfigUI();
    break;
  case UiMode::ConfigFrameTuning:
    drawFrameTuningConfigUI();
    break;
  case UiMode::ConfigLens:
    drawLensConfigUI();
    break;
  case UiMode::ConfigMeter:
    drawMeterConfigUI();
    break;
  case UiMode::ConfigLidar:
    drawLidarConfigUI();
    break;
  case UiMode::ConfigDisplay:
    drawDisplayConfigUI();
    break;
  case UiMode::ConfigHorizonTrim:
    drawHorizonTrimConfigUI();
    break;
  case UiMode::Calib:
    drawCalibUI();
    break;
  case UiMode::ResetConfirm:
    drawResetConfirmUI();
    break;
  case UiMode::Health:
    drawHealthUI();
    break;
  case UiMode::FactoryResetConfirm:
    drawFactoryResetConfirmUI();
    break;
  case UiMode::ReticleAdjust:
    drawReticleAdjustUI();
    break;
  }
}

bool shouldRunTask(unsigned long nowMs, unsigned long &lastRunMs, unsigned long intervalMs)
{
  if ((nowMs - lastRunMs) < intervalMs)
  {
    return false;
  }

  lastRunMs = nowMs;
  return true;
}

void initializeSchedulerIfNeeded(unsigned long nowMs)
{
  if (loopState.scheduler.initialized)
  {
    return;
  }

  loopState.scheduler.initialized = true;
  loopState.lastFilmMovementMs = nowMs;
  loopState.lastLensMovementMs = nowMs;
  loopState.lastMeterChangeMs = nowMs;
}

void runSleepTasks(unsigned long nowMs)
{
  if (!loopState.sleepServicesActive)
  {
    beginSleepFade(nowMs);
    loopState.sleepServicesActive = true;
  }

  // Non-blocking fade: step contrast down each iteration until complete.
  // Buttons are still polled so user input can wake the device mid-fade.
  if (!isFadeComplete())
  {
    stepFadeOutMainDisplay(nowMs);
    checkButtons();
    return;
  }

  // Fade finished — finalise sleep peripherals once before first light sleep.
  if (!loopState.sleepWakeBaselinesInitialized)
  {
    finaliseSleepServices();
  }

  // Sleep the CPU until a button GPIO fires or the sensor-poll timer expires.
  esp_sleep_enable_timer_wakeup(LOOP_SLEEP_LIGHT_SLEEP_US);
  esp_light_sleep_start();

  // Always check buttons regardless of wakeup cause.
  // GPIO_INTR_LOW_LEVEL fires when a button is pressed (pin goes LOW), but
  // releasing the button makes the pin HIGH again, so the release arrives on
  // the next timer wakeup — not a GPIO wakeup. Bounce2's rose() event (which
  // registers activity) would be missed if checkButtons() were skipped here.
  checkButtons();

  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_GPIO)
  {
    // Timer elapsed (or undefined wakeup) — poll I2C sensors for activity.
    pollSleepWakeEncoder();
    pollSleepWakeLens();
  }
}

void updateFilmCounterTask(unsigned long nowMs)
{
  unsigned long filmCounterIntervalMs = getFilmCounterIntervalMs(nowMs);
  if (!shouldRunTask(nowMs, loopState.scheduler.lastFilmCounterMs, filmCounterIntervalMs))
  {
    return;
  }

  int previousEncoder = encoder_value;
  float previousProgress = frame_progress;
  setFilmCounter();
  if (encoder_value != previousEncoder || fabsf(frame_progress - previousProgress) > 0.0001f)
  {
    loopState.lastFilmMovementMs = nowMs;
  }
}

void updateLidarTask(unsigned long nowMs)
{
  updateLidarIdleStandby(nowMs);
  if (!loopState.lidarIdleStandbyActive &&
      shouldRunTask(nowMs, loopState.scheduler.lastLidarMs, LOOP_LIDAR_INTERVAL_MS))
  {
    setDistance();
  }
}

void updateLensTask(unsigned long nowMs)
{
  if (!hardware.ads)
  {
    return;
  }

  unsigned long lensIntervalMs = getLensIntervalMs(nowMs);
  if (!shouldRunTask(nowMs, loopState.scheduler.lastLensMs, lensIntervalMs))
  {
    return;
  }

  int previousReading = lens_sensor_reading;
  lens_sensor_reading = getLensSensorReading();
  if (abs(lens_sensor_reading - previousReading) > LENS_ACTIVITY_THRESHOLD)
  {
    loopState.lastLensMovementMs = nowMs;
  }
  setLensDistance();
}

void updateLightMeterTask(unsigned long nowMs)
{
  unsigned long lightMeterIntervalMs = getLightMeterIntervalMs(nowMs);
  if (!shouldRunTask(nowMs, loopState.scheduler.lastMeterMs, lightMeterIntervalMs))
  {
    return;
  }

  float previousLux = lux;
  float previousAperture = aperture;
  int previousIso = iso;
  char previousShutter[sizeof(shutter_speed)] = {0};
  strncpy(previousShutter, shutter_speed, sizeof(previousShutter) - 1);
  setLightMeter();

  bool meterChanged = fabsf(lux - previousLux) >= LIGHTMETER_ACTIVITY_DELTA_LUX ||
                      aperture != previousAperture ||
                      iso != previousIso ||
                      strcmp(shutter_speed, previousShutter) != 0;
  if (meterChanged)
  {
    loopState.lastMeterChangeMs = nowMs;
  }
}

void updateBatteryTask(unsigned long nowMs)
{
  if (shouldRunTask(nowMs, loopState.scheduler.lastBatteryMs, LOOP_BATTERY_INTERVAL_MS))
  {
    setVoltage();
  }
}

void updateUiTask(unsigned long nowMs)
{
  if (!shouldRunTask(nowMs, loopState.scheduler.lastUiMs, LOOP_UI_INTERVAL_MS))
  {
    return;
  }

  bool drewPrimaryUi = false;
  if (hardware.mainDisplay && shouldDrawPrimaryUi(nowMs))
  {
    drewPrimaryUi = true;
    drawPrimaryUiForCurrentMode();
  }

  if (hardware.externalDisplay && (drewPrimaryUi || shouldDrawExternalUi()))
  {
    drawExternalUI();
  }

  updateUiRenderCacheMode();
}

void runAwakeTasks(unsigned long nowMs)
{
  if (loopState.sleepServicesActive)
  {
    exitSleepServices();
    loopState.sleepServicesActive = false;
  }

  if (shouldRunTask(nowMs, loopState.scheduler.lastInputMs, LOOP_INPUT_INTERVAL_MS))
  {
    checkButtons();
  }

  updateFilmCounterTask(nowMs);
  updateLidarTask(nowMs);
  updateLensTask(nowMs);
  updateLightMeterTask(nowMs);
  updateBatteryTask(nowMs);
  updateUiTask(nowMs);
  if (shouldRunTask(nowMs, loopState.scheduler.lastBrightnessMs, BRIGHTNESS_UPDATE_INTERVAL_MS))
  {
    applyDisplayBrightness();
  }
}

void flushPrefsTask(unsigned long nowMs)
{
  if (shouldRunTask(nowMs, loopState.scheduler.lastPrefsFlushMs, LOOP_PREFS_FLUSH_INTERVAL_MS))
  {
    flushPrefsIfDirty();
  }
}
} // namespace

void runLoopRuntimeIteration(unsigned long now_ms)
{
  initializeSchedulerIfNeeded(now_ms);
  if (shouldRunTask(now_ms, loopState.scheduler.lastSleepCheckMs, LOOP_SLEEP_CHECK_INTERVAL_MS))
  {
    updateSleepMode(now_ms);
  }

  if (sleepMode)
  {
    runSleepTasks(now_ms);
  }
  else
  {
    runAwakeTasks(now_ms);
  }

  flushPrefsTask(now_ms);
}
