#include "scd40_sensor.h"

#include <Arduino.h>
#include <SensirionI2cScd4x.h>
#include <Wire.h>

namespace {
constexpr uint8_t i2cSdaPin = 21;
constexpr uint8_t i2cSclPin = 22;
constexpr uint8_t scd40Address = 0x62;
SensirionI2cScd4x sensor;
bool sensorInitialized = false;
bool periodicMeasurementMayBeRunning = false;
}

bool scd40Begin() {
  Wire.begin(i2cSdaPin, i2cSclPin);
  sensor.begin(Wire, scd40Address);
  sensorInitialized = true;
  periodicMeasurementMayBeRunning = true;

  if (sensor.stopPeriodicMeasurement()) {
    return false;
  }
  periodicMeasurementMayBeRunning = false;

  if (sensor.startPeriodicMeasurement()) {
    return false;
  }
  periodicMeasurementMayBeRunning = true;
  return true;
}

bool scd40StopPeriodicMeasurement() {
  if (!sensorInitialized || !periodicMeasurementMayBeRunning) {
    return true;
  }
  if (sensor.stopPeriodicMeasurement()) {
    return false;
  }
  periodicMeasurementMayBeRunning = false;
  return true;
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
