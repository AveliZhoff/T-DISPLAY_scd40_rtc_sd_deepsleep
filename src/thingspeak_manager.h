#pragma once

#include <stddef.h>
#include <stdint.h>

#include "sd_storage.h"

bool thingSpeakBegin();
void thingSpeakSetSendInterval(uint32_t intervalSeconds);
bool thingSpeakSendDue(uint32_t recordTimestamp);
void thingSpeakSetSimulateErrorOnce(bool enabled);

enum class ThingSpeakSendStatus {
	NotAttempted,
	Success,
	Error
};

ThingSpeakSendStatus thingSpeakSendMeasurement(const MeasurementRecord& record,
											   const MeasurementRecord* pending,
											   size_t pendingCount);
