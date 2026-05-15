#pragma once

#include "OpenMobileDeviceLocaleTypes.h"

class OPENMOBILEDEVICE_API FOpenMobileDeviceTimeZoneInfo final
{
public:
	static void Apply(
		FOpenMobileLocaleSnapshot& Snapshot,
		const FString& TimeZoneIdentifier,
		int64 UtcOffsetSeconds,
		bool bUtcOffsetAvailable,
		bool bIsDaylightSavingTime,
		bool bDaylightSavingTimeAvailable
	);
};
