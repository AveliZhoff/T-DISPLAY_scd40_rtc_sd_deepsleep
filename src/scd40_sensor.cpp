#include "scd40_sensor.h"

#include <Arduino.h>
#include <SensirionI2cScd4x.h>
#include <Wire.h>

namespace {
constexpr uint8_t i2cSdaPin = 21;
constexpr uint8_t i2cSclPin = 22;
constexpr uint8_t scd40Address = 0x62;
constexpr uint32_t runningMarkerValue = 0x5CD40001UL;
SensirionI2cScd4x sensor;
}

// Признак того, что датчик уже выполняет periodic measurement. Хранится в
// RTC-памяти, поэтому переживает deep sleep; магическое значение защищает от
// мусора после холодного старта.
RTC_DATA_ATTR uint32_t scd40RunningMarker = 0;

bool scd40Begin() {
  Wire.begin(i2cSdaPin, i2cSclPin);
  sensor.begin(Wire, scd40Address);

  // Датчик не останавливается перед сном, поэтому к пробуждению измерение
  // обычно уже готово: перезапускать его (внутри stopPeriodicMeasurement
  // безусловные 500 мс, затем новые 5 с) не нужно.
  bool dataReady = false;
  if (scd40RunningMarker == runningMarkerValue &&
      sensor.getDataReadyStatus(dataReady) == 0 && dataReady) {
    return true;
  }

  if (sensor.stopPeriodicMeasurement()) {
    return false;
  }

  if (sensor.startPeriodicMeasurement()) {
    return false;
  }
  scd40RunningMarker = runningMarkerValue;
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
