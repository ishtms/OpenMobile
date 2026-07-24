#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticsSampleWidget.generated.h"

class UButton;
class UOpenMobileHapticsSubsystem;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class OPENMOBILEHAPTICSSAMPLEHOST_API UOpenMobileHapticsSampleWidget final
	: public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	void RefreshSummary();
	void ShowPlaybackResult(
		const FString& Action,
		const FOpenMobileHapticPlaybackResult& Result
	);
	void ShowControlResult(
		const FString& Action,
		const FOpenMobileHapticControlResult& Result
	);
	void SetStatus(const FString& Message, const FLinearColor& Color);
	void StopOwnedPlayback();
	void StopVehicleTimer();

	UFUNCTION()
	void HandleRefreshClicked();

	UFUNCTION()
	void HandleSnapshotClicked();

	UFUNCTION()
	void HandlePrepareClicked();

	UFUNCTION()
	void HandleReleaseClicked();

	UFUNCTION()
	void HandleSelectionClicked();

	UFUNCTION()
	void HandleUIConfirmClicked();

	UFUNCTION()
	void HandleCombatClicked();

	UFUNCTION()
	void HandleDamageClicked();

	UFUNCTION()
	void HandlePickupClicked();

	UFUNCTION()
	void HandleRewardClicked();

	UFUNCTION()
	void HandleVehicleClicked();

	UFUNCTION()
	void HandleChargingClicked();

	UFUNCTION()
	void HandleAudioSyncClicked();

	UFUNCTION()
	void HandleAccessibilityClicked();

	UFUNCTION()
	void HandleScheduleClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleStopGameplayClicked();

	UFUNCTION()
	void HandleToggleEnabledClicked();

	UFUNCTION()
	void HandleSofterClicked();

	UFUNCTION()
	void HandleFullIntensityClicked();

	UFUNCTION()
	void HandleVehicleUpdate();

	UFUNCTION()
	void HandlePlaybackEvent(const FOpenMobileHapticPlaybackEvent& Event);

	UFUNCTION()
	void HandleLibrariesPrepared(
		const FOpenMobileHapticLibraryPreloadResult& Result
	);

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileHapticsSubsystem> Haptics;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EventText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> Buttons;

	FOpenMobileHapticPlaybackHandle ActiveHandle;
	FOpenMobileHapticPlaybackHandle VehicleHandle;
	FOpenMobileHapticLibraryPreloadHandle PreloadHandle;
	FTimerHandle VehicleUpdateTimer;
	double VehicleStartedAtSeconds = 0.0;
	TArray<FString> RecentEvents;
};
