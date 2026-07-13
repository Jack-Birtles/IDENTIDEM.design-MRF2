#include "lightmeter_logic.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "mrfconstants.h"

namespace
{
struct StandardSpeed
{
  float seconds;
  const char *label;
};

const float SPEED_TOO_FAST_THRESHOLD = 0.001f;
// Nominal standard shutter speeds. The displayed label is the standard speed
// NEAREST the metered speed in log2 (stop) space; bucket bounds that floored
// to the faster speed gave a systematic 0..-1 stop (mean -0.5 stop)
// underexposure when the user set the displayed speed.
const StandardSpeed STANDARD_SPEEDS[] = {
    {1.0f / 1000.0f, "1/1000"},
    {1.0f / 500.0f, "1/500"},
    {1.0f / 250.0f, "1/250"},
    {1.0f / 125.0f, "1/125"},
    {1.0f / 60.0f, "1/60"},
    {1.0f / 30.0f, "1/30"},
    {1.0f / 15.0f, "1/15"},
    {1.0f / 8.0f, "1/8"},
    {1.0f / 4.0f, "1/4"},
    {1.0f / 2.0f, "1/2"}};

// Half a stop in linear terms: sqrt(2). A metered speed within half a stop of
// the table's fastest/slowest entry still rounds to that entry; beyond the
// edges the caller falls through to Bright! / the raw-decimal display.
const float HALF_STOP_RATIO = 1.4142135f;

const float METER_SMOOTHING_ALPHA[LIGHTMETER_SMOOTHING_MODE_COUNT] = {
    1.0f, // Off
    0.55f,
    0.35f,
    0.20f};

const char *METER_SMOOTHING_LABELS[LIGHTMETER_SMOOTHING_MODE_COUNT] = {
    "Off",
    "Low",
    "Medium",
    "High"};

const int STANDARD_SPEED_COUNT = sizeof(STANDARD_SPEEDS) / sizeof(STANDARD_SPEEDS[0]);

const char *getShutterFractionLabel(float speed)
{
  if (speed <= 0.0f ||
      speed < STANDARD_SPEEDS[0].seconds / HALF_STOP_RATIO ||
      speed >= STANDARD_SPEEDS[STANDARD_SPEED_COUNT - 1].seconds * HALF_STOP_RATIO)
  {
    return nullptr;
  }

  const char *nearest_label = nullptr;
  float nearest_stops = 0.0f;
  for (int i = 0; i < STANDARD_SPEED_COUNT; i++)
  {
    float stops = fabsf(log2f(speed / STANDARD_SPEEDS[i].seconds));
    if (nearest_label == nullptr || stops < nearest_stops)
    {
      nearest_stops = stops;
      nearest_label = STANDARD_SPEEDS[i].label;
    }
  }
  return nearest_label;
}

void trimLeadingSpaces(char *text)
{
  if (!text)
  {
    return;
  }

  while (*text == ' ')
  {
    size_t length = strlen(text);
    memmove(text, text + 1, length);
  }
}
} // namespace

float applyExposureCompensationToLux(float lux, float exposure_comp_ev)
{
  if (lux <= 0.0f)
  {
    return 0.0f;
  }

  float compensationScale = powf(2.0f, exposure_comp_ev);
  if (compensationScale <= 0.0f)
  {
    return lux;
  }

  return lux / compensationScale;
}

float getMeterSmoothingAlpha(int smoothing_mode)
{
  int clampedMode = constrain(
      smoothing_mode,
      LIGHTMETER_SMOOTHING_MODE_MIN,
      LIGHTMETER_SMOOTHING_MODE_MAX);
  return METER_SMOOTHING_ALPHA[clampedMode];
}

const char *getMeterSmoothingLabel(int smoothing_mode)
{
  int clampedMode = constrain(
      smoothing_mode,
      LIGHTMETER_SMOOTHING_MODE_MIN,
      LIGHTMETER_SMOOTHING_MODE_MAX);
  return METER_SMOOTHING_LABELS[clampedMode];
}

float calculateEV100(float lux)
{
  if (lux <= 0.0f)
  {
    return NAN;
  }

  return log2f((lux * 100.0f) / K);
}

void formatShutterSpeed(float lux, float aperture, int iso, char *buffer, size_t bufferSize)
{
  if (!buffer || bufferSize == 0)
  {
    return;
  }

  if (lux <= 0)
  {
    snprintf(buffer, bufferSize, "Dark!");
    return;
  }

  // Select the display label from the un-quantized speed: pre-rounding to
  // 1/1000 s here would misplace fast speeds (a metered 1/400 quantized to
  // 0.003 s reads nearest to 1/250 instead of 1/500).
  float speed = ((aperture * aperture) * K) / (lux * static_cast<float>(iso));

  // Cap at 25 minutes to prevent buffer overflow and absurd display values.
  if (speed > LIGHTMETER_MAX_SPEED_SECONDS)
  {
    speed = LIGHTMETER_MAX_SPEED_SECONDS;
  }

  if (speed >= SPEED_SECONDS_THRESHOLD)
  {
    // Round to nearest 0.5 second for display.
    float rounded = roundf(speed * 2.0f) / 2.0f;

    if (rounded >= 60.0f)
    {
      // Show as minutes and seconds.
      int total_secs = static_cast<int>(roundf(rounded));
      snprintf(buffer, bufferSize, "%dm%ds", total_secs / 60, total_secs % 60);
    }
    else
    {
      // Show as whole or half seconds.
      int whole = static_cast<int>(rounded);
      bool isHalf = (rounded - static_cast<float>(whole)) >= 0.4f;
      if (isHalf)
        snprintf(buffer, bufferSize, "%d.5 sec.", whole);
      else
        snprintf(buffer, bufferSize, "%d sec.", whole);
    }
    return;
  }

  const char *fraction_label = getShutterFractionLabel(speed);
  if (fraction_label != nullptr)
  {
    snprintf(buffer, bufferSize, "%s sec.", fraction_label);
    return;
  }

  // Faster than half a stop past the fastest standard speed: beyond what the
  // shutter can do, not something rounding should paper over.
  if (speed < SPEED_TOO_FAST_THRESHOLD)
  {
    snprintf(buffer, bufferSize, "Bright!");
    return;
  }

  // Between the slowest fraction's half-stop edge and 1 s: show the raw value,
  // quantized to 1/1000 s for a stable readout.
  speed = roundf(speed * LIGHTMETER_SPEED_ROUND_SCALE) / LIGHTMETER_SPEED_ROUND_SCALE;
  char speed_raw[SPEED_TEXT_BUFFER_LEN];
  dtostrf(speed, SPEED_TEXT_WIDTH, SPEED_TEXT_DECIMALS_SHORT, speed_raw);
  trimLeadingSpaces(speed_raw);
  snprintf(buffer, bufferSize, "%s sec.", speed_raw);
}
