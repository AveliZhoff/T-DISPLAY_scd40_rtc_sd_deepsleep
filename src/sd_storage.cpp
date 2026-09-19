#include "sd_storage.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

namespace {
constexpr uint8_t sdMisoPin = 26;
constexpr uint8_t sdMosiPin = 15;
constexpr uint8_t sdSckPin = 13;
constexpr uint8_t sdCsPin = 2;
constexpr uint32_t sdSpeed = 1000000;
constexpr char recordsPath[] = "/measurements.csv";
constexpr char tempPath[] = "/measurements.tmp";
SPIClass sdSpi(HSPI);
}

size_t sdStorageCountRecords() {
  File file = SD.open(recordsPath, FILE_READ);
  if (!file) {
    return 0;
  }
  size_t count = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      ++count;
    }
  }
  file.close();
  return count;
}

bool sdStorageBegin() {
  Serial.println("[SD] begin");
  Serial.printf("[SD] SCK=%u MISO=%u MOSI=%u CS=%u speed=%lu\n", sdSckPin,
                sdMisoPin, sdMosiPin, sdCsPin,
                static_cast<unsigned long>(sdSpeed));
  sdSpi.begin(sdSckPin, sdMisoPin, sdMosiPin, sdCsPin);
  if (!SD.begin(sdCsPin, sdSpi, sdSpeed)) {
    Serial.println("[SD] mount failed");
    return false;
  }

  uint8_t cardType = SD.cardType();
  Serial.printf("[SD] mounted, type=%u size=%llu MB\n", cardType,
                SD.cardSize() / (1024ULL * 1024ULL));
  return true;
}

bool sdStorageAppend(const MeasurementRecord& record) {
  File file = SD.open(recordsPath, FILE_APPEND);
  if (!file) {
    Serial.println("[SD] append open failed");
    return false;
  }

  file.printf("%lu,%u,%.3f,%.3f,%.3f,%.3f,%lu,%lu,%lu\n",
              static_cast<unsigned long>(record.timestamp), record.co2,
              record.temperature, record.humidity, record.battery, record.supply,
              static_cast<unsigned long>(record.wakeCount),
              static_cast<unsigned long>(record.awakeTimeMilliseconds),
              static_cast<unsigned long>(record.storedRecords));
  file.close();
  Serial.printf("[SD] record appended, stored=%u\n",
                static_cast<unsigned>(sdStorageCountRecords()));
  return true;
}

size_t sdStorageLoad(MeasurementRecord* records, size_t maxRecords) {
  File file = SD.open(recordsPath, FILE_READ);
  if (!file) {
    Serial.println("[SD] no records file");
    return 0;
  }

  size_t count = 0;
  while (count < maxRecords && file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      continue;
    }

    unsigned long timestamp = 0;
    unsigned int co2 = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
    float battery = 0.0f;
    float supply = 0.0f;
    unsigned long wakeCount = 0;
    unsigned long awakeTimeMilliseconds = 0;
    unsigned long storedRecords = 0;
    int parsed = sscanf(line.c_str(), "%lu,%u,%f,%f,%f,%f,%lu,%lu,%lu",
                        &timestamp, &co2, &temperature, &humidity, &battery,
              &supply, &wakeCount, &awakeTimeMilliseconds,
              &storedRecords);
    if (parsed < 8) {
      Serial.println("[SD] invalid record skipped");
      continue;
    }

    records[count++] = {static_cast<uint32_t>(timestamp),
                        static_cast<uint16_t>(co2), temperature, humidity,
                        battery, supply, static_cast<uint32_t>(wakeCount),
                        static_cast<uint32_t>(awakeTimeMilliseconds),
                        static_cast<uint32_t>(storedRecords)};
  }
  file.close();
  Serial.printf("[SD] loaded=%u records, stored=%u\n",
                static_cast<unsigned>(count),
                static_cast<unsigned>(sdStorageCountRecords()));
  return count;
}

void sdStoragePrintAll() {
  File file = SD.open(recordsPath, FILE_READ);
  if (!file) {
    Serial.println("[SD] no records to print");
    return;
  }

  size_t recordNumber = 0;
  Serial.println("[SD] all stored measurements:");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      continue;
    }
    ++recordNumber;
    Serial.printf("[SD] #%u %s\n", static_cast<unsigned>(recordNumber),
                  line.c_str());
  }
  file.close();
  Serial.printf("[SD] total stored=%u\n",
                static_cast<unsigned>(recordNumber));
}

bool sdStorageRemoveFirst(size_t recordCount) {
  File source = SD.open(recordsPath, FILE_READ);
  if (!source) {
    Serial.println("[SD] remove source open failed");
    return recordCount == 0;
  }

  SD.remove(tempPath);
  File target = SD.open(tempPath, FILE_WRITE);
  if (!target) {
    Serial.println("[SD] temp open failed");
    source.close();
    return false;
  }

  size_t skipped = 0;
  while (source.available()) {
    String line = source.readStringUntil('\n');
    if (skipped < recordCount) {
      ++skipped;
      continue;
    }
    target.println(line);
  }

  source.close();
  target.close();
  SD.remove(recordsPath);
  bool renamed = SD.rename(tempPath, recordsPath);
  Serial.printf("[SD] removed %u records: %s\n",
                static_cast<unsigned>(recordCount), renamed ? "ok" : "failed");
  if (renamed) {
    Serial.printf("[SD] remaining=%u records\n",
                  static_cast<unsigned>(sdStorageCountRecords()));
  }
  return renamed;
}
