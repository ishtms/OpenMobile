#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "OpenMobileAdsEvents.h"
#include "OpenMobileAdsOperations.h"
#include "OpenMobileAdsRewardedAsyncAction.generated.h"

class UOpenMobileAdsSubsystem;
class UWorld;
#if WITH_DEV_AUTOMATION_TESTS
class FOpenMobileAdsRewardedAsyncContractTest;
#endif

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsRewardedMilestone,
	const FOpenMobileAdsEvent&,
	Event
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsRewardedFailed,
	const FOpenMobileAdsError&,
	Error
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsRewardedCancelled,
	const FOpenMobileAdsError&,
	Error
);

UCLASS(meta = (ExposedAsyncProxy = "AsyncAction"))
class OPENMOBILEADS_API UOpenMobileAdsRewardedAsyncAction final :
	public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Placements")
	FOpenMobileAdsRewardedMilestone OnShown;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Placements")
	FOpenMobileAdsRewardedMilestone OnRewardEarned;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Placements")
	FOpenMobileAdsRewardedMilestone OnDismissed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Placements")
	FOpenMobileAdsRewardedFailed OnFailed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Placements")
	FOpenMobileAdsRewardedCancelled OnCancelled;

	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Ads|Placements",
		meta = (
			BlueprintInternalUseOnly = "true",
			WorldContext = "WorldContextObject",
			DisplayName = "Show Rewarded Ad Async",
			ToolTip = "Shows a rewarded ad. After dismissal, reward delivery stays active until earned, cancelled, world teardown, or eviction from the last 64 dismissed show contexts."
		)
	)
	static UOpenMobileAdsRewardedAsyncAction* ShowRewardedAd(
		const UObject* WorldContextObject,
		UPARAM(meta = (GetOptions = "OpenMobileAds.OpenMobileAdsSubsystem.GetConfiguredAdsPlacementNames"))
		FName Placement,
		FOpenMobileAdsShowOptions Options
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Ads|Placements")
	void Cancel();

	virtual void Activate() override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FOpenMobileAdsRewardedAsyncContractTest;
#endif

	void HandleAdsEvent(const FOpenMobileAdsEvent& Event);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	bool HandleRewardSettlement(float DeltaTime);
	void FinishFailed(const FOpenMobileAdsError& Error);
	void FinishCancelled(const FOpenMobileAdsError& Error);
	void FinishListening();
	void Cleanup();

	UPROPERTY(Transient)
	TObjectPtr<UObject> StoredWorldContextObject;

	TWeakObjectPtr<UOpenMobileAdsSubsystem> Subsystem;
	TWeakObjectPtr<UWorld> TargetWorld;
	FName Placement;
	FOpenMobileAdsShowOptions Options;
	FGuid RequestId;
	FDelegateHandle AdsEventHandle;
	FDelegateHandle WorldCleanupHandle;
	FTSTicker::FDelegateHandle RewardSettlementHandle;
	bool bRewardReceived = false;
	bool bDismissed = false;
	bool bFinished = false;
};
