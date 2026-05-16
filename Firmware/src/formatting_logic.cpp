#include "formatting_logic.h"

#include <stdio.h>

#include "mrfconstants.h"

void cmToReadable(int cm, int places, char *buffer, size_t bufferSize)
{
  if (!buffer || bufferSize == 0)
  {
    return;
  }

  if (cm < CM_PER_METER)
  {
    snprintf(buffer, bufferSize, "%dcm", cm);
  }
  else
  {
    snprintf(buffer, bufferSize, "%.*fm", places, static_cast<float>(cm) / static_cast<float>(CM_PER_METER));
  }
}
