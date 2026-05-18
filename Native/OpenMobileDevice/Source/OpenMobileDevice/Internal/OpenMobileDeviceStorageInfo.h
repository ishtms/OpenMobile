#pragma once

#include "OpenMobileDeviceResourceTypes.h"

class OPENMOBILEDEVICE_API FOpenMobileDeviceStorageInfo final
{
public:
	static FOpenMobileStorageSnapshot Build(
		EOpenMobileStorageScope Scope,
		uint64 TotalBytes,
		uint64 AvailableBytes,
		bool bImportantUsageAvailable,
		uint64 ImportantUsageAvailableBytes,
		bool bQuerySucceeded
	);
};
