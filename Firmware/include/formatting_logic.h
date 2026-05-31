#ifndef FORMATTING_LOGIC_H
#define FORMATTING_LOGIC_H

#include <stddef.h>

// Render a distance in centimetres into a short human-readable string. Below
// 1 m the value prints as "%dcm"; at or above 1 m it prints as a float with
// `places` decimal places suffixed with "m". Does nothing if buffer is null
// or empty; truncates instead of overflowing.
void cmToReadable(int cm, int places, char *buffer, size_t bufferSize);

#endif // FORMATTING_LOGIC_H
