#include "wifi_manager.h"

#include <Arduino.h>
#include <WiFi.h>

#include "device_config.h"

namespace {
constexpr uint32_t connectionTimeoutMs = 5000;
}

bool wifiBegin() {
  DeviceConfig deviceConfig = getDeviceConfig();
  WiFi.setHostname(deviceConfig.hostname);
  WiFi.mode(WIFI_STA);

  for (uint8_t index = 0; index < apListSize; ++index) {
    WiFi.begin(apList[index].ssid, apList[index].password);
    uint32_t connectionStart = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - connectionStart < connectionTimeoutMs) {
      delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }

    WiFi.disconnect();
  }

  return false;
}
