#include "OpenMobileDeviceBatteryInfo.h"

void FOpenMobileDeviceBatteryInfo::ApplyFraction(
	FOpenMobilePowerSnapshot& Snapshot,
	double NativeLevel,
	bool bAvailable
)
{
	Snapshot.BatteryPercent = {};
	Snapshot.NativeBatteryLevel = {};
	if (!bAvailable
		|| !FMath::IsFinite(NativeLevel)
		|| NativeLevel < 0.0
		|| NativeLevel > 1.0)
	{
		return;
	}

	Snapshot.NativeBatteryLevel =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(
			static_cast<float>(NativeLevel)
		);
	Snapshot.BatteryPercent = FOpenMobileDeviceOptionalFloat::MakeAvailable(
		static_cast<float>(NativeLevel * 100.0)
	);
}

void FOpenMobileDeviceBatteryInfo::ApplyRatio(
	FOpenMobilePowerSnapshot& Snapshot,
	int64 NativeLevel,
	int64 NativeScale,
	bool bAvailable
)
{
	ApplyFraction(
		Snapshot,
		NativeScale > 0
			? static_cast<double>(NativeLevel)
				/ static_cast<double>(NativeScale)
			: 0.0,
		bAvailable
			&& NativeLevel >= 0
			&& NativeScale > 0
			&& NativeLevel <= NativeScale
	);
}
