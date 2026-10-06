#include "voltage_sensor.h"

#include <Arduino.h>

namespace {
constexpr uint8_t adcPin = 34;
constexpr uint8_t adcEnablePin = 14;
constexpr float usbVoltageMin = 4.5f;
bool adcEnableState = false;

float readVoltage() {
  return static_cast<float>(analogRead(adcPin)) * 7.26f / 4095.0f;
}
}

void voltageSetup() {
  pinMode(adcPin, INPUT);
  pinMode(adcEnablePin, OUTPUT);
  digitalWrite(adcEnablePin, LOW);
}

VoltageMeasurements voltageReadMeasurements() {
  adcEnableState = false;
  digitalWrite(adcEnablePin, LOW);
  delay(2);
  float usbVoltage = readVoltage();

  adcEnableState = true;
  digitalWrite(adcEnablePin, HIGH);
  delay(2);
  float batteryVoltage = readVoltage();

  adcEnableState = false;
  digitalWrite(adcEnablePin, LOW);
  return {batteryVoltage, usbVoltage, usbVoltage >= usbVoltageMin};
}
