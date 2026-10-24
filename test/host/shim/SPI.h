// Хост-заглушка SPI: sd_storage.cpp только запускает шину перед монтированием.
#pragma once

#include <cstdint>

#define HSPI 1

class SPIClass {
 public:
  explicit SPIClass(uint8_t bus = 0) : bus_(bus) {}
  void begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss) {
    (void)sck;
    (void)miso;
    (void)mosi;
    (void)ss;
    (void)bus_;
  }

 private:
  uint8_t bus_;
};
