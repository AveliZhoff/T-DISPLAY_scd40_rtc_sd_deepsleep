#include "wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>

#include "device_config.h"

namespace {
constexpr uint32_t connectionTimeoutMs = 5000;
constexpr uint32_t lastAccessPointMarkerValue = 0x57494649UL;

// Последняя успешная точка доступа переживает deep sleep: пробуем ее первой,
// чтобы не тратить до 5 секунд на недоступные сети в начале перебора.
RTC_DATA_ATTR uint32_t lastAccessPointIndex = 0;
RTC_DATA_ATTR uint32_t lastAccessPointMarker = 0;

bool lastAccessPointKnown() {
  return lastAccessPointMarker == lastAccessPointMarkerValue &&
         lastAccessPointIndex < apListSize;
}
}

bool wifiBegin() {
  DeviceConfig deviceConfig = getDeviceConfig();
  WiFi.setHostname(deviceConfig.hostname);
  WiFi.mode(WIFI_STA);

  uint8_t startIndex =
      lastAccessPointKnown() ? static_cast<uint8_t>(lastAccessPointIndex) : 0;

  for (uint8_t offset = 0; offset < apListSize; ++offset) {
    uint8_t index = static_cast<uint8_t>((startIndex + offset) % apListSize);
    WiFi.begin(apList[index].ssid, apList[index].password);
    uint32_t connectionStart = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - connectionStart < connectionTimeoutMs) {
      delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
      lastAccessPointIndex = index;
      lastAccessPointMarker = lastAccessPointMarkerValue;
      return true;
    }

    WiFi.disconnect();
  }

  return false;
}
