#include "display.h"

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <driver/gpio.h>

namespace {
TFT_eSPI display;
DisplayStatus wifiStatus = DisplayStatus::Idle;
DisplayStatus scdStatus = DisplayStatus::Idle;
DisplayStatus thingSpeakStatus = DisplayStatus::Idle;
DisplayStatus sdStatus = DisplayStatus::Idle;
bool hasPreviousMeasurement = false;
uint16_t previousCo2 = 0;
float previousTemperature = 0.0f;
float previousHumidity = 0.0f;
bool hasStartupMessage = false;
char startupMessage[32] = "";
uint8_t startupMessageFont = 0;
char previousRtcDate[16] = "";
char previousRtcTime[16] = "";
constexpr uint8_t rtcFont = 4;
constexpr int16_t rtcDateY = 56;
constexpr int16_t rtcTimeY = 84;
constexpr uint8_t statusLabelFont = 4;
constexpr int16_t statusLabelY = 2;
constexpr int16_t co2ValueX = 80;
constexpr int16_t co2UnitX = 170;
constexpr int16_t co2Y = 60;

int16_t statusLabelX(uint8_t column) {
  constexpr const char* labels[] = {"WIFI", "SCD", "TS", "SD"};
  constexpr uint8_t labelCount = sizeof(labels) / sizeof(labels[0]);
  int16_t totalLabelWidth = 0;
  int16_t precedingLabelWidth = 0;

  for (uint8_t index = 0; index < labelCount; ++index) {
    int16_t labelWidth = display.textWidth(labels[index], statusLabelFont);
    totalLabelWidth += labelWidth;
    if (index < column) {
      precedingLabelWidth += labelWidth;
    }
  }

  int16_t gap =
      (display.width() - totalLabelWidth) / (labelCount + 1);
  return gap + precedingLabelWidth + column * gap;
}

uint16_t statusColor(DisplayStatus status) {
  switch (status) {
    case DisplayStatus::Connecting:
      return TFT_YELLOW;
    case DisplayStatus::Connected:
      return TFT_GREEN;
    case DisplayStatus::Error:
      return TFT_RED;
    case DisplayStatus::Idle:
    default:
      return TFT_DARKGREY;
  }
}

void drawStatusLabel(const char* label, uint16_t color, uint8_t column) {
  display.setTextDatum(TL_DATUM);
  display.setTextColor(color, TFT_BLACK);
  display.drawString(label, statusLabelX(column), statusLabelY,
                     statusLabelFont);
}

void drawFrame() {
  display.drawRect(0, 0, display.width(), display.height(), TFT_BLUE);
}
}

void displayBegin() {
  gpio_deep_sleep_hold_dis();
  display.init();
  display.setRotation(1);
  display.fillScreen(TFT_BLACK);
  displaySetBacklight(true);
  drawFrame();
  drawStatusLabel("WIFI", statusColor(wifiStatus), 0);
  drawStatusLabel("SCD", statusColor(scdStatus), 1);
  drawStatusLabel("TS", statusColor(thingSpeakStatus), 2);
  drawStatusLabel("SD", statusColor(sdStatus), 3);
}

void displayWakeBegin() {
  gpio_deep_sleep_hold_dis();
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  display.setRotation(1);
  gpio_hold_dis(GPIO_NUM_4);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
  gpio_hold_en(GPIO_NUM_4);
  gpio_deep_sleep_hold_en();
  digitalWrite(TFT_CS, LOW);
  digitalWrite(TFT_DC, LOW);
  SPI.transfer(0x11);
  digitalWrite(TFT_CS, HIGH);
}

void displayResetStatuses() {
  displayShowWifiStatus(DisplayStatus::Idle);
  displayShowScdStatus(DisplayStatus::Idle);
  displayShowThingSpeakStatus(DisplayStatus::Idle);
  displayShowSdStatus(DisplayStatus::Idle);
}

void displayShowWifiStatus(DisplayStatus status) {
  wifiStatus = status;
  drawStatusLabel("WIFI", TFT_BLACK, 0);
  drawStatusLabel("WIFI", statusColor(wifiStatus), 0);
}

void displayShowScdStatus(DisplayStatus status) {
  scdStatus = status;
  drawStatusLabel("SCD", TFT_BLACK, 1);
  drawStatusLabel("SCD", statusColor(scdStatus), 1);
}

void displayShowThingSpeakStatus(DisplayStatus status) {
  thingSpeakStatus = status;
  drawStatusLabel("TS", TFT_BLACK, 2);
  drawStatusLabel("TS", statusColor(thingSpeakStatus), 2);
}

void displayShowSdStatus(DisplayStatus status) {
  sdStatus = status;
  drawStatusLabel("SD", TFT_BLACK, 3);
  drawStatusLabel("SD", statusColor(sdStatus), 3);
}

void displayShowRtcTime(const char* dateText, const char* timeText,
                        DisplayStatus status) {
  display.setTextDatum(MC_DATUM);
  if (previousRtcDate[0] != '\0') {
    display.setTextColor(TFT_BLACK, TFT_BLACK);
    display.drawString(previousRtcDate, display.width() / 2, rtcDateY,
               rtcFont);
  }
  if (previousRtcTime[0] != '\0') {
    display.setTextColor(TFT_BLACK, TFT_BLACK);
    display.drawString(previousRtcTime, display.width() / 2, rtcTimeY,
               rtcFont);
  }
  display.setTextColor(statusColor(status), TFT_BLACK);
  display.drawString(dateText, display.width() / 2, rtcDateY, rtcFont);
  display.drawString(timeText, display.width() / 2, rtcTimeY, rtcFont);
  snprintf(previousRtcDate, sizeof(previousRtcDate), "%s", dateText);
  snprintf(previousRtcTime, sizeof(previousRtcTime), "%s", timeText);
}

