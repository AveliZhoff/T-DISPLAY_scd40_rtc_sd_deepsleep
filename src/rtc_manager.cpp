#include "rtc_manager.h"

#include <Arduino.h>
#include <RTClib.h>
#include <time.h>
#include <Wire.h>

namespace {
constexpr uint8_t i2cSdaPin = 21;
constexpr uint8_t i2cSclPin = 22;
constexpr uint32_t moscowUtcOffsetSeconds = 3 * 60 * 60;
constexpr char moscowTimeZone[] = "MSK-3";
RTC_DS3231 rtc;

const char* ntpServers[] = {
    "192.168.1.252",
    "192.168.1.254",
    "pool.ntp.org",
    "time.nist.gov",
    "time.google.com",
    "ru.pool.ntp.org",
    "de.pool.ntp.org",
    "0.openwrt.pool.ntp.org",
    "1.openwrt.pool.ntp.org",
    "2.openwrt.pool.ntp.org",
    "3.openwrt.pool.ntp.org",
};

constexpr size_t ntpServerCount = sizeof(ntpServers) / sizeof(ntpServers[0]);

void formatDateTime(const DateTime& dateTime, char* dateText, size_t dateTextSize,
                    char* timeText, size_t timeTextSize) {
  snprintf(dateText, dateTextSize, "%02u.%02u.%04u", dateTime.day(),
           dateTime.month(), dateTime.year());
  snprintf(timeText, timeTextSize, "%02u:%02u:%02u", dateTime.hour(),
           dateTime.minute(), dateTime.second());
}
}

RtcStatus rtcBegin(bool checkPowerLoss, char* dateText, size_t dateTextSize,
                   char* timeText, size_t timeTextSize) {
  Wire.begin(i2cSdaPin, i2cSclPin);
  if (!rtc.begin()) {
    return RtcStatus::Error;
  }

  formatDateTime(rtc.now(), dateText, dateTextSize, timeText, timeTextSize);
  if (checkPowerLoss && rtc.lostPower()) {
    return RtcStatus::NeedsSync;
  }

  return RtcStatus::Ready;
}

bool rtcSyncFromNtp(char* dateText, size_t dateTextSize, char* timeText,
                    size_t timeTextSize) {
  struct tm timeInfo;

  for (size_t index = 0; index < ntpServerCount; ++index) {
    configTzTime(moscowTimeZone, ntpServers[index]);
    if (!getLocalTime(&timeInfo, 10000)) {
      continue;
    }

    DateTime synchronizedTime(
        timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
        timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    rtc.adjust(synchronizedTime);
    formatDateTime(synchronizedTime, dateText, dateTextSize, timeText,
             timeTextSize);
    return true;
  }

  return false;
}

uint32_t rtcUnixTime() {
  // DS3231 stores Moscow local time; ThingSpeak timestamps must be UTC.
  return rtc.now().unixtime() - moscowUtcOffsetSeconds;
}
