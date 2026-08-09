#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticNamedPlaybackAsyncAction.generated.h"

class UOpenMobileHapticPlayback;
class UOpenMobileHapticPreparationLease;
class UOpenMobileHapticsSubsystem;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
	FOpenMobileHapticNamedWaitingDynamic
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileHapticNamedAcceptedDynamic,
	UOpenMobileHapticPlayback*,
	Playback,
	bool,
	UsedFallback,
	FName,
	ResolvedQuality
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticNamedSuppressedDynamic,
	const FOpenMobileHapticPlaybackResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticNamedRejectedDynamic,
	const FOpenMobileHapticError&,
	Error
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(
	FOpenMobileHapticNamedCancelledDynamic
);

UCLASS(
	BlueprintType,
	Transient,
	meta = (ExposedAsyncProxy = "NamedHapticTask")
)
class OPENMOBILEHAPTICS_API UOpenMobileHapticNamedPlaybackAsyncAction final :
	public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Waiting For Preparation", ToolTip = "Fires when configured libraries or native resources are preparing asynchronously."))
	FOpenMobileHapticNamedWaitingDynamic WaitingForPreparation;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Accepted", ToolTip = "Fires once when the typed configured pattern is accepted. Playback is null only for fire-and-forget paths."))
	FOpenMobileHapticNamedAcceptedDynamic Accepted;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Suppressed", ToolTip = "Fires once for intentional silence caused by player policy, lifecycle, rate limiting, overlap, or zero output."))
	FOpenMobileHapticNamedSuppressedDynamic Suppressed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Rejected", ToolTip = "Fires once when the identifier, preparation, world, or native submission is invalid or unavailable."))
	FOpenMobileHapticNamedRejectedDynamic Rejected;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Cancelled", ToolTip = "Fires once when this caller stops waiting or its world or Game Instance ends."))
	FOpenMobileHapticNamedCancelledDynamic Cancelled;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Play", meta = (ToolTip = "Immediate typed submission result after preparation completes."))
	FOpenMobileHapticPlaybackResult ImmediateResult;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Play", meta = (ToolTip = "Request-scoped playback object for accepted controllable playback."))
	TObjectPtr<UOpenMobileHapticPlayback> Playback = nullptr;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Play", meta = (AdvancedDisplay = "Options", BlueprintInternalUseOnly = "true", CPP_Default_Intensity = "1.0", CPP_Default_PrepareIfNeeded = "true", DisplayName = "Play Named Haptic", Keywords = "haptic typed configured pattern prepare library vibrate rumble tactile", ToolTip = "Plays one typed configured pattern. When Prepare If Needed is enabled, the task owns preparation and never requires global delegate filtering.", WorldContext = "WorldContextObject"))
	static UOpenMobileHapticNamedPlaybackAsyncAction* PlayNamedHaptic(
		const UObject* WorldContextObject,
		FOpenMobileHapticPatternIdentifier Pattern,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized strength from zero through one. Wired values are clamped too."))
		float Intensity,
		UPARAM(DisplayName = "Prepare If Needed", meta = (ToolTip = "Asynchronously prepares configured libraries before submission when this identifier is not ready."))
		bool bPrepareIfNeeded,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	virtual void Activate() override;
	virtual void Cancel() override;

private:
	friend class FOpenMobileHapticsNamedPlaybackAsyncContractTest;
	friend class UOpenMobileHapticsSubsystem;

	UFUNCTION()
	void HandlePreparationFinished(
		const FOpenMobileHapticLibraryPreloadResult& Result
	);

	void SubmitPreparedPattern();
	void HandleWorldCleanup(
		UWorld* World,
		bool bSessionEnded,
		bool bCleanupResources
	);
	void HandleGameInstanceTeardown();
	void FinishRejected(FOpenMobileHapticError Error);
	void ReleasePreparationLease();
	void Cleanup();
	bool TryFinish();

	UPROPERTY(Transient)
	TObjectPtr<UObject> StoredWorldContextObject;

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileHapticPreparationLease> PreparationLease;

	TWeakObjectPtr<UOpenMobileHapticsSubsystem> Subsystem;
	TWeakObjectPtr<UWorld> TargetWorld;
	FDelegateHandle WorldCleanupHandle;
	FOpenMobileHapticLibraryPreloadHandle PreloadHandle;
	FOpenMobileHapticPatternIdentifier RequestedPattern;
	FOpenMobileHapticPlaybackOptions RequestedOptions;
	float RequestedIntensity = 1.0f;
	bool bRequestedPrepareIfNeeded = true;
	bool bFinished = false;
};
