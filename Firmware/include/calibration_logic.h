#ifndef CALIBRATION_LOGIC_H
#define CALIBRATION_LOGIC_H

bool computeStableCalibrationReading(
    const int *samples,
    int sample_count,
    int outlier_max_delta,
    int min_inlier_count,
    int max_inlier_spread,
    int &averaged_reading);

bool validateMonotonicCalibration(const int *readings, int reading_count, int min_step);

// True when the calibration readings increase with focus distance, the required
// convention so estimateLensDistance() can interpolate (a backwards-wired sensor
// captures a descending sequence the estimator would read inverted). Fewer than
// two readings leave the direction undetermined and return true.
bool isAscendingCalibration(const int *readings, int reading_count);

#endif // CALIBRATION_LOGIC_H
