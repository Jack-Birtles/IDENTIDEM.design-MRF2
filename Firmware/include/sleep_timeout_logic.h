#ifndef SLEEP_TIMEOUT_LOGIC_H
#define SLEEP_TIMEOUT_LOGIC_H

// Sleep/idle timeout mode tables. Pure and host-testable; the runtime
// state-machine suite compiles this module directly so the tests always
// exercise the real mode->ms mapping.

int clampSleepTimeoutMode(int timeout_mode);
const char *getSleepTimeoutModeLabel(int timeout_mode);
unsigned long getSleepTimeoutModeMs(int timeout_mode);

#endif // SLEEP_TIMEOUT_LOGIC_H
