#include <Arduino.h>
#include <esp_sleep.h>

#include "display.h"
#include "scd40_sensor.h"
#include "thingspeak_manager.h"
#include "rtc_manager.h"
#include "sd_storage.h"
#include "voltage_sensor.h"
#include "wifi_manager.h"

#define DEEP_SLEEP_INTERVAL_SECONDS 60
#define THINGSPEAK_SEND_INTERVAL_SECONDS 600
#define DISABLE_THINGSPEAK_SEND 0

constexpr gpio_num_t BUTTON_1_PIN = GPIO_NUM_0;
constexpr gpio_num_t BUTTON_2_PIN = GPIO_NUM_35;

RTC_DATA_ATTR uint32_t wakeCount = 0;
RTC_DATA_ATTR bool previousChargerConnected = false;
RTC_DATA_ATTR bool previousDisplayValid = false;
RTC_DATA_ATTR float previousDisplayTemperature = 0.0f;
RTC_DATA_ATTR float previousDisplayHumidity = 0.0f;
RTC_DATA_ATTR bool previousMeasurementCo2Valid = false;
RTC_DATA_ATTR uint16_t previousMeasurementCo2 = 0;
RTC_DATA_ATTR float previousMeasurementTemperature = 0.0f;
RTC_DATA_ATTR float previousMeasurementHumidity = 0.0f;
RTC_DATA_ATTR float previousMeasurementBattery = 0.0f;
RTC_DATA_ATTR float previousMeasurementSupply = 0.0f;
RTC_DATA_ATTR bool previousDisplayRtcValid = false;
RTC_DATA_ATTR char previousDisplayDate[16] = "";
RTC_DATA_ATTR char previousDisplayTime[16] = "";
uint32_t lastMeasurement = 0;
uint32_t wakeStartTime = 0;
bool sdReady = false;
bool isDeepSleepWake = false;
bool displayFullInit = true;
bool displayEnabled = true;
bool wifiConnected = false;
esp_sleep_wakeup_cause_t wakeupCause = ESP_SLEEP_WAKEUP_UNDEFINED;

