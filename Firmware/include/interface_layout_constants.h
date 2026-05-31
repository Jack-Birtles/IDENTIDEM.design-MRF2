#ifndef INTERFACE_LAYOUT_CONSTANTS_H
#define INTERFACE_LAYOUT_CONSTANTS_H

// Pixel coordinates for the on-OLED user interface. These are display-only
// concerns; nothing outside interface.cpp needs them. Kept out of
// mrfconstants.h so a layout tweak doesn't force a full-tree rebuild and so
// the central constants file stays focused on cross-module behaviour tuning.

// ---------------------------------------------------------------------------
// Main display layout coordinates
// ---------------------------------------------------------------------------
const int MAIN_HEADER_HEIGHT = 15;         // Header bar height.
const int MAIN_RETICLE_CENTER_Y_OFFSET = 5; // Reticle center vertical offset.
const int MAIN_RETICLE_CENTER_RADIUS = 3;  // Reticle center dot radius.
const int MAIN_ISO_X = 2;                  // ISO label X position.
const int MAIN_ISO_Y = 7;                  // ISO label Y position.
const int MAIN_APERTURE_X = 46;            // Aperture label X position.
const int MAIN_APERTURE_Y = 7;             // Aperture label Y position.
const int MAIN_SHUTTER_X = 2;              // Shutter label X position.
const int MAIN_SHUTTER_Y = 14;             // Shutter label Y position.
const int MAIN_DISTANCE_X = 68;            // LiDAR distance label X position.
const int MAIN_DISTANCE_Y = 7;             // LiDAR distance label Y position.
const int MAIN_LENS_X = 68;                // Lens distance label X position.
const int MAIN_LENS_Y = 14;                // Lens distance label Y position.
const int MAIN_LIDAR_QUALITY_X = 123;      // LiDAR quality indicator origin X.
const int MAIN_LIDAR_QUALITY_Y = 2;        // LiDAR quality indicator origin Y.
const int MAIN_LIDAR_QUALITY_SIZE = 2;     // LiDAR quality block size.
const int MAIN_LIDAR_QUALITY_GAP = 1;      // Gap between LiDAR quality blocks.
const int MAIN_SUNLIGHT_ICON_CX = 116;     // Centre X of the high-sunlight indicator (sits in the gap before the quality blocks).
const int MAIN_SUNLIGHT_ICON_CY = 4;       // Centre Y of the high-sunlight indicator.

// ---------------------------------------------------------------------------
// Setup/calibration/health UI layout coordinates
// ---------------------------------------------------------------------------
const int CONFIG_TITLE_X = 3;              // Setup title X position.
const int CONFIG_TITLE_Y = 15;             // Setup title Y position.
const int CONFIG_HEADER_PADDING_Y = 4;     // Vertical padding below setup title.
const int CONFIG_ITEM_X = 3;               // Setup item X position.
const int CONFIG_ITEM_Y_START = 22;        // Setup item Y start.
const int CONFIG_ITEM_Y_STEP = 9;          // Setup item Y spacing.
const int CONFIG_FOOTER_Y = 121;           // Setup footer Y position.
const int CALIB_TITLE_X = 3;               // Calibration title X position.
const int CALIB_TITLE_Y = 15;              // Calibration title Y position.
const int CALIB_ITEM_X = 3;                // Calibration item X position.
const int CALIB_LENS_Y = 35;               // Calibration lens line Y position.
const int CALIB_DISTANCE_Y = 47;           // Calibration distance line Y position.
const int CALIB_PROGRESS_BAR_Y = 53;       // Calibration progress bar Y (below distance line).
const int CALIB_HELP_Y1 = 70;              // Calibration help line 1 Y.
const int CALIB_HELP_Y2 = 78;              // Calibration help line 2 Y.
const int CALIB_HELP_Y3 = 86;              // Calibration help line 3 Y.
const int HEALTH_TITLE_X = 3;              // Health title X position.
const int HEALTH_TITLE_Y = 15;             // Health title Y position.
const int HEALTH_ITEM_X = 3;               // Health item X position.
const int HEALTH_ITEM_Y_START = 30;        // Health item start Y.
const int HEALTH_ITEM_Y_STEP = 10;         // Health item line spacing.
const int HEALTH_FOOTER_Y = 112;           // Health footer Y position.

// ---------------------------------------------------------------------------
// External display layout coordinates
// ---------------------------------------------------------------------------
const int EXT_HEADER_HEIGHT = 10;          // External header bar height.
const int EXT_HEADER_FORMAT_X = 2;         // Format label X.
const int EXT_HEADER_FORMAT_Y = 8;         // Format label Y.
const int EXT_HEADER_DIVIDER_X = 33;       // Header divider X.
const int EXT_HEADER_LENS_X = 37;          // Lens label X.
const int EXT_HEADER_LENS_Y = 8;           // Lens label Y.
const int EXT_BATTERY_DIVIDER_FULL_X = 100; // Battery divider X when >=100%.
const int EXT_BATTERY_CURSOR_FULL_X = 104;  // Battery cursor X when >=100%.
const int EXT_BATTERY_DIVIDER_LOW_X = 111;  // Battery divider X for low percentage layout.
const int EXT_BATTERY_CURSOR_LOW_X = 115;   // Battery cursor X for low percentage layout.
const int EXT_BATTERY_DIVIDER_MID_X = 103;  // Battery divider X for mid percentage layout.
const int EXT_BATTERY_CURSOR_MID_X = 107;   // Battery cursor X for mid percentage layout.
const int EXT_PROGRESS_BAR_WIDTH = 90;      // Progress bar width.
const int EXT_PROGRESS_BAR_HEIGHT = 17;     // Progress bar height.
const int EXT_PROGRESS_BAR_X = 34;          // Progress bar X origin.
const int EXT_PROGRESS_BAR_Y = 15;          // Progress bar Y origin.
const float PERCENT_SCALE = 100.0f;         // Percentage scaling helper.
const int EXT_COUNTER_TEXT_Y = 30;          // Counter text baseline Y.
const int EXT_COUNTER_MESSAGE_X = 8;        // "Load film"/"Roll end" message X.
const int EXT_COUNTER_VALUE_X_WITH_PROGRESS = 8; // Counter X when progress bar is visible.
const int EXT_COUNTER_VALUE_X_NO_PROGRESS = 60;  // Counter X when no progress bar is visible.
const int EXT_SLEEP_FACE_CX     = 52;       // Sleep indicator face centre X.
const int EXT_SLEEP_FACE_CY     = 16;       // Sleep indicator face centre Y.
const int EXT_SLEEP_FACE_RADIUS = 12;       // Sleep indicator face radius (px).
const int EXT_SLEEP_ZZZ_X       = 70;       // Sleep indicator "Zzz" text cursor X.
const int EXT_SLEEP_ZZZ_Y       = 22;       // Sleep indicator "Zzz" text cursor Y.

#endif // INTERFACE_LAYOUT_CONSTANTS_H
