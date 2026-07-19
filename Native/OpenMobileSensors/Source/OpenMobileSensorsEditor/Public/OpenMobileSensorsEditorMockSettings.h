#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenMobileSensorsDevelopmentInput.h"
#include "OpenMobileSensorsEditorMockSettings.generated.h"

UCLASS(
	Config = EditorPerProjectUserSettings,
	DefaultConfig,
	meta = (DisplayName = "OpenMobile Sensors Mocks")
)
class OPENMOBILESENSORSEDITOR_API UOpenMobileSensorsEditorMockSettings final
	: public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("OpenMobile");
	}

	virtual FName GetSectionName() const override
	{
		return TEXT("OpenMobile Sensors Mocks");
	}

	UPROPERTY(Config, EditAnywhere, Category = "Controls")
	FOpenMobileSensorsMockInput Input;

	UPROPERTY(Config, EditAnywhere, Category = "Presets")
	EOpenMobileSensorsMockPreset Preset =
		EOpenMobileSensorsMockPreset::Custom;

	UPROPERTY(Config, EditAnywhere, Category = "Timeline")
	FOpenMobileSensorsMockTimeline Timeline;

	UPROPERTY(Config, EditAnywhere, Category = "Timeline", meta = (ClampMin = "0.0", Units = "s"))
	double TimelineAdvanceSeconds = 0.1;

	UPROPERTY(Config, EditAnywhere, Category = "Error Injection")
	EOpenMobileSensorType ErrorSensor = EOpenMobileSensorType::Accelerometer;

	UPROPERTY(Config, EditAnywhere, Category = "Error Injection")
	EOpenMobileSensorFailureReason ErrorReason =
		EOpenMobileSensorFailureReason::OperationalFailure;

	UPROPERTY(Config, EditAnywhere, Category = "Error Injection")
	FString NativeErrorCode = TEXT("MockFailure");

	UFUNCTION(CallInEditor, Category = "Controls")
	void ApplyConfiguredInput();

	UFUNCTION(CallInEditor, Category = "Presets")
	void ApplyConfiguredPreset();

	UFUNCTION(CallInEditor, Category = "Timeline")
	void PlayConfiguredTimeline();

	UFUNCTION(CallInEditor, Category = "Timeline")
	void StopConfiguredTimeline();

	UFUNCTION(CallInEditor, Category = "Timeline")
	void AdvanceConfiguredTimeline();

	UFUNCTION(CallInEditor, Category = "Error Injection")
	void InjectConfiguredError();
};