void enterDeepSleep(bool keepDisplayOn) {
  if (displayEnabled) {
    displaySetBacklight(keepDisplayOn);
  }
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT);
  if (esp_sleep_enable_timer_wakeup(
          static_cast<uint64_t>(DEEP_SLEEP_INTERVAL_SECONDS) * 1000000ULL) !=
      ESP_OK) {
    Serial.println("[SLEEP] timer wakeup setup failed");
  }
  if (esp_sleep_enable_ext0_wakeup(BUTTON_1_PIN, 0) != ESP_OK) {
    Serial.println("[SLEEP] button 1 wakeup setup failed");
  }
  if (esp_sleep_enable_ext1_wakeup(1ULL << BUTTON_2_PIN,
                                   ESP_EXT1_WAKEUP_ALL_LOW) != ESP_OK) {
    Serial.println("[SLEEP] button 2 wakeup setup failed");
  }
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n[BOOT] start");
  wakeStartTime = millis();
  ++wakeCount;
  wakeupCause = esp_sleep_get_wakeup_cause();
  isDeepSleepWake = esp_reset_reason() == ESP_RST_DEEPSLEEP;
  Serial.printf("[BOOT] wakeup cause=%d deep-sleep=%s\n",
                static_cast<int>(wakeupCause),
                isDeepSleepWake ? "true" : "false");
  if (wakeupCause == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("[BOOT] wakeup by button 1 (GPIO0)");
  } else if (wakeupCause == ESP_SLEEP_WAKEUP_EXT1) {
    Serial.printf("[BOOT] wakeup by button 2 (GPIO35), status=%d\n",
                  digitalRead(BUTTON_2_PIN));
  }
  voltageSetup();
  VoltageMeasurements bootVoltage = voltageReadMeasurements();
  bool canWakeDisplay = isDeepSleepWake &&
                        bootVoltage.isChargerConnected &&
                        previousChargerConnected;
  bool timerWakeWithoutPower =
      wakeupCause == ESP_SLEEP_WAKEUP_TIMER && !bootVoltage.isChargerConnected;
  displayEnabled = !timerWakeWithoutPower;
  displayFullInit = !canWakeDisplay && displayEnabled;
  if (displayFullInit) {
    displayBegin();
    displayShowWifiStatus(DisplayStatus::Connecting);
  } else if (displayEnabled) {
    displayWakeBegin();
  }

  wifiConnected = wifiBegin();
  if (!wifiConnected) {
    if (displayFullInit) {
      displayShowWifiStatus(DisplayStatus::Error);
    }
    Serial.println("[WIFI] unavailable; continuing in offline mode");
  } else if (displayFullInit) {
    displayShowWifiStatus(DisplayStatus::Connected);
  }

  char rtcDateText[16];
  char rtcTimeText[16];
  RtcStatus rtcStatus = rtcBegin(!isDeepSleepWake, rtcDateText,
                                 sizeof(rtcDateText), rtcTimeText,
                                 sizeof(rtcTimeText));
  if (rtcStatus == RtcStatus::Error) {
    if (!isDeepSleepWake) {
      displayShowMessage("RTC ERROR", 2);
    }
    enterDeepSleep(false);
    return;
  }
  if (rtcStatus == RtcStatus::NeedsSync) {
    if (!wifiConnected) {
      Serial.println("[RTC] NTP sync skipped; using RTC time offline");
    } else if (!isDeepSleepWake) {
      displayShowRtcTime(rtcDateText, rtcTimeText, DisplayStatus::Error);
      if (!rtcSyncFromNtp(rtcDateText, sizeof(rtcDateText), rtcTimeText,
                          sizeof(rtcTimeText))) {
        displayShowMessage("NTP ERROR", 2);
        enterDeepSleep(false);
        return;
      }
    }
  }
  if (!isDeepSleepWake) {
    displayShowRtcTime(rtcDateText, rtcTimeText, DisplayStatus::Connected);
  }

  if (displayFullInit) {
    displayShowThingSpeakStatus(DisplayStatus::Connecting);
  }
  if (!thingSpeakBegin()) {
    if (displayFullInit) {
      displayShowThingSpeakStatus(DisplayStatus::Error);
      displayShowMessage("THINGSPEAK ERROR", 2);
    }
    enterDeepSleep(false);
    return;
  }
  thingSpeakSetSendInterval(THINGSPEAK_SEND_INTERVAL_SECONDS);
  if (displayFullInit) {
    displayShowThingSpeakStatus(wifiConnected ? DisplayStatus::Connected
                                               : DisplayStatus::Error);
  }

  sdReady = sdStorageBegin();
  Serial.printf("[SD] ready=%s\n", sdReady ? "true" : "false");
  if (displayFullInit) {
    displayShowSdStatus(sdReady ? DisplayStatus::Connected
                                : DisplayStatus::Error);
  }
  if (sdReady) {
    sdStoragePrintAll();
  }

  if (!isDeepSleepWake) {
    displayShowScdStatus(DisplayStatus::Connecting);
  }
  if (!scd40Begin()) {
    if (displayFullInit) {
      displayShowScdStatus(DisplayStatus::Error);
      displayShowMessage("SCD40 ERROR", 2);
    }
    enterDeepSleep(false);
    return;
  }
  if (displayFullInit) {
    displayShowScdStatus(DisplayStatus::Connected);
  }

  if (displayFullInit) {
    displayShowMessage("W8 ...", 3);
    displayShowRtcTime(rtcDateText, rtcTimeText, DisplayStatus::Connected);
  }
}

