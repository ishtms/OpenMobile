#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "OpenMobileAdsCapabilities.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsEvents.h"
#include "OpenMobileAdsInitialization.h"
#include "OpenMobileAdsOperations.h"
#include "OpenMobileAdsResults.h"
#include "OpenMobileCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OpenMobileAdsSubsystem.generated.h"

class IOpenMobileAdsProvider;
class IOpenMobileAdsProviderInitializationSink;
class IModularFeature;
class FOpenMobileAdsEventDispatcher;
struct FOpenMobileAdsActiveRequestContext;
enum class ENetworkConnectionType : uint8;

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

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Ads", meta = (DisplayName = "Initialize Ads"))
	FOpenMobileAdsOperationResult InitializeAds();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads", meta = (DisplayName = "Get Ads Service State"))
	EOpenMobileAdsServiceState GetServiceState() const { return ServiceState; }

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads", meta = (DisplayName = "Get Ads Initialization Status"))
	FOpenMobileAdsInitializationStatusSnapshot GetInitializationStatus() const
	{
		return InitializationStatus;
	}
	const FOpenMobileAdsInitializationStatusSnapshot& GetInitializationStatusRef() const
	{
		return InitializationStatus;
	}

	FOpenMobileAdsInitializationStatusNativeEvent& OnNativeInitializationStatusChanged()
	{
		return NativeInitializationStatusChanged;
	}

	const FOpenMobileAdsPrivacySnapshot& GetPrivacySnapshot() const
	{
		return PrivacySnapshot;
	}

	void UpdatePrivacySnapshot(FOpenMobileAdsPrivacySnapshot Snapshot);

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

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Ads", meta = (DisplayName = "Cancel Ads Request"))
	FOpenMobileAdsOperationResult CancelRequest(FGuid RequestId);

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

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdsInitializationStatusDynamicEvent OnInitializationStatusChanged;

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
	FOpenMobileAdsCanShowResult EvaluateCanShow(
		FName Placement,
		IOpenMobileAdsProvider* KnownProvider,
		const FOpenMobileAdsResolvedPlacement* KnownPlacement
	) const;
	void SubmitServiceEvent(FOpenMobileAdsEvent Event);
	void HandleInitializationCompleted(
		FGuid RequestId,
		FName ProviderName,
		FOpenMobileAdsError Error
	);
	void HandleProviderInitializationStatus(
		FGuid RequestId,
		FName ProviderName,
		FOpenMobileAdsInitializationComponentStatus Status
	);
	void BroadcastInitializationStatus();
	void UpsertInitializationComponent(
		FOpenMobileAdsInitializationComponentStatus Status
	);
	void UpdatePartialInitializationState();
	void HandleProviderEvent(FOpenMobileAdsEvent Event);
	void RecordImpression(FName Placement, FDateTime Timestamp);
	void HandleProviderUnregistered(const FName& FeatureName, IModularFeature* Feature);
	void HandleProviderUnavailable(FName ProviderName);
	void CancelSupersededRequest(FGuid RequestId);
	void ReleaseCachedAd(FOpenMobileAdsPlacementStatus& Status);
	void RememberDismissedShow(FGuid RequestId, FGuid CachedAdId);
	bool IsRememberedDismissedShow(FGuid RequestId, FGuid CachedAdId) const;
	void ForgetShowRewardContext(FGuid RequestId);
	void ExpireCachedAds();
	void ScheduleCacheExpirationCheck();
	bool HandleCacheExpirationTick(float DeltaTime);
	void HandleNetworkConnectionChanged(ENetworkConnectionType ConnectionType);
	void HandleApplicationWillDeactivate();
	void HandleApplicationHasReactivated();
	void HandleApplicationWillEnterBackground();
	void HandleApplicationHasEnteredForeground();
	void HandleAdLoaded();
	void HandleAdShown();
	void HandleRewardEarned(int32 NetworkAmount, FString NetworkRewardType);
	void HandleAdClosed();
	void HandleAdFailed(FOpenMobileError Error);

	UPROPERTY(Transient)
	EOpenMobileRewardedAdState State = EOpenMobileRewardedAdState::Idle;

	TMap<FName, FOpenMobileAdsPlacementStatus> PlacementStatuses;
	TSet<FGuid> RewardedShowRequests;
	TMap<FGuid, FGuid> DismissedShowCachedAds;
	TArray<FGuid> DismissedShowRequestOrder;
	TSet<FGuid> ImpressedCachedAds;
	TMap<FName, TArray<FDateTime>> ImpressionTimestampsByPlacement;
	TSet<FGuid> PendingExpiredCachedAdEvents;
	TMap<FGuid, TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>> ActiveRequests;
	TSet<FGuid> CancelledRequestEvents;
	TSharedPtr<FOpenMobileAdsEventDispatcher, ESPMode::ThreadSafe> EventDispatcher;
	FOpenMobileAdsNativeEvent NativeAdsEvent;
	FOpenMobileAdsInitializationStatusNativeEvent NativeInitializationStatusChanged;
	FDelegateHandle ProviderUnregisteredHandle;
	FDelegateHandle NetworkConnectionChangedHandle;
	FDelegateHandle ApplicationWillDeactivateHandle;
	FDelegateHandle ApplicationHasReactivatedHandle;
	FDelegateHandle ApplicationWillEnterBackgroundHandle;
	FDelegateHandle ApplicationHasEnteredForegroundHandle;
	FTSTicker::FDelegateHandle CacheExpirationTickerHandle;
	TSharedPtr<IOpenMobileAdsProviderInitializationSink, ESPMode::ThreadSafe> InitializationSink;
	FName SelectedProviderName;
	FGuid InitializationRequestId;
	FOpenMobileAdsError InitializationError;
	FOpenMobileAdsInitializationStatusSnapshot InitializationStatus;
	FOpenMobileAdsPrivacySnapshot PrivacySnapshot;
	double InitializationStartedSeconds = 0.0;
	EOpenMobileAdsServiceState ServiceState = EOpenMobileAdsServiceState::Uninitialized;
	bool bProviderInitializationStarted = false;
	bool bPrivacySnapshotInitialized = false;
	bool bRuntimeInitialized = false;
	bool bDeinitialized = false;
	bool bApplicationActive = true;
	bool bApplicationInForeground = true;
	TAtomic<bool> bPlatformOffline {false};
};
