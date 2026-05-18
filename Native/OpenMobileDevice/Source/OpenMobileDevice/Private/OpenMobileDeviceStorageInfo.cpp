#include "OpenMobileDeviceStorageInfo.h"

FOpenMobileStorageSnapshot FOpenMobileDeviceStorageInfo::Build(
	EOpenMobileStorageScope Scope,
	uint64 TotalBytes,
	uint64 AvailableBytes,
	bool bImportantUsageAvailable,
	uint64 ImportantUsageAvailableBytes,
	bool bQuerySucceeded
)
{
	FOpenMobileStorageSnapshot Snapshot;
	Snapshot.Scope = Scope;
	const bool bTotalValid = bQuerySucceeded
		&& TotalBytes > 0
		&& TotalBytes <= static_cast<uint64>(MAX_int64);
	if (!bTotalValid)
	{
		return Snapshot;
	}

	Snapshot.TotalBytes = FOpenMobileDeviceOptionalInt64::MakeAvailable(
		static_cast<int64>(TotalBytes)
	);
	if (AvailableBytes <= TotalBytes
		&& AvailableBytes <= static_cast<uint64>(MAX_int64))
	{
		Snapshot.AvailableBytes =
			FOpenMobileDeviceOptionalInt64::MakeAvailable(
				static_cast<int64>(AvailableBytes)
			);
	}
	if (bImportantUsageAvailable
		&& ImportantUsageAvailableBytes <= TotalBytes
		&& ImportantUsageAvailableBytes <= static_cast<uint64>(MAX_int64))
	{
		Snapshot.ImportantUsageAvailableBytes =
			FOpenMobileDeviceOptionalInt64::MakeAvailable(
				static_cast<int64>(ImportantUsageAvailableBytes)
			);
	}
	return Snapshot;
}
