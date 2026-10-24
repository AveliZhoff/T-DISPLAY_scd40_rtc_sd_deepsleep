// Хост-заглушка Arduino API для прогона логики SD-хранилища вне микроконтроллера.
// Нужна только для тестов test/host: имитирует Serial, String и RTC-память.
#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

// RTC_DATA_ATTR на хосте — отдельная секция, которую тест может обнулить,
// чтобы сымитировать жёсткую перезагрузку или потерю питания.
#define RTC_DATA_ATTR __attribute__((section("rtc_data")))

class String {
 public:
  String() = default;
  String(const char* text) : value_(text ? text : "") {}
  String(const std::string& text) : value_(text) {}

  size_t length() const { return value_.size(); }
  const char* c_str() const { return value_.c_str(); }
  bool isEmpty() const { return value_.empty(); }

  void trim() {
    size_t begin = value_.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
      value_.clear();
      return;
    }
    size_t end = value_.find_last_not_of(" \t\r\n");
    value_ = value_.substr(begin, end - begin + 1);
  }

  void concat(const String& other) { value_ += other.value_; }
  String& operator+=(const String& other) {
    value_ += other.value_;
    return *this;
  }
  bool operator==(const char* other) const { return value_ == other; }

 private:
  std::string value_;
};

class HostSerial {
 public:
  void begin(unsigned long) {}
  void println() { std::printf("\n"); }
  void println(const char* text) { std::printf("%s\n", text); }
  void print(const char* text) { std::printf("%s", text); }
  void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::vprintf(format, args);
    va_end(args);
  }
};

extern HostSerial Serial;

// Обнуление RTC-памяти: имитирует холодный старт после потери питания.
void hostResetRtcMemory();
