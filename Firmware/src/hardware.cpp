#include "hardware.h"
#include "mrfconstants.h" // For pin/address constants and screen dimensions

#include <Arduino.h> // For HardwareSerial
#include <Wire.h>    // For I2C devices & display

#include <Adafruit_seesaw.h>
#include <seesaw_neopixel.h> // Correct header for seesaw_NeoPixel
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_MAX1704X.h>
#include <BH1750.h>
#include <DTS6012M_UART.h>
#include <Adafruit_SH110X.h> // For SH1107
#include <Adafruit_SSD1306.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <Bounce2.h>

// Hardware init
// ---------------------
// Inputs
Adafruit_seesaw encoder;
Adafruit_ADS1015 theads;
Adafruit_MPU6050 mpu;
seesaw_NeoPixel sspixel = seesaw_NeoPixel(NEOPIXEL_COUNT, SS_NEOPIX, NEO_GRB + NEO_KHZ800);
Bounce2::Button lbutton = Bounce2::Button();
Bounce2::Button rbutton = Bounce2::Button();

// Battery gauge
Adafruit_MAX17048 maxlipo;
// Lightmeter
BH1750 lightMeter;
// LiDAR setup
namespace
{
HardwareSerial lidarSerial(LIDAR_SERIAL_PORT); // Using serial port 2

DTSConfig makeLidarConfig()
{
  DTSConfig config;
  config.baudRate = LIDAR_BAUD_RATE;
  config.timeout_ms = LIDAR_NO_DATA_TIMEOUT_MS;
  config.minValidDistance_mm = DISTANCE_MIN * LIDAR_DISTANCE_DIVISOR;
  // Sensor's rated range, not the frameline parallax cap (DISTANCE_MAX = 18m).
  // From library 3.0.0 maxValidDistance_mm gates quality grading and the median
  // filter: a genuine far return beyond it grades POOR and is dropped from the
  // smoothed distance. The camera's own 18m display-to-"Inf." policy lives in
  // LIDAR_DISPLAY_INF_THRESHOLD_CM, so let the library trust the full 20m.
  config.maxValidDistance_mm = LIDAR_LIBRARY_MAX_VALID_DISTANCE_MM;
  config.minIntensityThreshold = LIDAR_LIBRARY_MIN_INTENSITY_THRESHOLD;
  config.crcByteOrder = DTSCRCByteOrder::AUTO;
  config.crcAutoSwitchErrorThreshold = 10;
  // Ambient-light gate (library 3.0.0) left OFF deliberately. The outdoor-range
  // investigation found sunlightBase reads a flat ~13 outdoors — it measures
  // ambient at the sensor aperture behind the bandpass filter, not the solar
  // 905nm reflecting off the sunlit target that actually causes the range
  // cliff. Gating on it would reject valid frames without fixing the cliff.
  config.maxSunlightBase = 0;
  return config;
}
} // namespace

DTS6012M_UART lidar(lidarSerial, makeLidarConfig());
// Display setup
Adafruit_SH1107 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, DISPLAY_I2C_FREQUENCY_HZ, SHARED_I2C_FREQUENCY_HZ);
Adafruit_SSD1306 display_ext(SCREEN_WIDTH, SCREEN_HEIGHT_EXT, &Wire, OLED_RESET);
U8G2_FOR_ADAFRUIT_GFX u8g2;
U8G2_FOR_ADAFRUIT_GFX u8g2_ext;
// ---------------------
