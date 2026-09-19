#pragma once

struct VoltageMeasurements {
  float battery;
  float supply;
  bool isChargerConnected;
};

void voltageSetup();
VoltageMeasurements voltageReadMeasurements();
