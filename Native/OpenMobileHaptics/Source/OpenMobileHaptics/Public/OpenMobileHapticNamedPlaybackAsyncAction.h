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

	/** Creates one caller-owned task, so waiting for preparation and the eventual playback result can't get mixed with another request. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Play", meta = (AdvancedDisplay = "Options", AutoCreateRefTerm = "Options", BlueprintInternalUseOnly = "true", CPP_Default_Intensity = "1.0", CPP_Default_PrepareIfNeeded = "true", DisplayName = "Play Named Haptic", Keywords = "haptic typed configured pattern prepare library vibrate rumble tactile", ToolTip = "Plays one typed configured pattern. When Prepare If Needed is enabled, the task owns preparation and never requires global delegate filtering.", WorldContext = "WorldContextObject"))
	static UOpenMobileHapticNamedPlaybackAsyncAction* PlayNamedHaptic(
		const UObject* WorldContextObject,
		FOpenMobileHapticPatternIdentifier Pattern,
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized strength from zero through one. Wired values are clamped too."))
		float Intensity,
		UPARAM(DisplayName = "Prepare If Needed", meta = (ToolTip = "Asynchronously prepares configured libraries before submission when this identifier is not ready."))
		bool bPrepareIfNeeded,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	/** Starts lookup or preparation only after Unreal has bound the Blueprint delegates. */
	virtual void Activate() override;

	/** Ends this caller's wait and releases its preparation claim without cancelling somebody else's claim also. */
	virtual void Cancel() override;

private:
	friend class FOpenMobileHapticsNamedPlaybackAsyncContractTest;
	friend class UOpenMobileHapticsSubsystem;

	/** Continues only the preload started for this task, so a global preparation event can't submit the wrong pattern. */
	UFUNCTION()
	void HandlePreparationFinished(
		const FOpenMobileHapticLibraryPreloadResult& Result
	);

	/** Submits after the named content is ready and hands its preparation lease to controllable playback when needed. */
	void SubmitPreparedPattern();

	/** Cancels world-scoped work before Unreal tears down the objects the request points at. */
	void HandleWorldCleanup(
		UWorld* World,
		bool bSessionEnded,
		bool bCleanupResources
	);
	/** Treats Game Instance shutdown as cancellation because this task can't safely outlive its subsystem. */
	void HandleGameInstanceTeardown();

	/** Publishes one rejected branch and keeps every later callback from finishing the task again. */
	void FinishRejected(FOpenMobileHapticError Error);

	/** Drops this task's ownership only, shared prepared content can stay alive for other callers. */
	void ReleasePreparationLease();

	/** Removes engine delegates and held objects once the task has reached any final branch. */
	void Cleanup();

	/** Claims the final transition atomically on the game thread so cleanup races still produce one result only. */
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
