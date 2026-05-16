#include "lens_spike_logic.h"

#include <stdlib.h>

#include "mrfconstants.h"

int updateLensSpikeFilter(LensSpikeFilterState &state, int smoothedReading)
{
  if (!state.initialized)
  {
    state.initialized = true;
    state.stableReading = smoothedReading;
    state.pendingReading = smoothedReading;
    state.pendingCount = 0;
    return state.stableReading;
  }

  if (abs(smoothedReading - state.stableReading) <= LENS_SPIKE_DELTA_THRESHOLD)
  {
    state.stableReading = smoothedReading;
    state.pendingCount = 0;
    return state.stableReading;
  }

  if (state.pendingCount == 0 ||
      abs(smoothedReading - state.pendingReading) > LENS_SPIKE_DELTA_THRESHOLD)
  {
    state.pendingReading = smoothedReading;
    state.pendingCount = 1;
    return state.stableReading;
  }

  state.pendingCount++;
  if (state.pendingCount >= LENS_SPIKE_CONFIRMATION_COUNT)
  {
    state.stableReading = smoothedReading;
    state.pendingCount = 0;
  }

  return state.stableReading;
}
