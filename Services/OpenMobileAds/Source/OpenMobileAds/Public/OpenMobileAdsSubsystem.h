#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsCapabilities.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsEvents.h"
#include "OpenMobileAdsOperations.h"
#include "OpenMobileAdsResults.h"
#include "OpenMobileCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OpenMobileAdsSubsystem.generated.h"

class IOpenMobileAdsProvider;
class IModularFeature;
class FOpenMobileAdsEventDispatcher;

UENUM(BlueprintType)
enum class EOpenMobileRewardedAdState : uint8
{
	Idle,
	Loading,
	Showing
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOpenMobileAdSimpleEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileRewardEarnedEvent,
	int32,
	NetworkAmount,
	const FString&,
	NetworkRewardType
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdFailedEvent,
	const FOpenMobileError&,
	Error
);

/** Provider-neutral rewarded-ad API. Provider SDKs live in separate plugins. */
UCLASS()
class OPENMOBILEADS_API UOpenMobileAdsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Ads", meta = (DisplayName = "Load Ad"))
	FOpenMobileAdsOperationResult LoadAd(
		FName Placement,
		FOpenMobileAdsLoadOptions Options
	);

	FOpenMobileAdsOperationResult LoadAd(FName Placement)
	{
		return LoadAd(Placement, FOpenMobileAdsLoadOptions());
	}

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Ads", meta = (DisplayName = "Show Ad"))
	FOpenMobileAdsOperationResult ShowAd(
		FName Placement,
		FOpenMobileAdsShowOptions Options
	);

	FOpenMobileAdsOperationResult ShowAd(FName Placement)
	{
		return ShowAd(Placement, FOpenMobileAdsShowOptions());
	}

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Ads", meta = (DisplayName = "Destroy Ad"))
	FOpenMobileAdsOperationResult DestroyAd(FName Placement);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Ads", meta = (DisplayName = "Destroy All Ads"))
	FOpenMobileAdsOperationResult DestroyAllAds();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads", meta = (DisplayName = "Is Ad Ready"))
	bool IsReady(FName Placement) const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads", meta = (DisplayName = "Can Show Ad"))
	FOpenMobileAdsCanShowResult CanShow(FName Placement) const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads", meta = (DisplayName = "Get Ad Placement Status"))
	FOpenMobileAdsPlacementStatus GetPlacementStatus(FName Placement) const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads", meta = (DisplayName = "Get Ads Provider Capabilities"))
	FOpenMobileAdsProviderCapabilities GetProviderCapabilities() const;

	FOpenMobileAdsNativeEvent& OnNativeAdsEvent() { return NativeAdsEvent; }

	/** Loads and presents one rewarded ad through the selected provider. */
	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Ads")
	bool RequestAndShowRewardedAd();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads")
	bool IsSupported() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads")
	bool IsBusy() const { return State != EOpenMobileRewardedAdState::Idle; }

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads")
	EOpenMobileRewardedAdState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads")
	FName GetActiveProviderName() const;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdSimpleEvent OnAdLoaded;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdSimpleEvent OnAdShown;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileRewardEarnedEvent OnRewardEarned;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdSimpleEvent OnAdClosed;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdFailedEvent OnAdFailed;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdsDynamicEvent OnAdsEvent;

private:
	friend class FOpenMobileAdsEventDispatcher;

	IOpenMobileAdsProvider* FindProvider(FOpenMobileAdsError* OutError = nullptr) const;
	FName GetPreferredProviderName() const;
	void EnsureRuntime();
	const FOpenMobileAdsPlacementSettings* FindConfiguredPlacement(FName Placement) const;
	FOpenMobileAdsError ValidatePlacementForProvider(
		FName Placement,
		IOpenMobileAdsProvider*& OutProvider,
		FOpenMobileAdsResolvedPlacement& OutPlacement
	) const;
	void SubmitServiceEvent(FOpenMobileAdsEvent Event);
	void HandleProviderEvent(FOpenMobileAdsEvent Event);
	void HandleProviderUnregistered(const FName& FeatureName, IModularFeature* Feature);
	void HandleProviderUnavailable(FName ProviderName);
	void HandleAdLoaded();
	void HandleAdShown();
	void HandleRewardEarned(int32 NetworkAmount, FString NetworkRewardType);
	void HandleAdClosed();
	void HandleAdFailed(FOpenMobileError Error);

	UPROPERTY(Transient)
	EOpenMobileRewardedAdState State = EOpenMobileRewardedAdState::Idle;

	TMap<FName, FOpenMobileAdsPlacementStatus> PlacementStatuses;
	TSet<FGuid> RewardedCachedAds;
	TSet<FGuid> ImpressedCachedAds;
	TSharedPtr<FOpenMobileAdsEventDispatcher, ESPMode::ThreadSafe> EventDispatcher;
	FOpenMobileAdsNativeEvent NativeAdsEvent;
	FDelegateHandle ProviderUnregisteredHandle;
	bool bRuntimeInitialized = false;
	bool bDeinitialized = false;
};
