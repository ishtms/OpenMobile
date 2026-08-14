#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticPreparationAsyncAction.generated.h"

class UOpenMobileHapticsSubsystem;
class UOpenMobileHapticPatternAsset;
class UGameInstance;
class UWorld;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticPreparationReadyDynamic,
	const FOpenMobileHapticPreparationResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticPreparationCancelledDynamic,
	const FOpenMobileHapticPreparationResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticPreparationFailedDynamic,
	const FOpenMobileHapticPreparationResult&,
	Result
);

UCLASS(
	BlueprintType,
	Transient,
	meta = (ExposedAsyncProxy = "PreparationTask")
)
class OPENMOBILEHAPTICS_API UOpenMobileHapticPreparationAsyncAction final :
	public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Ready", ToolTip = "Fires once with an owned lease when the requested Haptics content is ready."))
	FOpenMobileHapticPreparationReadyDynamic Ready;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Cancelled", ToolTip = "Fires once when this caller stops waiting or its world ends. Other preparation owners remain valid."))
	FOpenMobileHapticPreparationCancelledDynamic Cancelled;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Failed", ToolTip = "Fires once for invalid world context, recovery rejection, shutdown, invalid content, or native preparation failure."))
	FOpenMobileHapticPreparationFailedDynamic Failed;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Prepare", meta = (ToolTip = "Latest terminal result after Ready, Cancelled, or Failed fires."))
	FOpenMobileHapticPreparationResult PreparationResult;

	/** Creates a caller-owned preparation task for all configured content, so startup code doesn't have to filter shared delegates. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Prepare", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Prepare Haptics Async", Keywords = "haptic preload load library pattern ready", ToolTip = "Asynchronously prepares configured Haptics libraries and returns an owned lease. Every call reaches exactly one terminal branch, including invalid context, recovery rejection, cancellation, and teardown.", WorldContext = "WorldContextObject"))
	static UOpenMobileHapticPreparationAsyncAction* PrepareHapticsAsync(
		const UObject* WorldContextObject
	);

	/** Validates the typed library before sharing the prepared cache, which catches stale picker values at the caller. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Prepare", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Prepare Haptic Library Async", Keywords = "haptic preload load library ready typed configured", ToolTip = "Validates the selected typed library and prepares the shared configured-library cache that contains it. The returned lease keeps shared named content ready until this caller releases it.", WorldContext = "WorldContextObject"))
	static UOpenMobileHapticPreparationAsyncAction* PrepareHapticLibraryAsync(
		const UObject* WorldContextObject,
		FOpenMobileHapticLibraryIdentifier Library
	);

	/** Prepares one direct asset and its platform override without making the global named-library cache own either of them. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Prepare", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Prepare Haptic Pattern Async", Keywords = "haptic preload load pattern asset ready", ToolTip = "Asynchronously prepares a selected Haptic Pattern asset and its current-platform override without synchronous loading. The returned lease owns the loaded asset references.", WorldContext = "WorldContextObject"))
	static UOpenMobileHapticPreparationAsyncAction* PrepareHapticPatternAsync(
		const UObject* WorldContextObject,
		UOpenMobileHapticPatternAsset* Pattern
	);

	/** Starts the chosen preparation path after Blueprint has bound Ready, Cancelled, and Failed. */
	virtual void Activate() override;

	/** Stops this caller's wait and claim only, shared preparation can continue for somebody else. */
	virtual void Cancel() override;

private:
	friend class FOpenMobileHapticsPreparationAsyncContractTest;
	friend class UOpenMobileHapticsSubsystem;

	/** Accepts only the preload result tied to this task's handle, otherwise simultaneous callers would cross their outcomes. */
	UFUNCTION()
	void HandlePreparationFinished(
		const FOpenMobileHapticLibraryPreloadResult& Result
	);

	/** Finishes the direct-asset stream once both the pattern and its selected override are available. */
	UFUNCTION()
	void HandlePatternPreparationFinished();

	/** Starts shared configured-library preparation and records this task as one owner. */
	void ActivateConfiguredLibraries();
	/** Starts direct asset streaming, keeping it separate from named library lifetime. */
	void ActivatePattern();
	/** Creates the asset lease only after every requested object is loaded successfully. */
	void FinishPatternReady();
	/** Cancels before world teardown can leave the task holding invalid context. */
	void HandleWorldCleanup(
		UWorld* World,
		bool bSessionEnded,
		bool bCleanupResources
	);
	/** Ends outstanding work because a preparation lease can't outlive the Game Instance that owns its cache. */
	void HandleGameInstanceTeardown();
	/** Publishes the lease once and seals every other final branch. */
	void FinishReady(FOpenMobileHapticPreparationResult Result);
	/** Reports caller or lifecycle cancellation without turning it into a preparation failure. */
	void FinishCancelled(FOpenMobileHapticPreparationResult Result);
	/** Returns validation, loading, or native preparation errors through the one failure branch. */
	void FinishFailed(FOpenMobileHapticPreparationResult Result);
	/** Removes engine hooks and streamable ownership after any final result. */
	void Cleanup();
	/** Lets the first final path win when loading and teardown finish close together. */
	bool TryFinish();

	UPROPERTY(Transient)
	TObjectPtr<UObject> StoredWorldContextObject;

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileHapticPatternAsset> RequestedPattern;

	TWeakObjectPtr<UOpenMobileHapticsSubsystem> Subsystem;
	TWeakObjectPtr<UGameInstance> TargetGameInstance;
	TWeakObjectPtr<UWorld> TargetWorld;
	TSharedPtr<FStreamableHandle> PatternPreparationHandle;
	FOpenMobileHapticLibraryPreloadHandle PreloadHandle;
	FDelegateHandle WorldCleanupHandle;
	FOpenMobileHapticLibraryIdentifier RequestedLibrary;

	enum class EPreparationTarget : uint8
	{
		AllConfigured,
		ConfiguredLibrary,
		PatternAsset
	};

	EPreparationTarget PreparationTarget = EPreparationTarget::AllConfigured;
	bool bFinished = false;
};
