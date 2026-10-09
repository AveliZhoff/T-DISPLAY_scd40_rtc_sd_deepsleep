#pragma once

#include <stdint.h>

enum class DisplayStatus {
	Idle,
	Connecting,
	Connected,
	Error
};

void displayBegin();
void displayWakeBegin();
void displayResetStatuses();
void displayShowWifiStatus(DisplayStatus status);
void displayShowScdStatus(DisplayStatus status);
void displayShowThingSpeakStatus(DisplayStatus status);
void displayShowSdStatus(DisplayStatus status);
void displayShowRtcTime(const char* dateText, const char* timeText,
						DisplayStatus status);
void displaySetBacklight(bool enabled);
void displayClearMessage();
void displayShowModeMessage(const char* state);
void displayClearMeasurement(uint16_t co2, float temperature, float humidity,
							 const char* dateText, const char* timeText,
							 bool clearRtc);
void displayShowMessage(const char* message, uint8_t font);
void displayShowPreviousMeasurement(uint16_t co2, float temperature,
                                    float humidity);
void displayShowMeasurement(uint16_t co2, float temperature, float humidity);
