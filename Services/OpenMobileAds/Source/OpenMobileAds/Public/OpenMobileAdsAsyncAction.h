#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "OpenMobileAdsEvents.h"
#include "OpenMobileAdsOperations.h"
#include "OpenMobileAdsAsyncAction.generated.h"

class UOpenMobileAdsSubsystem;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsAsyncCompleted,
	const FOpenMobileAdsEvent&,
	Event
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsAsyncFailed,
	const FOpenMobileAdsError&,
	Error
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsAsyncCancelled,
	const FOpenMobileAdsError&,
	Error
);

UCLASS(meta = (ExposedAsyncProxy = "AsyncAction"))
class OPENMOBILEADS_API UOpenMobileAdsAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdsAsyncCompleted OnCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdsAsyncFailed OnFailed;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdsAsyncCancelled OnCancelled;

	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Ads",
		meta = (
			BlueprintInternalUseOnly = "true",
			WorldContext = "WorldContextObject",
			DisplayName = "Load Ad Async"
		)
	)
	static UOpenMobileAdsAsyncAction* LoadAd(
		const UObject* WorldContextObject,
		FName Placement,
		FOpenMobileAdsLoadOptions Options
	);

	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Ads",
		meta = (
			BlueprintInternalUseOnly = "true",
			WorldContext = "WorldContextObject",
			DisplayName = "Show Ad Async"
		)
	)
	static UOpenMobileAdsAsyncAction* ShowAd(
		const UObject* WorldContextObject,
		FName Placement,
		FOpenMobileAdsShowOptions Options
	);

	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Ads",
		meta = (
			BlueprintInternalUseOnly = "true",
			WorldContext = "WorldContextObject",
			DisplayName = "Destroy Ad Async"
		)
	)
	static UOpenMobileAdsAsyncAction* DestroyAd(
		const UObject* WorldContextObject,
		FName Placement
	);

	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Ads",
		meta = (
			BlueprintInternalUseOnly = "true",
			WorldContext = "WorldContextObject",
			DisplayName = "Destroy All Ads Async"
		)
	)
	static UOpenMobileAdsAsyncAction* DestroyAllAds(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Ads")
	void Cancel();

	virtual void Activate() override;

private:
	enum class EOperation : uint8
	{
		Load,
		Show,
		Destroy,
		DestroyAll
	};

	static UOpenMobileAdsAsyncAction* Create(
		const UObject* WorldContextObject,
		EOperation Operation,
		FName Placement
	);
	void HandleAdsEvent(const FOpenMobileAdsEvent& Event);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void FinishCompleted(const FOpenMobileAdsEvent& Event);
	void FinishFailed(const FOpenMobileAdsError& Error);
	void FinishCancelled(const FOpenMobileAdsError& Error);
	void Cleanup();
	EOpenMobileAdsFailureStage GetFailureStage() const;

	UPROPERTY(Transient)
	TObjectPtr<UObject> StoredWorldContextObject;

	TWeakObjectPtr<UOpenMobileAdsSubsystem> Subsystem;
	TWeakObjectPtr<UWorld> TargetWorld;
	FDelegateHandle AdsEventHandle;
	FDelegateHandle WorldCleanupHandle;
	FOpenMobileAdsLoadOptions LoadOptions;
	FOpenMobileAdsShowOptions ShowOptions;
	FName Placement;
	FGuid RequestId;
	EOperation Operation = EOperation::Load;
	bool bFinished = false;
};
