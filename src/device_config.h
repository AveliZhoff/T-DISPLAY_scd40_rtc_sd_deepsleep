#pragma once

#include <stdint.h>

constexpr uint8_t DEVICE_NUM = 3;

struct DeviceConfig {
  const char* hostname;
  unsigned long channelNumber;
  const char* writeApiKey;
};

inline DeviceConfig getDeviceConfig() {
  const char* hostname = "";
  unsigned long myChannelNumber = 0;
  const char* myWriteAPIKey = "";

  switch (DEVICE_NUM) {
    case 1:
      hostname = "esp32-scd40-01";
      myChannelNumber = 1448907;
      myWriteAPIKey = "6JHWQUA3D77IGQAW";
      break;
    case 2:
      hostname = "esp32-scd40-02";
      myChannelNumber = 2398760;
      myWriteAPIKey = "CK60HE8LCLTCELI0";
      break;
    case 3:
      hostname = "esp32-scd40-03";
      myChannelNumber = 2401929;
      myWriteAPIKey = "QEKNQE7HLVMRXRX4";
      break;
    default:
      break;
  }

  return {hostname, myChannelNumber, myWriteAPIKey};
}
