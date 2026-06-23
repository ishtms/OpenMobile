#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceAccessibilityTypes.h"
#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceNetworkTypes.h"
#include "OpenMobileDeviceResourceTypes.h"
#include "OpenMobileDeviceMockSettings.generated.h"

UCLASS(
	Config = EditorPerProjectUserSettings,
	DefaultConfig,
	meta = (DisplayName = "OpenMobile Device Mock")
)
class OPENMOBILEDEVICEEDITOR_API UOpenMobileDeviceMockSettings final
	: public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("OpenMobile"); }
	virtual FName GetSectionName() const override
	{
		return TEXT("OpenMobile Device Mock");
	}

	UPROPERTY(Config, EditAnywhere, Category = "Mock")
	bool bEnableMockBackend = false;

	UPROPERTY(Config, EditAnywhere, Category = "Mock")
	bool bResetOverridesAfterPlayInEditor = true;

	UPROPERTY(Config, EditAnywhere, Category = "Power", meta = (ClampMin = "0", ClampMax = "100"))
	float BatteryPercent = 100.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Power")
	EOpenMobileBatteryChargingState ChargingState =
		EOpenMobileBatteryChargingState::Full;

	UPROPERTY(Config, EditAnywhere, Category = "Power")
	EOpenMobileChargingSource ChargingSource = EOpenMobileChargingSource::AC;

	UPROPERTY(Config, EditAnywhere, Category = "Power")
	EOpenMobileThermalState ThermalState = EOpenMobileThermalState::Nominal;

	UPROPERTY(Config, EditAnywhere, Category = "Memory", meta = (ClampMin = "0"))
	int64 TotalPhysicalBytes = 8ll * 1024 * 1024 * 1024;

	UPROPERTY(Config, EditAnywhere, Category = "Memory", meta = (ClampMin = "0"))
	int64 AvailablePhysicalBytes = 4ll * 1024 * 1024 * 1024;

	UPROPERTY(Config, EditAnywhere, Category = "Memory")
	EOpenMobileMemoryPressureState MemoryPressureState =
		EOpenMobileMemoryPressureState::Nominal;

	UPROPERTY(Config, EditAnywhere, Category = "Storage", meta = (ClampMin = "0"))
	int64 TotalStorageBytes = 64ll * 1024 * 1024 * 1024;

	UPROPERTY(Config, EditAnywhere, Category = "Storage", meta = (ClampMin = "0"))
	int64 AvailableStorageBytes = 32ll * 1024 * 1024 * 1024;

	UPROPERTY(Config, EditAnywhere, Category = "Storage")
	bool bLowStorage = false;

	UPROPERTY(Config, EditAnywhere, Category = "Network")
	EOpenMobileNetworkPathState NetworkPathState =
		EOpenMobileNetworkPathState::InternetCapable;

	UPROPERTY(Config, EditAnywhere, Category = "Network")
	EOpenMobileNetworkTransport DefaultNetworkTransport =
		EOpenMobileNetworkTransport::Wifi;

	UPROPERTY(Config, EditAnywhere, Category = "Network")
	bool bNetworkMetered = false;

	UPROPERTY(Config, EditAnywhere, Category = "Display")
	TArray<FVector4> DisplayCutouts;

	UPROPERTY(Config, EditAnywhere, Category = "Display")
	EOpenMobileFoldablePosture FoldablePosture = EOpenMobileFoldablePosture::Flat;

	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	EOpenMobileSystemAppearance Appearance = EOpenMobileSystemAppearance::Light;

	UPROPERTY(Config, EditAnywhere, Category = "Accessibility", meta = (ClampMin = "0.1"))
	float PreferredTextScale = 1.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Accessibility")
	bool bReducedAnimationPreferred = false;

	UPROPERTY(Config, EditAnywhere, Category = "Accessibility")
	bool bScreenReaderActive = false;

	UPROPERTY(Config, EditAnywhere, Category = "Accessibility")
	bool bTouchExplorationActive = false;

	UPROPERTY(Config, EditAnywhere, Category = "Failures")
	bool bInjectFailure = false;

	UPROPERTY(Config, EditAnywhere, Category = "Failures")
	FName FailedCapability = TEXT("OpenMobile.Device.Storage.Space");

	UPROPERTY(Config, EditAnywhere, Category = "Failures")
	EOpenMobileErrorCode FailureCode = EOpenMobileErrorCode::NativeFailure;

	UPROPERTY(Config, EditAnywhere, Category = "Failures")
	FString FailureMessage = TEXT("Scripted Device mock failure");

	UPROPERTY(VisibleAnywhere, Transient, Category = "Status")
	bool bMockBackendSelected = false;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Status")
	int32 ActiveScriptStepCount = 0;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Status")
	FString ActiveOverrideSummary;

	UFUNCTION(CallInEditor, Category = "Mock")
	void ResetOverrides();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};
