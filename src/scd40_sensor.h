#pragma once

#include <stdint.h>

bool scd40Begin();

enum class Scd40ReadStatus {
	Ready,
	NotReady,
	Error
};

Scd40ReadStatus scd40ReadMeasurement(uint16_t& co2, float& temperature,
									 float& humidity);
