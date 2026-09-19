#pragma once

#include <stdint.h>

#include "device_config.h"

struct WiFiAP {
    const char* ssid;
    const char* password;
};

constexpr WiFiAP apListDevice1[] = {
    {"Example-1", "example-password-1"},
    {"Example-2", "example-password-2"},
};

constexpr WiFiAP apListDevice2[] = {
    {"Example-3", "example-password-3"},
    {"Example-4", "example-password-4"},
};

constexpr WiFiAP apListDevice3[] = {
    {"Example-5", "example-password-5"},
    {"Example-6", "example-password-6"},
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
