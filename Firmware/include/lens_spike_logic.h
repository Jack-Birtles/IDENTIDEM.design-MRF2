#ifndef LENS_SPIKE_LOGIC_H
#define LENS_SPIKE_LOGIC_H

#include <stdint.h>

#include "mrfconstants.h"

// Ring-buffer moving average for the lens ADC. The first sample after a reset
// primes the entire window: a zero-filled window would return sample/N and
// ramp up over N polls, which the spike filter downstream then latches as a
// bogus stable reading (the boot-time "lens stuck at closest focus" artefact).
struct LensMovingAverageState
{
  int samples[SMOOTHING_WINDOW_SIZE] = {};
  int index = 0;
  long total = 0;
  bool primed = false;
};

// Feed one sample; returns the window average rounded to the nearest count.
int updateLensMovingAverage(LensMovingAverageState &state, int sample);

// Re-arm priming so the next sample refills the window.
void resetLensMovingAverageState(LensMovingAverageState &state);

// Three-state filter that rejects transient spikes from the lens ADC.
// Used by the lens-distance pipeline to keep a momentary glitch from
// re-snapping the focus ring while the user is not actually turning the lens.
//
// Rule:
//   * Readings within LENS_SPIKE_DELTA_THRESHOLD of the current stable value
//     are accepted immediately and become the new stable reading.
//   * A reading that jumps further is treated as pending: the filter waits
//     for LENS_SPIKE_CONFIRMATION_COUNT consecutive samples all within the
//     threshold of that pending value before promoting it to stable.
//   * Any sample that drifts away from the pending value restarts the
//     pending sequence with the new reading.
//
// Note that the "consecutive samples" count is in *calls*, not in elapsed
// time, so the filter's lockout duration scales with the lens-sensor poll
// cadence. Callers that change the poll interval should re-tune
// LENS_SPIKE_CONFIRMATION_COUNT.
struct LensSpikeFilterState
{
  bool initialized = false;
  int stableReading = 0;
  int pendingReading = 0;
  uint8_t pendingCount = 0;
};

int updateLensSpikeFilter(LensSpikeFilterState &state, int smoothedReading);

#endif // LENS_SPIKE_LOGIC_H
