#include "thingspeak_manager.h"

#include <Arduino.h>
#include <ThingSpeak.h>
#include <WiFi.h>
#include <time.h>

#include "device_config.h"

namespace {
WiFiClient client;
DeviceConfig deviceConfig;
RTC_DATA_ATTR uint32_t lastSendAttemptTimestamp = 0;
uint32_t minimumUpdateIntervalMs = 60000;
bool simulateErrorOnce = false;

void appendRecordJson(String& body, const MeasurementRecord& record,
                      bool& firstRecord) {
  time_t timestamp = record.timestamp;
  struct tm timeInfo;
  gmtime_r(&timestamp, &timeInfo);
  char timestampText[25];
  strftime(timestampText, sizeof(timestampText), "%Y-%m-%dT%H:%M:%SZ",
           &timeInfo);

  if (!firstRecord) {
    body += ",";
  }
  firstRecord = false;
  body += "{\"created_at\":\"";
  body += timestampText;
  body += "\",\"field1\":\"";
  body += static_cast<float>(record.co2);
  body += "\",\"field2\":\"";
  body += record.temperature;
  body += "\",\"field3\":\"";
  body += record.humidity;
  body += "\",\"field4\":\"";
  body += record.battery;
  body += "\",\"field5\":\"";
  body += record.supply;
  body += "\",\"field6\":\"";
  body += record.wakeCount;
  body += "\",\"field7\":\"";
  body += record.awakeTimeMilliseconds;
  body += "\",\"field8\":\"";
  body += record.storedRecords;
  body += "\"}";
}

void printRecordForSend(const char* label, const MeasurementRecord& record) {
  Serial.printf(
      "[TS] %s timestamp=%lu co2=%u temperature=%.3f humidity=%.3f "
      "battery=%.3f supply=%.3f wake=%lu awake_ms=%lu stored=%lu\n",
      label, static_cast<unsigned long>(record.timestamp), record.co2,
      record.temperature, record.humidity, record.battery, record.supply,
      static_cast<unsigned long>(record.wakeCount),
      static_cast<unsigned long>(record.awakeTimeMilliseconds),
      static_cast<unsigned long>(record.storedRecords));
}

bool writeBulk(const MeasurementRecord& current,
               const MeasurementRecord* pending, size_t pendingCount) {
  String body = "{\"write_api_key\":\"";
  body += deviceConfig.writeApiKey;
  body += "\",\"updates\":[";
  bool firstRecord = true;
  for (size_t index = 0; index < pendingCount; ++index) {
    appendRecordJson(body, pending[index], firstRecord);
  }
  appendRecordJson(body, current, firstRecord);
  body += "]}";

  String debugBody = body;
  const String apiKeyPrefix = "\"write_api_key\":\"";
  int apiKeyStart = debugBody.indexOf(apiKeyPrefix);
  if (apiKeyStart >= 0) {
    apiKeyStart += apiKeyPrefix.length();
    int apiKeyEnd = debugBody.indexOf('\"', apiKeyStart);
    if (apiKeyEnd > apiKeyStart) {
      debugBody = debugBody.substring(0, apiKeyStart) + "<redacted>" +
                  debugBody.substring(apiKeyEnd);
    }
  }
  Serial.printf("[TS] payload records=%u bytes=%u\n",
                static_cast<unsigned>(pendingCount + 1),
                static_cast<unsigned>(body.length()));
  for (size_t index = 0; index < pendingCount; ++index) {
    char label[24];
    snprintf(label, sizeof(label), "pending[%u]",
             static_cast<unsigned>(index));
    printRecordForSend(label, pending[index]);
  }
  printRecordForSend("current", current);
  Serial.printf("[TS] payload=%s\n", debugBody.c_str());

  if (!client.connect("api.thingspeak.com", 80)) {
    return false;
  }
  client.print("POST /channels/");
  client.print(deviceConfig.channelNumber);
  client.print("/bulk_update.json HTTP/1.1\r\nHost: api.thingspeak.com\r\n");
  client.print("Content-Type: application/json\r\nContent-Length: ");
  client.print(body.length());
  client.print("\r\nConnection: close\r\n\r\n");
  client.print(body);

  uint32_t deadline = millis() + 10000;
  while (!client.available() && static_cast<int32_t>(deadline - millis()) > 0) {
    delay(10);
  }
  bool success = false;
  if (client.available()) {
    String statusLine = client.readStringUntil('\n');
    success = statusLine.indexOf(" 200 ") >= 0 ||
          statusLine.indexOf(" 202 ") >= 0;
  }
  client.stop();
  return success;
}
}

bool thingSpeakBegin() {
  deviceConfig = getDeviceConfig();
  if (deviceConfig.channelNumber == 0 || deviceConfig.writeApiKey[0] == '\0') {
    return false;
  }

  ThingSpeak.begin(client);
  return true;
}

void thingSpeakSetSendInterval(uint32_t intervalSeconds) {
  minimumUpdateIntervalMs = intervalSeconds * 1000UL;
}

ThingSpeakSendStatus thingSpeakSendMeasurement(
  const MeasurementRecord& record, const MeasurementRecord* pending,
  size_t pendingCount) {
  uint32_t minimumUpdateIntervalSeconds = minimumUpdateIntervalMs / 1000UL;
  if (lastSendAttemptTimestamp != 0 &&
      record.timestamp <
          lastSendAttemptTimestamp + minimumUpdateIntervalSeconds) {
    return ThingSpeakSendStatus::NotAttempted;
  }
  if (WiFi.status() != WL_CONNECTED) {
    lastSendAttemptTimestamp = record.timestamp;
    Serial.println("[TS] WiFi unavailable; storing measurement for retry");
    return ThingSpeakSendStatus::Error;
  }

  Serial.printf("[TS] bulk send: pending=%u total=%u\n",
                static_cast<unsigned>(pendingCount),
                static_cast<unsigned>(pendingCount + 1));
  if (simulateErrorOnce) {
    simulateErrorOnce = false;
    Serial.println("[TS] simulated send error");
    return ThingSpeakSendStatus::Error;
  }

  bool success = writeBulk(record, pending, pendingCount);
  lastSendAttemptTimestamp = record.timestamp;
  return success ? ThingSpeakSendStatus::Success : ThingSpeakSendStatus::Error;
}

void thingSpeakSetSimulateErrorOnce(bool enabled) {
  simulateErrorOnce = enabled;
  Serial.printf("[TS] simulate error once=%s\n", enabled ? "true" : "false");
}
