#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "OpenMobileAdsEvents.h"
#include "OpenMobileAdsOperations.h"
#include "OpenMobileAdsAsyncAction.generated.h"

class UOpenMobileAdsSubsystem;
class UWorld;
#if WITH_DEV_AUTOMATION_TESTS
class FOpenMobileAdsAsyncWorldCleanupTest;
#endif

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
	/** Fires once the requested Ads operation reaches its matching success event. */
	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdsAsyncCompleted OnCompleted;

	/** Fires when the service or provider rejects the operation before completion. */
	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdsAsyncFailed OnFailed;

	/** Fires when the caller, world cleanup, or subsystem teardown cancels the owned request. */
	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdsAsyncCancelled OnCancelled;

	/** Loads one configured placement and keeps the proxy alive till that request finishes. */
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

	/** Presents one ready placement and waits for its terminal provider event. */
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

	/** Hides a visible banner through the provider and reports whether its cache survived. */
	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Ads",
		meta = (
			BlueprintInternalUseOnly = "true",
			WorldContext = "WorldContextObject",
			DisplayName = "Hide Ad Async"
		)
	)
	static UOpenMobileAdsAsyncAction* HideAd(
		const UObject* WorldContextObject,
		FName Placement
	);

	/** Releases one placement and any provider-owned ad cached for it. */
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

	/** Releases every placement owned by this Game Instance and waits for the shared destroy event. */
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

	/** Cancels only this proxy's accepted request and leaves unrelated Ads work alone. */
	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Ads")
	void Cancel();

	/** Resolves the target world and submits the stored operation after Blueprint has bound its delegates. */
	virtual void Activate() override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FOpenMobileAdsAsyncWorldCleanupTest;
#endif

	enum class EOperation : uint8
	{
		Load,
		Show,
		Hide,
		Destroy,
		DestroyAll
	};

	static UOpenMobileAdsAsyncAction* Create(
		const UObject* WorldContextObject,
		EOperation Operation,
		FName Placement
	);
	/** Filters the shared event stream by request identity before completing this proxy. */
	void HandleAdsEvent(const FOpenMobileAdsEvent& Event);
	/** Cancels the proxy before its target world disappears and provider callbacks can arrive late. */
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	/** Broadcasts success once and disconnects every engine delegate owned by the proxy. */
	void FinishCompleted(const FOpenMobileAdsEvent& Event);
	/** Broadcasts a typed failure once and makes later provider events harmless. */
	void FinishFailed(const FOpenMobileAdsError& Error);
	/** Broadcasts cancellation separately so graphs don't mistake an expected stop for provider failure. */
	void FinishCancelled(const FOpenMobileAdsError& Error);
	/** Removes roots and delegates without asking the subsystem to cancel twice. */
	void Cleanup();
	/** Maps the stored operation to the failure stage used in locally created errors. */
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
