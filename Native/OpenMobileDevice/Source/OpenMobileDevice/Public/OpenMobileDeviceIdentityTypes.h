#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceIdentityTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileDevicePlatform : uint8
{
	Unknown,
	Android,
	IOS
};

UENUM(BlueprintType)
enum class EOpenMobileDeviceFormFactor : uint8
{
	Unknown,
	Phone,
	Tablet,
	Foldable
};

UENUM(BlueprintType)
enum class EOpenMobileDeviceEmulatorConfidence : uint8
{
	Unknown,
	NoEvidence,
	Possible,
	Likely,
	Confirmed
};

UENUM(BlueprintType)
enum class EOpenMobileBuildConfiguration : uint8
{
	Unknown,
	Debug,
	Development,
	Test,
	Shipping
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileDeviceInformationSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileDevicePlatform Platform = EOpenMobileDevicePlatform::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString ReadableOsVersion;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString RawOsVersion;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalInt32 OsVersionMajor;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalInt32 OsVersionMinor;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalInt32 OsVersionPatch;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalInt32 AndroidApiLevel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString Manufacturer;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString Brand;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString Model;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString HardwareModelIdentifier;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileDeviceFormFactor FormFactor = EOpenMobileDeviceFormFactor::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString ProcessArchitecture;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bSupportedAbisAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	TArray<FString> SupportedAbis;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalInt32 LogicalProcessorCount;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bProbablyEmulator;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileDeviceEmulatorConfidence EmulatorConfidence =
		EOpenMobileDeviceEmulatorConfidence::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString EmulatorReason;

	bool operator==(const FOpenMobileDeviceInformationSnapshot& Other) const
	{
		return Metadata == Other.Metadata
			&& Platform == Other.Platform
			&& ReadableOsVersion == Other.ReadableOsVersion
			&& RawOsVersion == Other.RawOsVersion
			&& OsVersionMajor == Other.OsVersionMajor
			&& OsVersionMinor == Other.OsVersionMinor
			&& OsVersionPatch == Other.OsVersionPatch
			&& AndroidApiLevel == Other.AndroidApiLevel
			&& Manufacturer == Other.Manufacturer
			&& Brand == Other.Brand
			&& Model == Other.Model
			&& HardwareModelIdentifier == Other.HardwareModelIdentifier
			&& FormFactor == Other.FormFactor
			&& ProcessArchitecture == Other.ProcessArchitecture
			&& bSupportedAbisAvailable == Other.bSupportedAbisAvailable
			&& SupportedAbis == Other.SupportedAbis
			&& LogicalProcessorCount == Other.LogicalProcessorCount
			&& bProbablyEmulator == Other.bProbablyEmulator
			&& EmulatorConfidence == Other.EmulatorConfidence
			&& EmulatorReason == Other.EmulatorReason;
	}

	bool operator!=(const FOpenMobileDeviceInformationSnapshot& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileApplicationMetadataSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString PackageIdentifier;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString VersionName;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString BuildNumber;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileBuildConfiguration BuildConfiguration =
		EOpenMobileBuildConfiguration::Unknown;

	bool operator==(const FOpenMobileApplicationMetadataSnapshot& Other) const
	{
		return Metadata == Other.Metadata
			&& DisplayName == Other.DisplayName
			&& PackageIdentifier == Other.PackageIdentifier
			&& VersionName == Other.VersionName
			&& BuildNumber == Other.BuildNumber
			&& BuildConfiguration == Other.BuildConfiguration;
	}
};
