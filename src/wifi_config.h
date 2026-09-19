#pragma once

#include <stdint.h>

#include "device_config.h"

struct WiFiAP {
    const char* ssid;
    const char* password;
};

constexpr WiFiAP apListDevice1[] = {
    {"OpenWrt", "zarazo4ka"},
    {"Keenetic-6588", "zarazo4ka"},
};

constexpr WiFiAP apListDevice2[] = {
    {"Sber-Guest", ""},
    {"Pixel_4536", "zarazo4ka"},
};

constexpr WiFiAP apListDevice3[] = {
    {"Keenetic-6588", "zarazo4ka"},
    {"OpenWrt", "zarazo4ka"},
};

static_assert(DEVICE_NUM >= 1 && DEVICE_NUM <= 3,
              "Unsupported DEVICE_NUM: configure a Wi-Fi access point list");

constexpr const WiFiAP* apList =
    DEVICE_NUM == 1 ? apListDevice1
    : DEVICE_NUM == 2 ? apListDevice2
                      : apListDevice3;
constexpr uint8_t apListSize =
    DEVICE_NUM == 1 ? sizeof(apListDevice1) / sizeof(apListDevice1[0])
    : DEVICE_NUM == 2 ? sizeof(apListDevice2) / sizeof(apListDevice2[0])
                       : sizeof(apListDevice3) / sizeof(apListDevice3[0]);
