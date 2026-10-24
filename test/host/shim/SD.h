// Хост-заглушка SD-библиотеки: файловая система в памяти с возможностью
// сымитировать потерю питания посреди записи и сбой rename.
#pragma once

#include <Arduino.h>
#include <SPI.h>

#include <cstdint>
#include <cstdio>
#include <map>
#include <string>

#define FILE_READ 0
#define FILE_WRITE 1
#define FILE_APPEND 2

namespace hostfs {
extern std::map<std::string, std::string> files;
// true — следующий rename завершится ошибкой (имитация сбоя в «окне» замены файла).
extern bool failNextRename;
// Непустая строка — rename с таким приёмником один раз завершится ошибкой.
extern std::string failRenameTo;
// Сколько байт из следующей записи реально доедет до карты (SIZE_MAX — без обрыва).
extern size_t tornWriteLimit;

void reset();
void setFile(const std::string& path, const std::string& content);
std::string content(const std::string& path);
bool exists(const std::string& path);
}  // namespace hostfs

class File {
 public:
  File() = default;
  File(std::string path, int mode);

  operator bool() const { return open_; }
  bool available();
  int read();
  String readStringUntil(char delimiter);
  size_t size() const;
  size_t write(const uint8_t* data, size_t length);
  size_t print(const char* text);
  size_t println(const char* text);
  size_t println(const String& text);
  size_t printf(const char* format, ...);
  void close();
  void flush() {}

 private:
  size_t writeBytes(const std::string& data);

  std::string path_;
  int mode_ = FILE_READ;
  size_t position_ = 0;
  bool open_ = false;
};

class HostSDClass {
 public:
  bool begin(uint8_t cs, SPIClass& spi, uint32_t speed);
  uint8_t cardType() const { return mounted_ ? 1 : 0; }
  uint64_t cardSize() const { return mounted_ ? 1024ULL * 1024ULL * 1024ULL : 0; }
  File open(const char* path, int mode = FILE_READ);
  bool remove(const char* path);
  bool rename(const char* from, const char* to);
  bool mounted() const { return mounted_; }

 private:
  bool mounted_ = true;
};

extern HostSDClass SD;
