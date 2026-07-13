#include "lens_spike_logic.h"

#include <stdlib.h>

#include "mrfconstants.h"

int updateLensMovingAverage(LensMovingAverageState &state, int sample)
{
  if (!state.primed)
  {
    for (int i = 0; i < SMOOTHING_WINDOW_SIZE; i++)
    {
      state.samples[i] = sample;
    }
    state.index = 0;
    state.total = static_cast<long>(sample) * SMOOTHING_WINDOW_SIZE;
    state.primed = true;
    return sample;
  }

  state.total -= state.samples[state.index];
  state.samples[state.index] = sample;
  state.total += sample;
  state.index = (state.index + 1) % SMOOTHING_WINDOW_SIZE;
  return static_cast<int>((state.total + SMOOTHING_WINDOW_SIZE / 2) / SMOOTHING_WINDOW_SIZE);
}

void resetLensMovingAverageState(LensMovingAverageState &state)
{
  state = LensMovingAverageState{};
}

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
