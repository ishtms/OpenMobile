#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "OpenMobileSensors.h"
#include "OpenMobileSensorsDemoWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class OPENMOBILESENSORSSAMPLEHOST_API UOpenMobileSensorsDemoWidget final
	: public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(
		const FGeometry& MyGeometry,
		float InDeltaTime
	) override;

private:
	void BuildWidgetTree();
	void RefreshAll();
	void RefreshCapabilities();
	void RefreshLiveSamples();
	void RefreshDiagnostics();
	void StartSensor(
		EOpenMobileSensorType SensorType,
		EOpenMobileSensorDeliveryMode DeliveryMode =
			EOpenMobileSensorDeliveryMode::LatestValue,
		EOpenMobileSensorCoordinateSpace CoordinateSpace =
			EOpenMobileSensorCoordinateSpace::DeviceFixed
	);
	const FOpenMobileSensorSubscriptionHandle* FindHandle(
		EOpenMobileSensorType SensorType
	) const;
	int64& SequenceFor(EOpenMobileSensorType SensorType);
	void SetStatus(const FString& Message, const FLinearColor& Color);

	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleStopClicked();

	UFUNCTION()
	void HandleDrainClicked();

	UFUNCTION()
	void HandleRecenterClicked();

	UFUNCTION()
	void HandleScreenRotationClicked();

	UFUNCTION()
	void HandleResetStepsClicked();

	UFUNCTION()
	void HandlePermissionClicked();

	UFUNCTION()
	void HandleRecordingClicked();

	UFUNCTION()
	void HandleReplayClicked();

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileSensorsSubsystem> SensorsSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CapabilityText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LiveText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DiagnosticsText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> StartButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> StopButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DrainButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RecenterButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ScreenRotationButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ResetStepsButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PermissionButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RecordingButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ReplayButton;

	TMap<EOpenMobileSensorType, FOpenMobileSensorSubscriptionHandle> Handles;
	TMap<EOpenMobileSensorType, int64> LastSequences;
	FOpenMobileSensorSubscriptionHandle StepSessionHandle;
	FOpenMobileSensorSubscriptionHandle RelativeAltitudeHandle;
	FOpenMobilePermissionRequestHandle PermissionRequest;
	FGuid RecordingIdentifier;
	FGuid ReplayIdentifier;
	FString LastRecordingPath;
	EOpenMobileMotionActivity PreviousActivity =
		EOpenMobileMotionActivity::Unknown;
	FString LastActivityTransition;
	EOpenMobileSensorScreenRotation ScreenRotation =
		EOpenMobileSensorScreenRotation::Rotation0;
	double NextRefreshSeconds = 0.0;
};
