#include "scd40_sensor.h"

#include <Arduino.h>
#include <SensirionI2cScd4x.h>
#include <Wire.h>

namespace {
constexpr uint8_t i2cSdaPin = 21;
constexpr uint8_t i2cSclPin = 22;
constexpr uint8_t scd40Address = 0x62;
SensirionI2cScd4x sensor;
}

bool scd40Begin() {
  Wire.begin(i2cSdaPin, i2cSclPin);
  sensor.begin(Wire, scd40Address);

  if (sensor.stopPeriodicMeasurement()) {
    return false;
  }

  return sensor.startPeriodicMeasurement() == 0;
}

Scd40ReadStatus scd40ReadMeasurement(uint16_t& co2, float& temperature,
                                     float& humidity) {
  bool dataReady = false;
  if (sensor.getDataReadyStatus(dataReady)) {
    return Scd40ReadStatus::Error;
  }
  if (!dataReady) {
    return Scd40ReadStatus::NotReady;
  }

  if (sensor.readMeasurement(co2, temperature, humidity)) {
    return Scd40ReadStatus::Error;
  }
  return Scd40ReadStatus::Ready;
}
