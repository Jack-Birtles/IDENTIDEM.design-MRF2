#include "formats.h"
#include <stddef.h>  // For size_t
#include "mrfconstants.h"

// 9x3 and 6x9 share the same physical advance pattern on 120 film (8 frames per
// roll, identical backing-paper number positions). Keep their encoder thresholds
// identical via a macro so a future calibration change cannot drift them apart.
#define SENSOR_8_FRAMES_120 {0, 142, 184, 223, 260, 295, 329, 360, 390, 550}
#define FRAMES_8_PLUS_END   {0, 1, 2, 3, 4, 5, 6, 7, 8, 99}

FilmFormat film_formats[] = {
    {35, "PANO", {0, 37, 73, 108, 142, 175, 207, 238, 268, 297, 325, 352, 378, 403, 427, 450, 472, 493, 513, 532, 550, 800}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 99}, 65.0f, 24.0f},
    // 3x6 sensor[] derived from a per-frame counts/mm regression across the four
    // calibrated 120-film formats (6x4.5, 6x6, 6x7, 6x9), evaluated at 30mm frame
    // pitch + 5mm gap. Leading delta matches 6x7. Pending field verification.
    {36, "3x6", {0, 140, 157, 173, 188, 202, 216, 229, 242, 255, 268, 281, 293, 305, 317, 329, 341, 353, 365, 376, 387, 398, 550}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 99}, 30.0f, 56.0f},
    {645, "6x4.5", {0, 134, 157, 179, 200, 220, 240, 259, 278, 296, 314, 331, 348, 365, 381, 397, 413, 550}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 99}, 42.0f, 56.0f},
    {66, "6x6", {0, 137, 167, 195, 222, 248, 273, 297, 321, 344, 366, 388, 409, 550}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 99}, 56.0f, 56.0f},
    {67, "6x7", {0, 140, 174, 206, 237, 267, 295, 322, 349, 375, 400, 550}, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 99}, 70.0f, 56.0f},
    {93, "9x3", SENSOR_8_FRAMES_120, FRAMES_8_PLUS_END, 90.0f, 30.0f},
    {69, "6x9", SENSOR_8_FRAMES_120, FRAMES_8_PLUS_END, 84.0f, 56.0f}};

#undef SENSOR_8_FRAMES_120
#undef FRAMES_8_PLUS_END

const size_t NUM_FILM_FORMATS = sizeof(film_formats) / sizeof(film_formats[0]);

int getFilmFormatPointCount(const FilmFormat &film_format)
{
  int frame_count = static_cast<int>(sizeof(film_format.sensor) / sizeof(film_format.sensor[0]));
  while (frame_count > 1 &&
         film_format.sensor[frame_count - 1] == 0 &&
         film_format.frame[frame_count - 1] == 0)
  {
    frame_count--;
  }
  return frame_count;
}

int getFilmFormatMaxFrame(const FilmFormat &film_format)
{
  int frame_count = getFilmFormatPointCount(film_format);
  int max_frame = 0;

  for (int i = 0; i < frame_count; i++)
  {
    int frame_value = film_format.frame[i];
    if (frame_value == FILM_COUNTER_END)
    {
      break;
    }
    if (frame_value > max_frame)
    {
      max_frame = frame_value;
    }
  }

  return max_frame;
}
