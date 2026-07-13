#include "lens_logic.h"

#include <Arduino.h>
#include <math.h>

#include "mrfconstants.h"

int findLensSnapIndex(const Lens &lens, int sensor_reading)
{
  const int reading_count = getLensDistancePointCount(lens);
  if (reading_count <= 0)
  {
    return -1;
  }

  int snap_index = -1;
  int snap_delta = max(LENS_SNAP_DEADZONE, LENS_SNAP_DEADZONE_FAR) + 1;

  for (int i = 0; i < reading_count; i++)
  {
    int delta = abs(sensor_reading - lens.sensor_reading[i]);
    int snap_deadzone = (lens.distance[i] >= LENS_SNAP_FAR_DISTANCE_M) ? LENS_SNAP_DEADZONE_FAR : LENS_SNAP_DEADZONE;
    if (delta <= snap_deadzone && delta < snap_delta)
    {
      // The far deadzone is in ADC counts, but far marks can sit on very
      // sparse spans (a handful of counts covering metres of focus travel).
      // Only snap when the interpolated distance is actually near the mark;
      // otherwise a reading 3 counts from a sparse 10m mark displays "10.0m"
      // for a subject the table places at 8.5m — and that snapped value also
      // becomes the LiDAR plausibility prior.
      if (delta > 0 && lens.distance[i] >= LENS_SNAP_FAR_DISTANCE_M)
      {
        LensDistanceEstimate estimate = estimateLensDistance(lens, sensor_reading);
        if (estimate.valid && !estimate.is_infinity)
        {
          int mark_cm = static_cast<int>(lroundf(lens.distance[i] * CM_PER_METER));
          int error_cm = abs(estimate.distance_cm - mark_cm);
          if (error_cm * 100 > mark_cm * LENS_SNAP_FAR_MAX_ERROR_PCT)
          {
            continue;
          }
        }
      }
      snap_delta = delta;
      snap_index = i;
    }
  }

  return snap_index;
}

LensDistanceEstimate estimateLensDistance(const Lens &lens, int sensor_reading)
{
  LensDistanceEstimate result = {false, false, 0};
  const int reading_count = getLensDistancePointCount(lens);
  if (reading_count <= 0)
  {
    return result;
  }

  const int last_index = reading_count - 1;

  if (sensor_reading < lens.sensor_reading[0])
  {
    result.valid = true;
    result.distance_cm = static_cast<int>(lroundf(lens.distance[0] * CM_PER_METER));
    return result;
  }

  const int last_sensor = lens.sensor_reading[last_index];
  if (sensor_reading > last_sensor + LENS_INF_THRESHOLD)
  {
    result.valid = true;
    result.is_infinity = true;
    result.distance_cm = LENS_INFINITY_RAW;
    return result;
  }

  for (int i = 0; i < reading_count; i++)
  {
    if (sensor_reading == lens.sensor_reading[i])
    {
      result.valid = true;
      result.distance_cm = static_cast<int>(lroundf(lens.distance[i] * CM_PER_METER));
      return result;
    }
  }

  for (int i = 0; i < last_index; i++)
  {
    const int left_sensor = lens.sensor_reading[i];
    const int right_sensor = lens.sensor_reading[i + 1];
    if (left_sensor < sensor_reading && sensor_reading < right_sensor)
    {
      const float left_distance = lens.distance[i];
      const float right_distance = lens.distance[i + 1];
      // Interpolate in reciprocal-distance space. The focus helicoid extension
      // (what the linear sensor measures) is ~proportional to 1/distance, so
      // 1/distance is ~linear in the sensor reading between marks. Linear-in-
      // distance interpolation over-reads across the sparse far marks.
      const float t = static_cast<float>(sensor_reading - left_sensor) /
                      static_cast<float>(right_sensor - left_sensor);
      const float inv_left = 1.0f / left_distance;
      const float inv_right = 1.0f / right_distance;
      const float interpolated = 1.0f / (inv_left + t * (inv_right - inv_left));
      result.valid = true;
      // Round to the nearest cm; truncation biased every between-marks
      // estimate up to 1 cm short.
      result.distance_cm = static_cast<int>(lroundf(interpolated * CM_PER_METER));
      return result;
    }
  }

  if (sensor_reading >= last_sensor)
  {
    result.valid = true;
    result.distance_cm = static_cast<int>(lroundf(lens.distance[last_index] * CM_PER_METER));
    return result;
  }

  return result;
}
