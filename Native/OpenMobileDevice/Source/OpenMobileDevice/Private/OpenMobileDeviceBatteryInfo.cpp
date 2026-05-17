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

void FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingState(
	FOpenMobilePowerSnapshot& Snapshot,
	int64 NativeState,
	bool bAvailable
)
{
	Snapshot.ChargingState = EOpenMobileBatteryChargingState::Unknown;
	Snapshot.NativeChargingState = {};
	if (!bAvailable)
	{
		return;
	}
	Snapshot.NativeChargingState =
		FOpenMobileDeviceOptionalString::MakeAvailable(
			FString::Printf(TEXT("Android:%lld"), NativeState)
		);
	switch (NativeState)
	{
	case 2:
		Snapshot.ChargingState = EOpenMobileBatteryChargingState::Charging;
		break;
	case 3:
		Snapshot.ChargingState = EOpenMobileBatteryChargingState::Discharging;
		break;
	case 5:
		Snapshot.ChargingState = EOpenMobileBatteryChargingState::Full;
		break;
	default:
		break;
	}
}

void FOpenMobileDeviceBatteryInfo::ApplyIOSChargingState(
	FOpenMobilePowerSnapshot& Snapshot,
	int64 NativeState,
	bool bAvailable
)
{
	Snapshot.ChargingState = EOpenMobileBatteryChargingState::Unknown;
	Snapshot.NativeChargingState = {};
	if (!bAvailable)
	{
		return;
	}
	Snapshot.NativeChargingState =
		FOpenMobileDeviceOptionalString::MakeAvailable(
			FString::Printf(TEXT("IOS:%lld"), NativeState)
		);
	switch (NativeState)
	{
	case 1:
		Snapshot.ChargingState = EOpenMobileBatteryChargingState::Discharging;
		break;
	case 2:
		Snapshot.ChargingState = EOpenMobileBatteryChargingState::Charging;
		break;
	case 3:
		Snapshot.ChargingState = EOpenMobileBatteryChargingState::Full;
		break;
	default:
		break;
	}
}

void FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingSource(
	FOpenMobilePowerSnapshot& Snapshot,
	int64 NativeSource,
	bool bAvailable
)
{
	Snapshot.ChargingSource = EOpenMobileChargingSource::Unknown;
	if (!bAvailable || NativeSource <= 0)
	{
		return;
	}
	switch (NativeSource)
	{
	case 1:
		Snapshot.ChargingSource = EOpenMobileChargingSource::AC;
		break;
	case 2:
		Snapshot.ChargingSource = EOpenMobileChargingSource::USB;
		break;
	case 4:
		Snapshot.ChargingSource = EOpenMobileChargingSource::Wireless;
		break;
	default:
		if ((NativeSource & (NativeSource - 1)) == 0)
		{
			Snapshot.ChargingSource = EOpenMobileChargingSource::Other;
		}
		break;
	}
}

void FOpenMobileDeviceBatteryInfo::ApplyIOSChargingSource(
	FOpenMobilePowerSnapshot& Snapshot
)
{
	Snapshot.ChargingSource = EOpenMobileChargingSource::Unsupported;
}

void FOpenMobileDeviceBatteryInfo::ApplyAndroidPowerSavingState(
	FOpenMobilePowerSnapshot& Snapshot,
	bool bEnabled,
	bool bAvailable
)
{
	Snapshot.bPowerSavingEnabled = {};
	Snapshot.NativePowerSavingState = {};
	if (!bAvailable)
	{
		return;
	}
	Snapshot.bPowerSavingEnabled =
		FOpenMobileDeviceOptionalBool::MakeAvailable(bEnabled);
	Snapshot.NativePowerSavingState =
		FOpenMobileDeviceOptionalString::MakeAvailable(
			bEnabled ? TEXT("Android:true") : TEXT("Android:false")
		);
}

void FOpenMobileDeviceBatteryInfo::ApplyIOSPowerSavingState(
	FOpenMobilePowerSnapshot& Snapshot,
	bool bEnabled,
	bool bAvailable
)
{
	Snapshot.bPowerSavingEnabled = {};
	Snapshot.NativePowerSavingState = {};
	if (!bAvailable)
	{
		return;
	}
	Snapshot.bPowerSavingEnabled =
		FOpenMobileDeviceOptionalBool::MakeAvailable(bEnabled);
	Snapshot.NativePowerSavingState =
		FOpenMobileDeviceOptionalString::MakeAvailable(
			bEnabled ? TEXT("IOS:true") : TEXT("IOS:false")
		);
}
