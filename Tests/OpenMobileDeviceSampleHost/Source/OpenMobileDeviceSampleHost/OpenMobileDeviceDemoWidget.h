#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceEndpointReachabilityTypes.h"
#include "OpenMobileDeviceSubsystem.h"
#include "OpenMobileDeviceDemoWidget.generated.h"

class UButton;
class UOpenMobileBrightnessHandle;
class UOpenMobileDeviceEndpointReachabilityAsyncAction;
class UOpenMobileDeviceMonitoringSubscription;
class UOpenMobileDeviceUserInitiatedPasteAsyncAction;
class UOpenMobileKeepScreenAwakeHandle;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class OPENMOBILEDEVICESAMPLEHOST_API UOpenMobileDeviceDemoWidget final : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	void BindSubsystemEvents();
	void UnbindSubsystemEvents();
	void RefreshSnapshots();
	void UpdateCapabilityReport(const FOpenMobileDeviceCapabilityReport& Report);
	void SetActionStatus(const FString& Message, const FLinearColor& Color);

	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleReachabilityClicked();

	UFUNCTION()
	void HandlePasteClicked();

	UFUNCTION()
	void HandleBrightnessClicked();

	UFUNCTION()
	void HandleAwakeClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandlePowerChanged(const FOpenMobilePowerSnapshot& Snapshot);

	UFUNCTION()
	void HandleMemoryChanged(const FOpenMobileMemorySnapshot& Snapshot);

	UFUNCTION()
	void HandleStorageChanged(const FOpenMobileStorageSnapshot& Snapshot);

	UFUNCTION()
	void HandleNetworkChanged(const FOpenMobileNetworkPathSnapshot& Snapshot);

	UFUNCTION()
	void HandleWindowChanged(const FOpenMobileWindowDisplaySnapshot& Snapshot);

	UFUNCTION()
	void HandleAppearanceChanged(const FOpenMobileAppearanceSnapshot& Snapshot);

	UFUNCTION()
	void HandleAccessibilityChanged(const FOpenMobileAccessibilitySnapshot& Snapshot);

	UFUNCTION()
	void HandleSettingsReturned(const FOpenMobileDeviceCapabilityReport& Report);

	UFUNCTION()
	void HandleReachabilityCompleted(
		const FOpenMobileEndpointReachabilityResult& Result
	);

	UFUNCTION()
	void HandleReachabilityCancelled(const FOpenMobileError& Error);

	UFUNCTION()
	void HandleReachabilityFailed(const FOpenMobileError& Error);

	UFUNCTION()
	void HandlePasteSucceeded();

	UFUNCTION()
	void HandlePasteCancelled(const FOpenMobileError& Error);

	UFUNCTION()
	void HandlePasteFailed(const FOpenMobileError& Error);

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileDeviceSubsystem> DeviceSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileDeviceMonitoringSubscription> MonitoringSubscription;

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileDeviceEndpointReachabilityAsyncAction> ReachabilityAction;

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileDeviceUserInitiatedPasteAsyncAction> PasteAction;

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileBrightnessHandle> BrightnessHandle;

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileKeepScreenAwakeHandle> AwakeHandle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SnapshotText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CapabilityText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PolicyText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActionStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ReachabilityButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PasteButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BrightnessButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> AwakeButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SettingsButton;

	FOpenMobileNetworkPathSnapshot PreviousNetworkSnapshot;
	FOpenMobileWindowDisplaySnapshot PreviousWindowSnapshot;
	bool bHasPreviousNetworkSnapshot = false;
	bool bHasPreviousWindowSnapshot = false;
};
