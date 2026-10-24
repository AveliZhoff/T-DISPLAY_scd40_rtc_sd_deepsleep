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
// Длина строки записи с запасом: 9 полей, из них 6 с плавающей точкой.
constexpr size_t maxRecordLineLength = 192;
constexpr char recordsPath[] = "/measurements.csv";
constexpr char tempPath[] = "/measurements.tmp";
constexpr char backupPath[] = "/measurements.bak";
SPIClass sdSpi(HSPI);

// Количество записей кэшируется в RTC-памяти: файл меняет только само
// устройство, поэтому полное чтение CSV нужно лишь при холодном старте.
// Рядом с количеством хранится размер файла: если он не совпадает, кэш
// считается устаревшим (подмена карты, обрыв записи) и пересчитывается.
RTC_DATA_ATTR uint32_t cachedRecordCount = 0;
RTC_DATA_ATTR uint32_t cachedRecordCountMarker = 0;
RTC_DATA_ATTR uint32_t cachedFileSize = 0;

struct FileScan {
  size_t validRecords = 0;
  size_t invalidLines = 0;
  bool hasTornTail = false;
};

bool parseRecordLine(const char* line, MeasurementRecord* record) {
  unsigned long timestamp = 0;
  unsigned int co2 = 0;
  float temperature = 0.0f;
  float humidity = 0.0f;
  float battery = 0.0f;
  float supply = 0.0f;
  unsigned long wakeCount = 0;
  unsigned long awakeTimeMilliseconds = 0;
  unsigned long storedRecords = 0;
  int parsed = sscanf(line, "%lu,%u,%f,%f,%f,%f,%lu,%lu,%lu", &timestamp, &co2,
                      &temperature, &humidity, &battery, &supply, &wakeCount,
                      &awakeTimeMilliseconds, &storedRecords);
  if (parsed < 8) {
    return false;
  }
  if (record != nullptr) {
    record->timestamp = static_cast<uint32_t>(timestamp);
    record->co2 = static_cast<uint16_t>(co2);
    record->temperature = temperature;
    record->humidity = humidity;
    record->battery = battery;
    record->supply = supply;
    record->wakeCount = static_cast<uint32_t>(wakeCount);
    record->awakeTimeMilliseconds = static_cast<uint32_t>(awakeTimeMilliseconds);
    record->storedRecords = static_cast<uint32_t>(storedRecords);
  }
  return true;
}

// Читает строку до перевода строки и сообщает, был ли этот перевод.
// Нужно, чтобы отличить целую запись от оборванной потерей питания.
bool readLine(File& file, String& line, bool& terminated) {
  char buffer[maxRecordLineLength];
  size_t length = 0;
  terminated = false;
  bool hasData = false;
  while (file.available()) {
    int symbol = file.read();
    if (symbol < 0) {
      break;
    }
    hasData = true;
    if (symbol == '\n') {
      terminated = true;
      break;
    }
    if (symbol == '\r') {
      continue;
    }
    if (length + 1 < sizeof(buffer)) {
      buffer[length++] = static_cast<char>(symbol);
    }
  }
  if (!hasData) {
    return false;
  }
  buffer[length] = '\0';
  line = String(buffer);
  line.trim();
  return true;
}

size_t fileSize(const char* path) {
  File file = SD.open(path, FILE_READ);
  if (!file) {
    return 0;
  }
  size_t size = file.size();
  file.close();
  return size;
}

bool fileExists(const char* path) {
  File file = SD.open(path, FILE_READ);
  bool exists = static_cast<bool>(file);
  file.close();
  return exists;
}

void invalidateCachedCount() {
  cachedRecordCountMarker = 0;
  cachedFileSize = 0;
}

void storeCachedCount(size_t count, size_t size) {
  cachedRecordCount = static_cast<uint32_t>(count);
  cachedFileSize = static_cast<uint32_t>(size);
  cachedRecordCountMarker = recordCountMarkerValue;
}

bool cachedCountValid() {
  return cachedRecordCountMarker == recordCountMarkerValue;
}

FileScan scanFile(const char* path) {
  FileScan scan;
  File file = SD.open(path, FILE_READ);
  if (!file) {
    return scan;
  }
  String line;
  bool terminated = false;
  while (readLine(file, line, terminated)) {
    if (line.length() == 0) {
      continue;
    }
    if (!terminated) {
      // Хвост без перевода строки: запись оборвана потерей питания.
      scan.hasTornTail = true;
      ++scan.invalidLines;
      continue;
    }
    if (parseRecordLine(line.c_str(), nullptr)) {
      ++scan.validRecords;
    } else {
      ++scan.invalidLines;
    }
  }
  file.close();
  return scan;
}

