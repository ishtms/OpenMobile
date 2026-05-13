#include "OpenMobileDeviceMemoryInfo.h"

namespace OpenMobileDeviceMemoryInfoPrivate
{
	FOpenMobileDeviceOptionalInt64 MakeByteCount(uint64 Bytes)
	{
		return Bytes > 0 && Bytes <= static_cast<uint64>(MAX_int64)
			? FOpenMobileDeviceOptionalInt64::MakeAvailable(
				static_cast<int64>(Bytes)
			)
			: FOpenMobileDeviceOptionalInt64();
	}
}

FOpenMobileMemorySnapshot FOpenMobileDeviceMemoryInfo::Build(
	uint64 TotalPhysicalBytes,
	uint64 AvailablePhysicalBytes,
	bool bAvailableBytesAreApproximate,
	EOpenMobileMemoryPressureState PressureState
)
{
	using namespace OpenMobileDeviceMemoryInfoPrivate;
	FOpenMobileMemorySnapshot Snapshot;
	Snapshot.TotalPhysicalBytes = MakeByteCount(TotalPhysicalBytes);
	const bool bAvailableOrderIsValid = TotalPhysicalBytes == 0
		|| AvailablePhysicalBytes <= TotalPhysicalBytes;
	if (bAvailableOrderIsValid)
	{
		Snapshot.AvailablePhysicalBytes = MakeByteCount(
			AvailablePhysicalBytes
		);
	}
	Snapshot.bAvailableBytesAreApproximate =
		Snapshot.AvailablePhysicalBytes.bIsAvailable
		&& bAvailableBytesAreApproximate;
	Snapshot.PressureState = PressureState;
	return Snapshot;
}
