#pragma once

#include <stddef.h>
#include <stdint.h>

struct MeasurementRecord {
  uint32_t timestamp;
  uint16_t co2;
  float temperature;
  float humidity;
  float battery;
  float supply;
  uint32_t wakeCount;
  uint32_t awakeTimeMilliseconds;
  uint32_t storedRecords;
};

enum class SdStorageAppendResult {
  Stored,
  SkippedInterval,
  Error
};

bool sdStorageBegin();
void sdStorageEnd();
size_t sdStorageCountRecords();
bool sdStorageAppend(const MeasurementRecord& record);
SdStorageAppendResult sdStorageAppendOfflineIfInterval(
    const MeasurementRecord& record, uint32_t minimumIntervalSeconds);
size_t sdStorageLoad(MeasurementRecord* records, size_t maxRecords);
bool sdStorageRemoveFirst(size_t recordCount);
void sdStoragePrintAll();
