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

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Prepare", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Prepare Haptics Async", Keywords = "haptic preload load library pattern ready", ToolTip = "Asynchronously prepares configured Haptics libraries and returns an owned lease. Every call reaches exactly one terminal branch, including invalid context, recovery rejection, cancellation, and teardown.", WorldContext = "WorldContextObject"))
	static UOpenMobileHapticPreparationAsyncAction* PrepareHapticsAsync(
		const UObject* WorldContextObject
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Prepare", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Prepare Haptic Library Async", Keywords = "haptic preload load library ready typed configured", ToolTip = "Validates the selected typed library and prepares the shared configured-library cache that contains it. The returned lease keeps shared named content ready until this caller releases it.", WorldContext = "WorldContextObject"))
	static UOpenMobileHapticPreparationAsyncAction* PrepareHapticLibraryAsync(
		const UObject* WorldContextObject,
		FOpenMobileHapticLibraryIdentifier Library
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Prepare", meta = (BlueprintInternalUseOnly = "true", DisplayName = "Prepare Haptic Pattern Async", Keywords = "haptic preload load pattern asset ready", ToolTip = "Asynchronously prepares a selected Haptic Pattern asset and its current-platform override without synchronous loading. The returned lease owns the loaded asset references.", WorldContext = "WorldContextObject"))
	static UOpenMobileHapticPreparationAsyncAction* PrepareHapticPatternAsync(
		const UObject* WorldContextObject,
		UOpenMobileHapticPatternAsset* Pattern
	);

	virtual void Activate() override;
	virtual void Cancel() override;

private:
	friend class FOpenMobileHapticsPreparationAsyncContractTest;
	friend class UOpenMobileHapticsSubsystem;

	UFUNCTION()
	void HandlePreparationFinished(
		const FOpenMobileHapticLibraryPreloadResult& Result
	);

	UFUNCTION()
	void HandlePatternPreparationFinished();

	void ActivateConfiguredLibraries();
	void ActivatePattern();
	void FinishPatternReady();
	void HandleWorldCleanup(
		UWorld* World,
		bool bSessionEnded,
		bool bCleanupResources
	);
	void HandleGameInstanceTeardown();
	void FinishReady(FOpenMobileHapticPreparationResult Result);
	void FinishCancelled(FOpenMobileHapticPreparationResult Result);
	void FinishFailed(FOpenMobileHapticPreparationResult Result);
	void Cleanup();
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
