#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticPatternPlaybackAsyncAction.generated.h"

struct FStreamableHandle;
class UOpenMobileHapticPatternAsset;
class UOpenMobileHapticPlayback;
class UOpenMobileHapticsSubsystem;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
	FOpenMobileHapticPatternWaitingDynamic
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileHapticPatternAcceptedDynamic,
	UOpenMobileHapticPlayback*,
	Playback,
	bool,
	UsedFallback,
	FName,
	ResolvedQuality
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticPatternSuppressedDynamic,
	const FOpenMobileHapticPlaybackResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticPatternRejectedDynamic,
	const FOpenMobileHapticError&,
	Error
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(
	FOpenMobileHapticPatternCancelledDynamic
);

UCLASS(
	BlueprintType,
	Transient,
	meta = (ExposedAsyncProxy = "PatternTask")
)
class OPENMOBILEHAPTICS_API UOpenMobileHapticPatternPlaybackAsyncAction final :
	public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Waiting For Preparation", ToolTip = "Fires when an unloaded platform override is being prepared asynchronously. No synchronous asset load occurs."))
	FOpenMobileHapticPatternWaitingDynamic WaitingForPreparation;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Accepted", ToolTip = "Fires once when asset playback is accepted. Playback is null for accepted fire-and-forget paths that expose no useful control handle."))
	FOpenMobileHapticPatternAcceptedDynamic Accepted;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Suppressed", ToolTip = "Fires once for intentional silence caused by player policy, lifecycle, rate limiting, overlap, or zero output."))
	FOpenMobileHapticPatternSuppressedDynamic Suppressed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Rejected", ToolTip = "Fires once when the asset, world, preparation, or native submission is invalid or unavailable."))
	FOpenMobileHapticPatternRejectedDynamic Rejected;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Play", meta = (DisplayName = "Cancelled", ToolTip = "Fires once when this task is cancelled before submission or its world ends."))
	FOpenMobileHapticPatternCancelledDynamic Cancelled;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Play", meta = (ToolTip = "Immediate typed submission result after the task leaves preparation."))
	FOpenMobileHapticPlaybackResult ImmediateResult;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Play", meta = (ToolTip = "Request-scoped playback object when accepted playback exposes a useful handle."))
	TObjectPtr<UOpenMobileHapticPlayback> Playback = nullptr;

	/** Creates one task for this asset, which keeps async override loading attached to the caller that requested it. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Play", meta = (AdvancedDisplay = "Options", AutoCreateRefTerm = "Options", BlueprintInternalUseOnly = "true", CPP_Default_Intensity = "1.0", CPP_Default_PrepareIfNeeded = "true", DisplayName = "Play Haptic Pattern Asset", Keywords = "haptic pattern asset vibrate rumble tactile feedback prepared", ToolTip = "Plays a selected Haptic Pattern asset without a raw name. Unloaded platform overrides wait asynchronously when Prepare If Needed is enabled; playback never loads synchronously.", WorldContext = "WorldContextObject"))
	static UOpenMobileHapticPatternPlaybackAsyncAction* PlayHapticPatternAsset(
		const UObject* WorldContextObject,
		UOpenMobileHapticPatternAsset* Pattern,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized strength from zero to one. Wired values are clamped too."))
		float Intensity,
		UPARAM(DisplayName = "Prepare If Needed", meta = (ToolTip = "Asynchronously loads an unloaded platform override before submission."))
		bool bPrepareIfNeeded,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	/** Starts work after Blueprint has connected every outcome delegate, otherwise fast failures could be missed. */
	virtual void Activate() override;

	/** Stops waiting and releases this task's async load without disturbing an asset another caller already owns. */
	virtual void Cancel() override;

private:
	/** Uses the asset only after its current-platform override is loaded, so gameplay never falls into a sync load. */
	void SubmitPreparedPattern();

	/** Resumes the exact request that initiated the streamable handle and ignores any unrelated preparation work. */
	void HandlePreparationFinished();

	/** Cancels the task while its world is still valid enough to remove delegates and release held assets. */
	void HandleWorldCleanup(
		UWorld* World,
		bool bSessionEnded,
		bool bCleanupResources
	);
	/** Sends one useful rejection and seals the task before late streaming callbacks arrive. */
	void FinishRejected(FOpenMobileHapticError Error);

	/** Clears world hooks and asset ownership after any final outcome. */
	void Cleanup();

	/** Allows only the first completion path through, since cancellation and loading can finish in the same frame. */
	bool TryFinish();

	UPROPERTY(Transient)
	TObjectPtr<UObject> StoredWorldContextObject;

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileHapticPatternAsset> RequestedPattern;

	TWeakObjectPtr<UOpenMobileHapticsSubsystem> Subsystem;
	TWeakObjectPtr<UWorld> TargetWorld;
	TSharedPtr<FStreamableHandle> PreparationHandle;
	FDelegateHandle WorldCleanupHandle;
	float RequestedIntensity = 1.0f;
	bool bRequestedPrepareIfNeeded = true;
	bool bFinished = false;
	FOpenMobileHapticPlaybackOptions RequestedOptions;
};
