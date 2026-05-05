#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceNetworkTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileNetworkPathState : uint8
{
	Unknown,
	Unavailable,
	Available,
	LocalOnly,
	CaptivePortal,
	InternetCapable
};

UENUM(BlueprintType)
enum class EOpenMobileNetworkValidationSource : uint8
{
	Unknown,
	PlatformPath,
	DeclaredCapability,
	OsValidatedPath
};

UENUM(BlueprintType)
enum class EOpenMobileNetworkTransport : uint8
{
	Unknown,
	Wifi,
	Cellular,
	Ethernet,
	VPN,
	Bluetooth,
	Other
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileNetworkPathSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileNetworkPathState PathState = EOpenMobileNetworkPathState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileNetworkValidationSource ValidationSource =
		EOpenMobileNetworkValidationSource::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bTransportsAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	TArray<EOpenMobileNetworkTransport> Transports;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bDefaultTransportAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileNetworkTransport DefaultTransport = EOpenMobileNetworkTransport::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bIsMetered;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bIsExpensive;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bIsConstrained;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bIsCaptivePortal;

	bool operator==(const FOpenMobileNetworkPathSnapshot& Other) const
	{
		return Metadata == Other.Metadata
			&& PathState == Other.PathState
			&& ValidationSource == Other.ValidationSource
			&& bTransportsAvailable == Other.bTransportsAvailable
			&& Transports == Other.Transports
			&& bDefaultTransportAvailable == Other.bDefaultTransportAvailable
			&& DefaultTransport == Other.DefaultTransport
			&& bIsMetered == Other.bIsMetered
			&& bIsExpensive == Other.bIsExpensive
			&& bIsConstrained == Other.bIsConstrained
			&& bIsCaptivePortal == Other.bIsCaptivePortal;
	}
};
