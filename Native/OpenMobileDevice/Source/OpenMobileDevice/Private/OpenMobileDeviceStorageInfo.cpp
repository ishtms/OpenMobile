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

void FOpenMobileDeviceStorageInfo::ApplyLowStorageState(
	FOpenMobileStorageSnapshot& Snapshot,
	int64 ThresholdBytes,
	int64 RecoveryHysteresisBytes,
	const TOptional<bool>& PreviousLowStorageState
)
{
	const int64 ValidThresholdBytes = FMath::Max<int64>(ThresholdBytes, 0);
	const int64 ValidHysteresisBytes =
		FMath::Max<int64>(RecoveryHysteresisBytes, 0);
	const int64 RecoveryThresholdBytes =
		ValidHysteresisBytes > MAX_int64 - ValidThresholdBytes
			? MAX_int64
			: ValidThresholdBytes + ValidHysteresisBytes;

	Snapshot.LowStorageThresholdBytes =
		FOpenMobileDeviceOptionalInt64::MakeAvailable(ValidThresholdBytes);
	Snapshot.RecoveryThresholdBytes =
		FOpenMobileDeviceOptionalInt64::MakeAvailable(RecoveryThresholdBytes);
	Snapshot.bIsLowStorage = {};
	if (!Snapshot.AvailableBytes.bIsAvailable)
	{
		return;
	}

	const bool bWasLow = PreviousLowStorageState.IsSet()
		&& PreviousLowStorageState.GetValue();
	const bool bIsLow = bWasLow
		? Snapshot.AvailableBytes.Value < RecoveryThresholdBytes
		: Snapshot.AvailableBytes.Value <= ValidThresholdBytes;
	Snapshot.bIsLowStorage =
		FOpenMobileDeviceOptionalBool::MakeAvailable(bIsLow);
}
