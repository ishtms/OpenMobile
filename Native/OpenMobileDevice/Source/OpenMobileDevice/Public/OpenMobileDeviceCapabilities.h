#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceCapabilities.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileDeviceCapabilityLimit : uint8
{
	None,
	UnsupportedPlatform,
	MissingHardware,
	MinimumOsVersion,
	Simulator,
	PolicyRestriction,
	NativeResourceConflict
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileDeviceCapability
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FName Name;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileCapabilityState State = EOpenMobileCapabilityState::Unavailable;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileDeviceCapabilityLimit Limit = EOpenMobileDeviceCapabilityLimit::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString MinimumOsVersion;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FString Detail;

	bool IsAvailable() const
	{
		return State == EOpenMobileCapabilityState::Available;
	}

	bool operator==(const FOpenMobileDeviceCapability& Other) const
	{
		return Name == Other.Name
			&& State == Other.State
			&& Limit == Other.Limit
			&& MinimumOsVersion == Other.MinimumOsVersion
			&& BackendName == Other.BackendName
			&& Detail == Other.Detail;
	}

	static EOpenMobileCapabilityState NormalizeState(EOpenMobileCapabilityState InState);
	static FOpenMobileDeviceCapability Resolve(
		FName CapabilityName,
		TConstArrayView<FOpenMobileDeviceCapability> Candidates
	);
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileDeviceCapabilityReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	TArray<FOpenMobileDeviceCapability> Capabilities;

	const FOpenMobileDeviceCapability* Find(FName CapabilityName) const;

	bool operator==(const FOpenMobileDeviceCapabilityReport& Other) const
	{
		return Metadata == Other.Metadata && Capabilities == Other.Capabilities;
	}
};

struct OPENMOBILEDEVICE_API FOpenMobileDeviceCapabilityNames
{
	static const FName CapabilityReport;
	static const FName PlatformInformation;
	static const FName ManufacturerBrandModel;
	static const FName HardwareModelIdentifier;
	static const FName FormFactor;
	static const FName CpuArchitecture;
	static const FName LogicalProcessorCount;
	static const FName PhysicalMemory;
	static const FName ApplicationMetadata;
	static const FName EmulatorDetection;
	static const FName PreferredLanguages;
	static const FName Locale;
	static const FName TimeZone;
	static const FName RegionalFormatting;
	static const FName LocaleChangeEvents;
	static const FName BatteryLevel;
	static const FName ChargingState;
	static const FName ChargingSource;
	static const FName PowerSavingMode;
	static const FName ThermalState;
	static const FName ThermalHeadroom;
	static const FName BatteryEvents;
	static const FName PowerSavingEvents;
	static const FName ThermalEvents;
	static const FName MemoryPressureEvents;
	static const FName StorageSpace;
	static const FName LowStorageEvents;
	static const FName NetworkPath;
	static const FName NetworkTransport;
	static const FName NetworkPolicy;
	static const FName CaptivePortal;
	static const FName NetworkChangeEvents;
	static const FName EndpointReachability;
	static const FName WindowMetrics;
	static const FName RefreshRateInformation;
	static const FName RefreshRateControl;
	static const FName SafeAreaInsets;
	static const FName DisplayCutout;
	static const FName WindowOrientation;
	static const FName WindowChangeEvents;
	static const FName OrientationControl;
	static const FName MultiWindowEvents;
	static const FName FoldablePosture;
	static const FName HdrWideColor;
	static const FName SystemAppearance;
	static const FName AppearanceChangeEvents;
	static const FName PreferredTextScale;
	static const FName ReducedAnimation;
	static const FName ScreenReader;
	static const FName AccessibilityChangeEvents;
	static const FName Brightness;
	static const FName KeepScreenAwake;
	static const FName ImmersiveMode;
	static const FName FlashlightAvailability;
	static const FName FlashlightControl;
	static const FName ClipboardWrite;
	static const FName ClipboardRead;
	static const FName ClipboardTypeCheck;
	static const FName ClipboardClear;
	static const FName UrlHandlerCheck;
	static const FName AndroidPackageCheck;
	static const FName OpenApplicationSettings;
	static const FName MediaVolume;
	static const FName VolumeEvents;

	static const TArray<FName>& GetAll();
	static bool IsKnown(FName CapabilityName);
};
