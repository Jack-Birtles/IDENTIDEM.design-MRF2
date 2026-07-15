#include "sleep_timeout_logic.h"

#include "mrfconstants.h"

namespace
{
const char *SLEEP_TIMEOUT_MODE_LABELS[SLEEP_TIMEOUT_MODE_COUNT] = {
    "Off",
    "15s",
    "30sec",
    "1m",
    "1m30s",
    "2m"};

const unsigned long SLEEP_TIMEOUT_MODE_MS[SLEEP_TIMEOUT_MODE_COUNT] = {
    0,
    15000,
    30000,
    60000,
    90000,
    120000};
} // namespace

int clampSleepTimeoutMode(int timeout_mode)
{
  if (timeout_mode < SLEEP_TIMEOUT_MODE_MIN)
  {
    return SLEEP_TIMEOUT_MODE_MIN;
  }
  if (timeout_mode > SLEEP_TIMEOUT_MODE_MAX)
  {
    return SLEEP_TIMEOUT_MODE_MAX;
  }
  return timeout_mode;
}

const char *getSleepTimeoutModeLabel(int timeout_mode)
{
  return SLEEP_TIMEOUT_MODE_LABELS[clampSleepTimeoutMode(timeout_mode)];
}

unsigned long getSleepTimeoutModeMs(int timeout_mode)
{
  return SLEEP_TIMEOUT_MODE_MS[clampSleepTimeoutMode(timeout_mode)];
}
