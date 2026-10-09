#pragma once

#include <stdint.h>

constexpr uint8_t DEVICE_NUM = 1;

struct DeviceConfig {
  const char* hostname;
  unsigned long channelNumber;
  const char* writeApiKey;
  uint32_t deepSleepIntervalSeconds;
  uint32_t thingSpeakSendIntervalSeconds;
};

inline DeviceConfig getDeviceConfig() {
  const char* hostname = "";
  unsigned long myChannelNumber = 0;
  const char* myWriteAPIKey = "";
  uint32_t deepSleepIntervalSeconds = 30;
  uint32_t thingSpeakSendIntervalSeconds = 300;

  switch (DEVICE_NUM) {
    case 1:
      hostname = "esp32-scd40-01";
      myChannelNumber = 1000001;
      myWriteAPIKey = "YOUR_DEVICE_1_WRITE_API_KEY";
      deepSleepIntervalSeconds = 30;
      thingSpeakSendIntervalSeconds = 300;
      break;
    case 2:
      hostname = "esp32-scd40-02";
      myChannelNumber = 1000002;
      myWriteAPIKey = "YOUR_DEVICE_2_WRITE_API_KEY";
      deepSleepIntervalSeconds = 30;
      thingSpeakSendIntervalSeconds = 300;
      break;
    case 3:
      hostname = "esp32-scd40-03";
      myChannelNumber = 1000003;
      myWriteAPIKey = "YOUR_DEVICE_3_WRITE_API_KEY";
      deepSleepIntervalSeconds = 30;
      thingSpeakSendIntervalSeconds = 300;
      break;
    default:
      break;
  }

  return {hostname, myChannelNumber, myWriteAPIKey,
          deepSleepIntervalSeconds, thingSpeakSendIntervalSeconds};
}

struct WiFiAP {
    const char* ssid;
    const char* password;
};

constexpr WiFiAP apListDevice1[] = {
    {"Example-1", "example-password-1"},
    {"Example-2", "example-password-2"},
};

constexpr WiFiAP apListDevice2[] = {
    {"Example-3", "example-password-3"},
    {"Example-4", "example-password-4"},
};

constexpr WiFiAP apListDevice3[] = {
    {"Example-5", "example-password-5"},
    {"Example-6", "example-password-6"},
};

static_assert(DEVICE_NUM >= 1 && DEVICE_NUM <= 3,
              "Unsupported DEVICE_NUM: configure a Wi-Fi access point list");

constexpr const WiFiAP* apList =
    DEVICE_NUM == 1 ? apListDevice1
    : DEVICE_NUM == 2 ? apListDevice2
                      : apListDevice3;
constexpr uint8_t apListSize =
    DEVICE_NUM == 1 ? sizeof(apListDevice1) / sizeof(apListDevice1[0])
    : DEVICE_NUM == 2 ? sizeof(apListDevice2) / sizeof(apListDevice2[0])
                       : sizeof(apListDevice3) / sizeof(apListDevice3[0]);
