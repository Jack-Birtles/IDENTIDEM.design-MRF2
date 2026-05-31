#ifndef INTERFACE_MENU_HELPERS_H
#define INTERFACE_MENU_HELPERS_H

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>

// Display helpers shared between the main UI (interface.cpp) and the
// configuration / calibration / health / reset screens
// (interface_config_screens.cpp). Each function operates on the primary OLED
// via the globals owned by hardware.cpp; callers do not need to thread any
// state through.

void prepareU8g2TextDefaults(U8G2_FOR_ADAFRUIT_GFX &u8g2_ref,
                             uint16_t foregroundColor,
                             uint16_t backgroundColor);
void preparePrimaryDisplayTextMode();

void setMenuItemColors(bool selected);
void printSignedInt(int value);
void printSignedDeciDegrees(int deciDegrees);

int getConfigMenuRowY(int step);
void beginConfigMenuScreen(const __FlashStringHelper *title);
void selectConfigMenuRow(int step, bool selected);

void drawCalibProgressBar(int current, int total);

#endif // INTERFACE_MENU_HELPERS_H
