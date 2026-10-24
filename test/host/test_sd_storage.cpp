// Хост-тесты логики SD-хранилища: счётчик записей и очередь неотправленных
// измерений. Прогоняются на ПК под g++, без платы: SD-карта и RTC-память
// имитируются заглушками из test/host/shim.
#include <Arduino.h>
#include <SD.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "sd_storage.h"

namespace {

constexpr char kRecordsPath[] = "/measurements.csv";
int failures = 0;
int checks = 0;

void check(bool condition, const char* name) {
  ++checks;
  if (!condition) {
    ++failures;
    std::printf("  FAIL: %s\n", name);
  } else {
    std::printf("  ok:   %s\n", name);
  }
}

void checkEqual(size_t actual, size_t expected, const char* name) {
  ++checks;
  if (actual != expected) {
    ++failures;
    std::printf("  FAIL: %s (ожидалось %zu, получено %zu)\n", name, expected,
                actual);
  } else {
    std::printf("  ok:   %s = %zu\n", name, actual);
  }
}

std::string recordLine(uint32_t index, bool withNewline = true) {
  char buffer[160];
  std::snprintf(buffer, sizeof(buffer),
                "%lu,%u,%.3f,%.3f,%.3f,%.3f,%lu,%lu,%lu%s",
                1700000000UL + index * 60, 400 + index, 21.5f, 41.25f, 3.9f,
                5.0f, static_cast<unsigned long>(index + 1),
                static_cast<unsigned long>(5200UL + index),
                static_cast<unsigned long>(index), withNewline ? "\n" : "");
  return std::string(buffer);
}

std::string validFile(size_t records) {
  std::string content;
  for (size_t index = 0; index < records; ++index) {
    content += recordLine(static_cast<uint32_t>(index));
  }
  return content;
}

size_t countLines(const std::string& content) {
  size_t lines = 0;
  for (char character : content) {
    if (character == '\n') {
      ++lines;
    }
  }
  return lines;
}

size_t countCompleteLines(const std::string& content) {
  size_t lines = 0;
  size_t start = 0;
  while (start < content.size()) {
    size_t end = content.find('\n', start);
    if (end == std::string::npos) {
      break;
    }
    std::string line = content.substr(start, end - start);
    if (!line.empty()) {
      ++lines;
    }
    start = end + 1;
  }
  return lines;
}

MeasurementRecord makeRecord(uint32_t index) {
  MeasurementRecord record = {};
  record.timestamp = 1700000000UL + index * 60;
  record.co2 = static_cast<uint16_t>(400 + index);
  record.temperature = 21.5f;
  record.humidity = 41.25f;
  record.battery = 3.9f;
  record.supply = 5.0f;
  record.wakeCount = index + 1;
  record.awakeTimeMilliseconds = 5200;
  record.storedRecords = index;
  return record;
}

// Холодный старт: RTC-память обнуляется, как после потери питания.
void coldStart() { hostResetRtcMemory(); }

// Пробуждение из deep sleep: RTC-память сохраняется.
void deepSleepWake() {}

void testColdStartWithData() {
  std::printf("[1] Холодный старт: счётчик восстанавливается из файла\n");
  hostfs::reset();
  coldStart();
  hostfs::setFile(kRecordsPath, validFile(5));
  check(sdStorageBegin(), "карта смонтирована");
  checkEqual(sdStorageCountRecords(), 5, "счётчик после холодного старта");
}

void testCountAcrossDeepSleep() {
  std::printf("[2] Цикл deep sleep: счётчик растёт на каждой записи\n");
  hostfs::reset();
  coldStart();
  hostfs::setFile(kRecordsPath, validFile(2));
  check(sdStorageBegin(), "карта смонтирована");
  check(sdStorageAppend(makeRecord(2)), "запись добавлена");
  deepSleepWake();
  check(sdStorageBegin(), "карта смонтирована после пробуждения");
  check(sdStorageAppend(makeRecord(3)), "запись добавлена после пробуждения");
  checkEqual(sdStorageCountRecords(), 4, "счётчик");
  checkEqual(countLines(hostfs::content(kRecordsPath)), 4,
             "строк в файле");
}

void testPowerLossRecount() {
  std::printf("[3] Потеря питания: счётчик не обнуляется\n");
  hostfs::reset();
  coldStart();
  hostfs::setFile(kRecordsPath, validFile(7));
  check(sdStorageBegin(), "карта смонтирована");
  checkEqual(sdStorageCountRecords(), 7, "счётчик после потери питания");
  check(sdStorageAppend(makeRecord(7)), "запись добавлена");
  checkEqual(sdStorageCountRecords(), 8, "счётчик после записи");
}

void testTornLineRepair() {
  std::printf("[4] Оборванная запись (питание пропало при записи)\n");
  hostfs::reset();
  coldStart();
  // Файл: три целые записи и оборванная четвёртая без перевода строки.
  hostfs::setFile(kRecordsPath, validFile(3) + "1700000180,412,21.5,41.2");
  check(sdStorageBegin(), "карта смонтирована");
  checkEqual(sdStorageCountRecords(), 3, "счётчик без оборванной строки");
  const std::string repaired = hostfs::content(kRecordsPath);
  checkEqual(countCompleteLines(repaired), 3, "в файле три целые записи");
  check(!repaired.empty() && repaired.back() == '\n',
        "файл заканчивается переводом строки");

  MeasurementRecord records[11];
  size_t loaded = sdStorageLoad(records, 11);
  checkEqual(loaded, 3, "загружено записей");
  check(sdStorageRemoveFirst(loaded), "отправленные записи удалены");
  checkEqual(sdStorageCountRecords(), 0, "очередь пуста");
  checkEqual(hostfs::content(kRecordsPath).size(), 0, "файл пуст");

  // Следующий цикл не должен «застрять» на неразбираемой строке.
  deepSleepWake();
  check(sdStorageBegin(), "карта смонтирована");
  checkEqual(sdStorageCountRecords(), 0, "счётчик на следующем цикле");
}

void testStaleCacheByFileSize() {
  std::printf("[5] Подмена карты: кэш сверяется с размером файла\n");
  hostfs::reset();
  coldStart();
  hostfs::setFile(kRecordsPath, validFile(2));
  check(sdStorageBegin(), "карта смонтирована");
  checkEqual(sdStorageCountRecords(), 2, "счётчик из кэша");
  // Карту заменили: в файле уже 7 записей, кэш в RTC остался прежним.
  hostfs::setFile(kRecordsPath, validFile(7));
  checkEqual(sdStorageCountRecords(), 7, "счётчик перечитан из файла");
}

void testRemoveFirstAtomicFailure() {
  std::printf("[6] Сбой замены файла при удалении отправленных записей\n");
  hostfs::reset();
  coldStart();
  hostfs::setFile(kRecordsPath, validFile(4));
  check(sdStorageBegin(), "карта смонтирована");
  hostfs::failRenameTo = kRecordsPath;  // падает последний rename
  bool removed = sdStorageRemoveFirst(2);
  hostfs::failRenameTo.clear();
  check(!removed, "удаление сообщило об ошибке");
  checkEqual(countCompleteLines(hostfs::content(kRecordsPath)), 4,
             "записи не потеряны");
}

void testTornAppendDetected() {
  std::printf("[7] Обрыв записи при добавлении\n");
  hostfs::reset();
  coldStart();
  hostfs::setFile(kRecordsPath, validFile(2));
  check(sdStorageBegin(), "карта смонтирована");
  hostfs::tornWriteLimit = 12;  // до карты доехала только часть строки
  bool appended = sdStorageAppend(makeRecord(2));
  hostfs::tornWriteLimit = static_cast<size_t>(-1);
  check(!appended, "добавление сообщило об ошибке");
  coldStart();  // потеря питания посреди записи
  check(sdStorageBegin(), "карта смонтирована после перезагрузки");
  checkEqual(sdStorageCountRecords(), 2, "счётчик не считает обрывок");
  const std::string repairedAfterAppend = hostfs::content(kRecordsPath);
  checkEqual(countCompleteLines(repairedAfterAppend), 2,
             "в файле две целые записи");
  check(!repairedAfterAppend.empty() && repairedAfterAppend.back() == '\n',
        "обрывок удалён из файла");
}

void testLoadValidRecordsOnly() {
  std::printf("[8] Загрузка очереди: только корректные записи\n");
  hostfs::reset();
  coldStart();
  hostfs::setFile(kRecordsPath, validFile(2) + "\n" + validFile(3));
  check(sdStorageBegin(), "карта смонтирована");
  checkEqual(sdStorageCountRecords(), 5, "счётчик");
  MeasurementRecord records[11];
  checkEqual(sdStorageLoad(records, 11), 5, "загружено записей");
  checkEqual(records[1].co2, 401, "поле CO2 второй записи");
  checkEqual(records[4].timestamp, 1700000000UL + 2 * 60,
             "время пятой записи");
}

void testManyCyclesStayConsistent() {
  std::printf("[9] Двадцать циклов: счётчик совпадает с файлом\n");
  hostfs::reset();
  coldStart();
  check(sdStorageBegin(), "карта смонтирована");
  for (uint32_t cycle = 0; cycle < 20; ++cycle) {
    if (cycle % 4 == 3) {
      MeasurementRecord records[11];
      size_t loaded = sdStorageLoad(records, 11);
      if (loaded > 0) {
        sdStorageRemoveFirst(loaded);
      }
      deepSleepWake();
    } else {
      sdStorageAppend(makeRecord(cycle));
      deepSleepWake();
    }
    check(sdStorageCountRecords() == countLines(hostfs::content(kRecordsPath)),
          "счётчик равен числу строк");
  }
}

void testEmptyFileAfterRemovingAll() {
  std::printf("[10] Полная отправка очереди\n");
  hostfs::reset();
  coldStart();
  hostfs::setFile(kRecordsPath, validFile(3));
  check(sdStorageBegin(), "карта смонтирована");
  MeasurementRecord records[11];
  size_t loaded = sdStorageLoad(records, 11);
  checkEqual(loaded, 3, "загружено записей");
  check(sdStorageRemoveFirst(loaded), "записи удалены");
  checkEqual(sdStorageCountRecords(), 0, "счётчик после удаления");
  check(sdStorageAppend(makeRecord(99)), "новая запись добавлена");
  checkEqual(sdStorageCountRecords(), 1, "счётчик");
}

}  // namespace

int main() {
  testColdStartWithData();
  testCountAcrossDeepSleep();
  testPowerLossRecount();
  testTornLineRepair();
  testStaleCacheByFileSize();
  testRemoveFirstAtomicFailure();
  testTornAppendDetected();
  testLoadValidRecordsOnly();
  testManyCyclesStayConsistent();
  testEmptyFileAfterRemovingAll();

  std::printf("\nИтог: %d проверок, ошибок %d\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
