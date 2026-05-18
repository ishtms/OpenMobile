#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceResourceTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileBatteryChargingState : uint8
{
	Unknown,
	Discharging,
	Charging,
	Full
};

UENUM(BlueprintType)
enum class EOpenMobileChargingSource : uint8
{
	Unknown,
	AC,
	USB,
	Wireless,
	Other,
	Unsupported
};

UENUM(BlueprintType)
enum class EOpenMobileThermalState : uint8
{
	Unknown,
	Nominal,
	Fair,
	Serious,
	Critical
};

UENUM(BlueprintType)
enum class EOpenMobileThermalTrend : uint8
{
	Unknown,
	Cooling,
	Stable,
	Heating
};

UENUM(BlueprintType)
enum class EOpenMobileMemoryPressureState : uint8
{
	Unknown,
	Nominal,
	Warning,
	Critical
};

UENUM(BlueprintType)
enum class EOpenMobileStorageScope : uint8
{
	Unknown,
	ApplicationContainer,
	ApplicationDataVolume
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobilePowerSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalFloat BatteryPercent;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalFloat NativeBatteryLevel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileBatteryChargingState ChargingState =
		EOpenMobileBatteryChargingState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString NativeChargingState;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileChargingSource ChargingSource = EOpenMobileChargingSource::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bPowerSavingEnabled;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString NativePowerSavingState;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileThermalState ThermalState = EOpenMobileThermalState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalInt32 NativeThermalState;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalFloat ThermalHeadroom;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalFloat ThermalForecastSeconds;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FDateTime ThermalHeadroomSampleTimeUtc;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileThermalTrend ThermalTrend = EOpenMobileThermalTrend::Unknown;

	bool operator==(const FOpenMobilePowerSnapshot& Other) const
	{
		return Metadata == Other.Metadata
			&& BatteryPercent == Other.BatteryPercent
			&& NativeBatteryLevel == Other.NativeBatteryLevel
			&& ChargingState == Other.ChargingState
			&& NativeChargingState == Other.NativeChargingState
			&& ChargingSource == Other.ChargingSource
			&& bPowerSavingEnabled == Other.bPowerSavingEnabled
			&& NativePowerSavingState == Other.NativePowerSavingState
			&& ThermalState == Other.ThermalState
			&& NativeThermalState == Other.NativeThermalState
			&& ThermalHeadroom == Other.ThermalHeadroom
			&& ThermalForecastSeconds == Other.ThermalForecastSeconds
			&& ThermalHeadroomSampleTimeUtc == Other.ThermalHeadroomSampleTimeUtc
			&& ThermalTrend == Other.ThermalTrend;
	}

	bool operator!=(const FOpenMobilePowerSnapshot& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileMediaVolumeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalFloat VolumePercent;

	bool operator==(const FOpenMobileMediaVolumeSnapshot& Other) const
	{
		return Metadata == Other.Metadata && VolumePercent == Other.VolumePercent;
	}

	bool operator!=(const FOpenMobileMediaVolumeSnapshot& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileMemorySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalInt64 TotalPhysicalBytes;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalInt64 AvailablePhysicalBytes;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bAvailableBytesAreApproximate = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileMemoryPressureState PressureState = EOpenMobileMemoryPressureState::Unknown;

	bool operator==(const FOpenMobileMemorySnapshot& Other) const
	{
		return Metadata == Other.Metadata
			&& TotalPhysicalBytes == Other.TotalPhysicalBytes
			&& AvailablePhysicalBytes == Other.AvailablePhysicalBytes
			&& bAvailableBytesAreApproximate == Other.bAvailableBytesAreApproximate
			&& PressureState == Other.PressureState;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileStorageSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileStorageScope Scope = EOpenMobileStorageScope::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalInt64 TotalBytes;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalInt64 AvailableBytes;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalInt64 ImportantUsageAvailableBytes;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bIsLowStorage;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalInt64 LowStorageThresholdBytes;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalInt64 RecoveryThresholdBytes;

	bool operator==(const FOpenMobileStorageSnapshot& Other) const
	{
		return Metadata == Other.Metadata
			&& Scope == Other.Scope
			&& TotalBytes == Other.TotalBytes
			&& AvailableBytes == Other.AvailableBytes
			&& ImportantUsageAvailableBytes == Other.ImportantUsageAvailableBytes
			&& bIsLowStorage == Other.bIsLowStorage
			&& LowStorageThresholdBytes == Other.LowStorageThresholdBytes
			&& RecoveryThresholdBytes == Other.RecoveryThresholdBytes;
	}
};