// Заменяет основной файл подготовленным temp через промежуточную копию:
// при сбое на любом шаге исходные записи остаются на карте.
bool replaceRecordsFile() {
  bool hasOriginal = fileExists(recordsPath);
  if (hasOriginal) {
    SD.remove(backupPath);
    if (!SD.rename(recordsPath, backupPath)) {
      Serial.println("[SD] rename to backup failed");
      return false;
    }
  }
  if (!SD.rename(tempPath, recordsPath)) {
    Serial.println("[SD] rename temp failed");
    if (hasOriginal) {
      SD.rename(backupPath, recordsPath);
    }
    return false;
  }
  if (hasOriginal) {
    SD.remove(backupPath);
  }
  return true;
}

// Возвращает карту в согласованное состояние после потери питания.
// temp пишется полностью до первой замены, поэтому он приоритетнее копии.
void recoverFileState() {
  if (!fileExists(recordsPath)) {
    if (fileExists(tempPath)) {
      Serial.println("[SD] recover from temp file");
      SD.rename(tempPath, recordsPath);
    } else if (fileExists(backupPath)) {
      Serial.println("[SD] recover from backup file");
      SD.rename(backupPath, recordsPath);
    }
  }
  if (fileExists(tempPath)) {
    SD.remove(tempPath);
  }
  if (fileExists(backupPath)) {
    SD.remove(backupPath);
  }
}

// Перезапись файла только корректными записями: убирает оборванные строки,
// из-за которых счётчик расходился с содержимым карты.
size_t repairRecordsFile() {
  File source = SD.open(recordsPath, FILE_READ);
  if (!source) {
    storeCachedCount(0, 0);
    return 0;
  }
  SD.remove(tempPath);
  File target = SD.open(tempPath, FILE_WRITE);
  if (!target) {
    Serial.println("[SD] repair temp open failed");
    source.close();
    invalidateCachedCount();
    return cachedCountValid() ? cachedRecordCount : 0;
  }

  size_t written = 0;
  size_t dropped = 0;
  String line;
  bool terminated = false;
  while (readLine(source, line, terminated)) {
    if (line.length() == 0) {
      continue;
    }
    if (!terminated || !parseRecordLine(line.c_str(), nullptr)) {
      ++dropped;
      continue;
    }
    target.println(line);
    ++written;
  }
  source.close();
  target.flush();
  target.close();
  Serial.printf("[SD] repair: kept=%u dropped=%u\n",
                static_cast<unsigned>(written), static_cast<unsigned>(dropped));

  if (!replaceRecordsFile()) {
    invalidateCachedCount();
    return 0;
  }
  storeCachedCount(written, fileSize(recordsPath));
  return written;
}

size_t refreshCachedCount() {
  FileScan scan = scanFile(recordsPath);
  if (scan.invalidLines > 0) {
    Serial.printf("[SD] invalid lines detected=%u, repairing\n",
                  static_cast<unsigned>(scan.invalidLines));
    return repairRecordsFile();
  }
  storeCachedCount(scan.validRecords, fileSize(recordsPath));
  return cachedRecordCount;
}
}  // namespace

size_t sdStorageCountRecords() {
  if (cachedCountValid() && cachedFileSize == fileSize(recordsPath)) {
    return cachedRecordCount;
  }
  return refreshCachedCount();
}

bool sdStorageBegin() {
  Serial.println("[SD] begin");
  Serial.printf("[SD] SCK=%u MISO=%u MOSI=%u CS=%u speed=%lu\n", sdSckPin,
                sdMisoPin, sdMosiPin, sdCsPin,
                static_cast<unsigned long>(sdSpeed));
  sdSpi.begin(sdSckPin, sdMisoPin, sdMosiPin, sdCsPin);
  if (!SD.begin(sdCsPin, sdSpi, sdSpeed)) {
    Serial.println("[SD] mount failed");
    invalidateCachedCount();
    return false;
  }

  uint8_t cardType = SD.cardType();
  Serial.printf("[SD] mounted, type=%u size=%llu MB\n", cardType,
                SD.cardSize() / (1024ULL * 1024ULL));
  // Кэш счётчика не переживает потерю питания, поэтому при монтировании
  // количество записей всегда сверяется с содержимым карты.
  recoverFileState();
  size_t storedRecords = refreshCachedCount();
  Serial.printf("[SD] stored=%u\n", static_cast<unsigned>(storedRecords));
  return true;
}

