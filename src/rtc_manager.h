#pragma once

#include <stddef.h>

#include <stdint.h>

enum class RtcStatus {
  Ready,
  NeedsSync,
  Error
};

RtcStatus rtcBegin(bool checkPowerLoss, char* dateText, size_t dateTextSize,
                   char* timeText, size_t timeTextSize);
bool rtcSyncFromNtp(char* dateText, size_t dateTextSize, char* timeText,
                    size_t timeTextSize);
uint32_t rtcUnixTime();
