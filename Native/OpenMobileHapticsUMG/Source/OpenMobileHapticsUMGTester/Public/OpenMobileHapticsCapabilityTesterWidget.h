#pragma once

#include "Blueprint/UserWidget.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticsCapabilityTesterWidget.generated.h"

class UButton;
class UOpenMobileHapticPatternAsset;
class UOpenMobileHapticsSubsystem;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;

UCLASS(Blueprintable)
class OPENMOBILEHAPTICSUMGTESTER_API
UOpenMobileHapticsCapabilityTesterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics Tester")
	EOpenMobileHapticSemanticEffect SemanticEffect =
		EOpenMobileHapticSemanticEffect::Selection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics Tester")
	EOpenMobileHapticImpactStyle ImpactStyle =
		EOpenMobileHapticImpactStyle::Medium;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics Tester")
	EOpenMobileHapticNotificationType NotificationType =
		EOpenMobileHapticNotificationType::Success;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics Tester")
	EOpenMobileHapticGamePreset GamePreset =
		EOpenMobileHapticGamePreset::Confirm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics Tester")
	TSoftObjectPtr<UOpenMobileHapticPatternAsset> WaveformPattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics Tester")
	TSoftObjectPtr<UOpenMobileHapticPatternAsset> PrimitivePattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics Tester")
	TSoftObjectPtr<UOpenMobileHapticPatternAsset> EnvelopePattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics Tester")
	TSoftObjectPtr<UOpenMobileHapticPatternAsset> AHAPPattern;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics Tester")
	TSoftObjectPtr<UOpenMobileHapticPatternAsset> FallbackPattern;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics Tester")
	void RefreshCapabilitySnapshot();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Haptics Tester")
	FString GetSanitizedSnapshotJson() const;

	virtual TSharedRef<SWidget> RebuildWidget() override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildWidgetTree();
	UButton* AddButton(
		UUniformGridPanel* Grid,
		FName Name,
		const FString& Label,
		int32 Row,
		int32 Column
	);
	bool AdmitPlayback(const FString& Label);
	void HandlePlaybackResult(
		const FString& Label,
		const FOpenMobileHapticPlaybackResult& Result
	);
	void PlayConfiguredPattern(
		const FString& Label,
		const TSoftObjectPtr<UOpenMobileHapticPatternAsset>& Pattern
	);
	void StopTesterPlayback();
	void SetStatus(const FString& Value, const FLinearColor& Color);

	UFUNCTION()
	void HandleRefresh();

	UFUNCTION()
	void HandleCopySnapshot();

	UFUNCTION()
	void HandlePrepare();

	UFUNCTION()
	void HandleRelease();

	UFUNCTION()
	void HandleStop();

	UFUNCTION()
	void HandleSemantic();

	UFUNCTION()
	void HandleImpact();

	UFUNCTION()
	void HandleNotification();

	UFUNCTION()
	void HandlePreset();

	UFUNCTION()
	void HandlePulse();

	UFUNCTION()
	void HandleWaveform();

	UFUNCTION()
	void HandlePrimitive();

	UFUNCTION()
	void HandleEnvelope();

	UFUNCTION()
	void HandleAHAP();

	UFUNCTION()
	void HandleFallback();

	UFUNCTION()
	void HandlePrepared(
		const FOpenMobileHapticLibraryPreloadResult& Result
	);

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileHapticsSubsystem> Haptics;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SnapshotText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> Buttons;

	TArray<double> RecentPlaybackTimes;
	FString SnapshotJson;
	FTimerHandle SafetyStopTimer;
	double LastPlaybackSeconds = -1.0;
};