void loop() {
  if (millis() - lastMeasurement < 1000) {
    return;
  }
  lastMeasurement = millis();

  uint16_t co2 = 0;
  float temperature = 0.0f;
  float humidity = 0.0f;
  Scd40ReadStatus status =
      scd40ReadMeasurement(co2, temperature, humidity);
  if (status == Scd40ReadStatus::NotReady) {
    return;
  }
  if (status == Scd40ReadStatus::Error) {
    if (displayFullInit) {
      displayShowMessage("SCD40 ERROR", 2);
    }
    return;
  }

  VoltageMeasurements voltage = voltageReadMeasurements();
  uint16_t averagedCo2 = co2;
  float averagedTemperature = temperature;
  float averagedHumidity = humidity;
  float averagedBattery = voltage.battery;
  float averagedSupply = voltage.supply;
  if (previousMeasurementCo2Valid) {
    averagedCo2 = static_cast<uint16_t>(
        (static_cast<uint32_t>(co2) + previousMeasurementCo2) / 2);
    averagedTemperature =
        (temperature + previousMeasurementTemperature) / 2.0f;
    averagedHumidity = (humidity + previousMeasurementHumidity) / 2.0f;
    averagedBattery = (voltage.battery + previousMeasurementBattery) / 2.0f;
    averagedSupply = (voltage.supply + previousMeasurementSupply) / 2.0f;
  }
  previousMeasurementTemperature = temperature;
  previousMeasurementHumidity = humidity;
  previousMeasurementBattery = voltage.battery;
  previousMeasurementSupply = voltage.supply;
  previousMeasurementCo2Valid = true;

  previousChargerConnected = voltage.isChargerConnected;
  uint32_t awakeTimeMilliseconds = millis() - wakeStartTime;
  MeasurementRecord currentRecord = {
      rtcUnixTime(), averagedCo2, averagedTemperature, averagedHumidity,
      averagedBattery, averagedSupply, wakeCount, awakeTimeMilliseconds,
      static_cast<uint32_t>(sdReady ? sdStorageCountRecords() : 0)};
  MeasurementRecord pendingRecords[11];
  size_t pendingCount = 0;

  if (displayEnabled && isDeepSleepWake && previousDisplayValid) {
    displayClearMeasurement(previousMeasurementCo2, previousDisplayTemperature,
                            previousDisplayHumidity, previousDisplayDate,
                            previousDisplayTime, previousDisplayRtcValid);
  }
  if (displayEnabled) {
    displayShowMeasurement(averagedCo2, averagedTemperature, averagedHumidity);
    previousDisplayTemperature = averagedTemperature;
    previousDisplayHumidity = averagedHumidity;
    previousDisplayRtcValid = false;
    previousDisplayValid = true;
  }
  previousMeasurementCo2 = averagedCo2;

#if DISABLE_THINGSPEAK_SEND
  if (sdReady && sdStorageAppend(currentRecord)) {
    Serial.println("[SD] measurement stored; ThingSpeak send disabled");
  } else {
    Serial.println("[SD] measurement store failed");
  }
  enterDeepSleep(voltage.isChargerConnected);
  return;
#endif

  size_t actualStoredCount = sdReady ? sdStorageCountRecords() : 0;
  pendingCount = sdReady ? sdStorageLoad(pendingRecords, 11) : 0;
  size_t expectedPendingCount = actualStoredCount < 11 ? actualStoredCount : 11;
  currentRecord.storedRecords = static_cast<uint32_t>(actualStoredCount);
  Serial.printf("[SD] before send actual=%u loaded=%u expected_loaded=%u\n",
                static_cast<unsigned>(actualStoredCount),
                static_cast<unsigned>(pendingCount),
                static_cast<unsigned>(expectedPendingCount));
  Serial.printf("[SD] send queue consistency=%s\n",
                pendingCount == expectedPendingCount ? "ok" : "mismatch");
  Serial.printf("[SD] payload will contain %u stored + 1 current measurement\n",
                static_cast<unsigned>(pendingCount));
  Serial.printf("[TS] sending %u stored + 1 current measurement\n",
                static_cast<unsigned>(pendingCount));
  ThingSpeakSendStatus sendStatus =
      thingSpeakSendMeasurement(currentRecord, pendingRecords, pendingCount);
  Serial.printf("[TS] send result=%s\n",
                sendStatus == ThingSpeakSendStatus::Success ? "success"
                : sendStatus == ThingSpeakSendStatus::Error ? "error"
                                                            : "not attempted");
  if (sendStatus == ThingSpeakSendStatus::Success) {
    if (displayFullInit) {
      displayShowThingSpeakStatus(DisplayStatus::Connected);
    }
    if (sdReady && pendingCount > 0 && !sdStorageRemoveFirst(pendingCount)) {
      if (displayEnabled) {
        displayShowSdStatus(DisplayStatus::Error);
      }
    } else if (sdReady) {
      Serial.printf("[SD] deleted after successful send=%u records\n",
                    static_cast<unsigned>(pendingCount));
    }
  } else if (sendStatus == ThingSpeakSendStatus::Error) {
    if (displayFullInit) {
      displayShowThingSpeakStatus(DisplayStatus::Error);
    }
    if (sdReady && !sdStorageAppend(currentRecord)) {
      if (displayFullInit) {
        displayShowSdStatus(DisplayStatus::Error);
      }
    }
  }

  enterDeepSleep(voltage.isChargerConnected);
}