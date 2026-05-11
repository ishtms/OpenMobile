#pragma once

#include "OpenMobileDeviceIdentityTypes.h"

class OPENMOBILEDEVICE_API FOpenMobileDeviceProcessorInfo final
{
public:
	static void ApplyLogicalProcessorCount(
		FOpenMobileDeviceInformationSnapshot& Snapshot,
		int32 LogicalProcessorCount
	);
};
