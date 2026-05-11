#include "OpenMobileDeviceProcessorInfo.h"

void FOpenMobileDeviceProcessorInfo::ApplyLogicalProcessorCount(
	FOpenMobileDeviceInformationSnapshot& Snapshot,
	int32 LogicalProcessorCount
)
{
	Snapshot.LogicalProcessorCount = LogicalProcessorCount > 0
		? FOpenMobileDeviceOptionalInt32::MakeAvailable(LogicalProcessorCount)
		: FOpenMobileDeviceOptionalInt32();
}
