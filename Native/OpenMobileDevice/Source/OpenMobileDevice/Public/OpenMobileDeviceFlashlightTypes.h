#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceFlashlightTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileFlashlightHardwareState : uint8
{
	Unknown,
	Unavailable,
	Available
};

UENUM(BlueprintType)
enum class EOpenMobileFlashlightTorchState : uint8
{
	Unknown,
	Off,
	On
};

UENUM(BlueprintType)
enum class EOpenMobileFlashlightPermissionState : uint8
{
	Unknown,
	NotRequired,
	NotDetermined,
	Granted,
	Denied,
	Restricted
};

UENUM(BlueprintType)
enum class EOpenMobileFlashlightConflictState : uint8
{
	Unknown,
	None,
	CameraResourceBusy
};

UENUM(BlueprintType)
enum class EOpenMobileFlashlightThermalState : uint8
{
	Unknown,
	NotRestricted,
	Restricted
};

UENUM(BlueprintType)
enum class EOpenMobileFlashlightOwnership : uint8
{
	Unknown,
	ThisApplication,
	External
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileFlashlightSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileFlashlightHardwareState HardwareState =
		EOpenMobileFlashlightHardwareState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileFlashlightTorchState TorchState =
		EOpenMobileFlashlightTorchState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalFloat CurrentIntensity;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bVariableIntensitySupported;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileFlashlightPermissionState PermissionState =
		EOpenMobileFlashlightPermissionState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileFlashlightConflictState ConflictState =
		EOpenMobileFlashlightConflictState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileFlashlightThermalState ThermalState =
		EOpenMobileFlashlightThermalState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileFlashlightOwnership Ownership =
		EOpenMobileFlashlightOwnership::Unknown;

	bool operator==(const FOpenMobileFlashlightSnapshot& Other) const
	{
		return Metadata == Other.Metadata
			&& HardwareState == Other.HardwareState
			&& TorchState == Other.TorchState
			&& CurrentIntensity == Other.CurrentIntensity
			&& bVariableIntensitySupported
				== Other.bVariableIntensitySupported
			&& PermissionState == Other.PermissionState
			&& ConflictState == Other.ConflictState
			&& ThermalState == Other.ThermalState
			&& Ownership == Other.Ownership;
	}
};
