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
constexpr uint32_t recordCountMarkerValue = 0x53444331UL;
constexpr char recordsPath[] = "/measurements.csv";
constexpr char tempPath[] = "/measurements.tmp";
SPIClass sdSpi(HSPI);

// Количество записей кэшируется в RTC-памяти: файл меняет только само
// устройство, поэтому полное чтение CSV нужно лишь при холодном старте.
RTC_DATA_ATTR uint32_t cachedRecordCount = 0;
RTC_DATA_ATTR uint32_t cachedRecordCountMarker = 0;

size_t countRecordsInFile() {
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

void storeCachedCount(size_t count) {
  cachedRecordCount = static_cast<uint32_t>(count);
  cachedRecordCountMarker = recordCountMarkerValue;
}

size_t refreshCachedCount() {
  storeCachedCount(countRecordsInFile());
  return cachedRecordCount;
}

bool cachedCountValid() {
  return cachedRecordCountMarker == recordCountMarkerValue;
}
}

size_t sdStorageCountRecords() {
  return cachedCountValid() ? cachedRecordCount : refreshCachedCount();
}

bool sdStorageBegin() {
  Serial.println("[SD] begin");
  Serial.printf("[SD] SCK=%u MISO=%u MOSI=%u CS=%u speed=%lu\n", sdSckPin,
                sdMisoPin, sdMosiPin, sdCsPin,
                static_cast<unsigned long>(sdSpeed));
  sdSpi.begin(sdSckPin, sdMisoPin, sdMosiPin, sdCsPin);
  if (!SD.begin(sdCsPin, sdSpi, sdSpeed)) {
    Serial.println("[SD] mount failed");
    cachedRecordCountMarker = 0;
    return false;
  }

  uint8_t cardType = SD.cardType();
  Serial.printf("[SD] mounted, type=%u size=%llu MB\n", cardType,
                SD.cardSize() / (1024ULL * 1024ULL));
  Serial.printf("[SD] stored=%u\n",
                static_cast<unsigned>(refreshCachedCount()));
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
  storeCachedCount(sdStorageCountRecords() + 1);
  Serial.printf("[SD] record appended, stored=%u\n",
                static_cast<unsigned>(cachedRecordCount));
  return true;
}

size_t sdStorageLoad(MeasurementRecord* records, size_t maxRecords) {
  File file = SD.open(recordsPath, FILE_READ);
  if (!file) {
    Serial.println("[SD] no records file");
    storeCachedCount(0);
    return 0;
  }

  size_t count = 0;
  size_t totalLines = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      continue;
    }
    ++totalLines;
    if (count >= maxRecords) {
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
  storeCachedCount(totalLines);
  Serial.printf("[SD] loaded=%u records, stored=%u\n",
                static_cast<unsigned>(count),
                static_cast<unsigned>(totalLines));
  return count;
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
  size_t remaining = 0;
  while (source.available()) {
    String line = source.readStringUntil('\n');
    if (skipped < recordCount) {
      ++skipped;
      continue;
    }
    line.trim();
    if (line.length() == 0) {
      continue;
    }
    target.println(line);
    ++remaining;
  }

  source.close();
  target.close();
  SD.remove(recordsPath);
  bool renamed = SD.rename(tempPath, recordsPath);
  Serial.printf("[SD] removed %u records: %s\n",
                static_cast<unsigned>(recordCount), renamed ? "ok" : "failed");
  if (renamed) {
    storeCachedCount(remaining);
    Serial.printf("[SD] remaining=%u records\n",
                  static_cast<unsigned>(remaining));
  } else {
    cachedRecordCountMarker = 0;
  }
  return renamed;
}