bool sdStorageAppend(const MeasurementRecord& record) {
  char line[maxRecordLineLength];
  int lineLength = snprintf(
      line, sizeof(line), "%lu,%u,%.3f,%.3f,%.3f,%.3f,%lu,%lu,%lu\n",
      static_cast<unsigned long>(record.timestamp), record.co2,
      record.temperature, record.humidity, record.battery, record.supply,
      static_cast<unsigned long>(record.wakeCount),
      static_cast<unsigned long>(record.awakeTimeMilliseconds),
      static_cast<unsigned long>(record.storedRecords));
  if (lineLength <= 0 || static_cast<size_t>(lineLength) >= sizeof(line)) {
    Serial.println("[SD] record formatting failed");
    return false;
  }

  size_t sizeBefore = fileSize(recordsPath);
  File file = SD.open(recordsPath, FILE_APPEND);
  if (!file) {
    Serial.println("[SD] append open failed");
    invalidateCachedCount();
    return false;
  }
  size_t written =
      file.write(reinterpret_cast<const uint8_t*>(line), lineLength);
  file.flush();
  file.close();

  size_t sizeAfter = fileSize(recordsPath);
  if (written != static_cast<size_t>(lineLength) ||
      sizeAfter != sizeBefore + static_cast<size_t>(lineLength)) {
    Serial.printf("[SD] append incomplete: wrote=%u expected=%d size=%u\n",
                  static_cast<unsigned>(written), lineLength,
                  static_cast<unsigned>(sizeAfter));
    invalidateCachedCount();
    return false;
  }

  if (cachedCountValid() && cachedFileSize == sizeBefore) {
    storeCachedCount(cachedRecordCount + 1, sizeAfter);
  } else {
    refreshCachedCount();
  }
  Serial.printf("[SD] record appended, stored=%u\n",
                static_cast<unsigned>(cachedRecordCount));
  return true;
}

size_t sdStorageLoad(MeasurementRecord* records, size_t maxRecords) {
  File file = SD.open(recordsPath, FILE_READ);
  if (!file) {
    Serial.println("[SD] no records file");
    storeCachedCount(0, 0);
    return 0;
  }

  size_t count = 0;
  size_t validTotal = 0;
  size_t invalidLines = 0;
  String line;
  bool terminated = false;
  while (readLine(file, line, terminated)) {
    if (line.length() == 0) {
      continue;
    }
    MeasurementRecord parsedRecord = {};
    if (!terminated || !parseRecordLine(line.c_str(), &parsedRecord)) {
      ++invalidLines;
      continue;
    }
    ++validTotal;
    if (count < maxRecords) {
      records[count++] = parsedRecord;
    }
  }
  file.close();

  if (invalidLines > 0) {
    Serial.printf("[SD] invalid records found=%u during load\n",
                  static_cast<unsigned>(invalidLines));
    repairRecordsFile();
  } else {
    storeCachedCount(validTotal, fileSize(recordsPath));
  }
  Serial.printf("[SD] loaded=%u records, stored=%u\n",
                static_cast<unsigned>(count),
                static_cast<unsigned>(cachedRecordCount));
  return count;
}

bool sdStorageRemoveFirst(size_t recordCount) {
  if (recordCount == 0) {
    return true;
  }

  File source = SD.open(recordsPath, FILE_READ);
  if (!source) {
    Serial.println("[SD] remove source open failed");
    storeCachedCount(0, 0);
    return true;
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
  String line;
  bool terminated = false;
  while (readLine(source, line, terminated)) {
    if (line.length() == 0) {
      continue;
    }
    if (skipped < recordCount) {
      ++skipped;
      continue;
    }
    if (!terminated || !parseRecordLine(line.c_str(), nullptr)) {
      continue;
    }
    target.println(line);
    ++remaining;
  }

  source.close();
  target.flush();
  target.close();

  if (!replaceRecordsFile()) {
    Serial.println("[SD] remove failed: records kept");
    invalidateCachedCount();
    return false;
  }
  storeCachedCount(remaining, fileSize(recordsPath));
  Serial.printf("[SD] removed %u records, remaining=%u\n",
                static_cast<unsigned>(recordCount),
                static_cast<unsigned>(remaining));
  return true;
}