void displaySetBacklight(bool enabled) {
  pinMode(TFT_BL, OUTPUT);
  gpio_hold_dis(GPIO_NUM_4);
  digitalWrite(TFT_BL, enabled ? TFT_BACKLIGHT_ON : !TFT_BACKLIGHT_ON);
  gpio_hold_en(GPIO_NUM_4);
  gpio_deep_sleep_hold_en();
}

void displayClearMeasurement(uint16_t co2, float temperature, float humidity,
                             const char* dateText, const char* timeText,
                             bool clearRtc) {
  char co2Text[12];
  char temperatureText[16];
  char humidityText[16];
  snprintf(co2Text, sizeof(co2Text), "%u", co2);
  snprintf(temperatureText, sizeof(temperatureText), "%.1f C", temperature);
  snprintf(humidityText, sizeof(humidityText), "%.1f %%", humidity);

  display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_BLACK, TFT_BLACK);
  display.drawString(co2Text, co2ValueX, co2Y, 6);
  display.drawString("ppm", co2UnitX, co2Y, 4);
  display.drawString(temperatureText, 70, 108, 4);
  display.drawString(humidityText, 170, 108, 4);

  if (clearRtc) {
    display.drawString(dateText, display.width() / 2, rtcDateY, rtcFont);
    display.drawString(timeText, display.width() / 2, rtcTimeY, rtcFont);
  }
}

void displayShowMessage(const char* message, uint8_t font) {
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextDatum(MC_DATUM);
  display.drawString(message, display.width() / 2, display.height() / 2, font);
  drawFrame();
  drawStatusLabel("WIFI", statusColor(wifiStatus), 0);
  drawStatusLabel("SCD", statusColor(scdStatus), 1);
  drawStatusLabel("TS", statusColor(thingSpeakStatus), 2);
  drawStatusLabel("SD", statusColor(sdStatus), 3);
  snprintf(startupMessage, sizeof(startupMessage), "%s", message);
  startupMessageFont = font;
  hasStartupMessage = true;
}

void displayShowMeasurement(uint16_t co2, float temperature, float humidity) {
  if (hasPreviousMeasurement && previousCo2 == co2 &&
      previousTemperature == temperature && previousHumidity == humidity) {
    return;
  }

  char co2ValueText[12];
  char temperatureText[16];
  char humidityText[16];
  snprintf(co2ValueText, sizeof(co2ValueText), "%u", co2);
  snprintf(temperatureText, sizeof(temperatureText), "%.1f C", temperature);
  snprintf(humidityText, sizeof(humidityText), "%.1f %%", humidity);

  display.setTextDatum(MC_DATUM);

  if (hasStartupMessage) {
    display.setTextColor(TFT_BLACK, TFT_BLACK);
    display.drawString(startupMessage, display.width() / 2,
                       display.height() / 2, startupMessageFont);
    hasStartupMessage = false;
  }

  if (hasPreviousMeasurement) {
    char previousCo2ValueText[12];
    char previousTemperatureText[16];
    char previousHumidityText[16];
    snprintf(previousCo2ValueText, sizeof(previousCo2ValueText), "%u",
         previousCo2);
    snprintf(previousTemperatureText, sizeof(previousTemperatureText),
             "%.1f C", previousTemperature);
    snprintf(previousHumidityText, sizeof(previousHumidityText), "%.1f %%",
             previousHumidity);

    display.setTextColor(TFT_BLACK, TFT_BLACK);
    display.drawString(previousCo2ValueText, co2ValueX, co2Y, 6);
    display.drawString(previousTemperatureText, 70, 108, 4);
    display.drawString(previousHumidityText, 170, 108, 4);
  }

  if (previousRtcDate[0] != '\0') {
    display.setTextColor(TFT_BLACK, TFT_BLACK);
    display.drawString(previousRtcDate, display.width() / 2, rtcDateY,
               rtcFont);
    display.drawString(previousRtcTime, display.width() / 2, rtcTimeY,
               rtcFont);
  }

  uint16_t co2Color = TFT_GREEN;
  if (co2 >= 1200) {
    co2Color = TFT_RED;
  } else if (co2 >= 800) {
    co2Color = TFT_YELLOW;
  }

  uint16_t temperatureColor = TFT_GREEN;
  if (temperature < 18.0f) {
    temperatureColor = TFT_BLUE;
  } else if (temperature >= 24.0f) {
    temperatureColor = TFT_RED;
  }

  uint16_t humidityColor = TFT_RED;
  if (humidity >= 30.0f && humidity < 40.0f) {
    humidityColor = TFT_YELLOW;
  } else if (humidity >= 40.0f && humidity < 60.0f) {
    humidityColor = TFT_GREEN;
  } else if (humidity >= 60.0f && humidity < 70.0f) {
    humidityColor = TFT_YELLOW;
  }

  display.setTextColor(co2Color, TFT_BLACK);
  display.drawString(co2ValueText, co2ValueX, co2Y, 6);
  display.drawString("ppm", co2UnitX, co2Y, 4);
  display.setTextColor(temperatureColor, TFT_BLACK);
  display.drawString(temperatureText, 70, 108, 4);
  display.setTextColor(humidityColor, TFT_BLACK);
  display.drawString(humidityText, 170, 108, 4);

  previousCo2 = co2;
  previousTemperature = temperature;
  previousHumidity = humidity;
  hasPreviousMeasurement = true;
}
