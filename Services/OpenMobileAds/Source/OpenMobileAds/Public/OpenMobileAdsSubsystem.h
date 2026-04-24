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
class IOpenMobileAdsConsentProviderSink;
class IModularFeature;
class FOpenMobileAdsEventDispatcher;
class FOpenMobileAdsFullscreenLifecycleCoordinator;
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

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads", meta = (DisplayName = "Get Consent Status"))
	FOpenMobileAdsPrivacySnapshot GetConsentStatus() const
	{
		return PrivacySnapshot;
	}

	const FOpenMobileAdsPrivacySnapshot& GetPrivacySnapshot() const
	{
		return PrivacySnapshot;
	}

	FOpenMobileAdsConsentStatusNativeEvent& OnNativeConsentStatusChanged()
	{
		return NativeConsentStatusChanged;
	}

	UFUNCTION(
		BlueprintPure,
		Category = "Open Mobile|Ads",
		meta = (DisplayName = "Get Tracking Authorization Status")
	)
	EOpenMobileAdsTrackingAuthorizationStatus
	GetTrackingAuthorizationStatus() const
	{
		return TrackingAuthorizationStatus;
	}

	FOpenMobileAdsTrackingAuthorizationStatusNativeEvent&
	OnNativeTrackingAuthorizationStatusChanged()
	{
		return NativeTrackingAuthorizationStatusChanged;
	}

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads")
	FOpenMobileAdsConsentSignalDeliverySnapshot
	GetConsentSignalDeliveryStatus() const
	{
		return ConsentSignalDeliveryStatus;
	}

	FOpenMobileAdsConsentSignalDeliveryNativeEvent&
	OnNativeConsentSignalDeliveryChanged()
	{
		return NativeConsentSignalDeliveryChanged;
	}

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads")
	FOpenMobileAdsCanRequestAdsResult CanRequestAds() const;

	FOpenMobileAdsCanRequestAdsNativeEvent& OnNativeCanRequestAdsChanged()
	{
		return NativeCanRequestAdsChanged;
	}

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Ads", meta = (DisplayName = "Refresh Consent"))
	FOpenMobileAdsOperationResult RefreshConsent();

	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Ads",
		meta = (DisplayName = "Reset Consent for Testing", DevelopmentOnly)
	)
	FOpenMobileAdsOperationResult ResetConsentForTesting();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads")
	bool IsPrivacyOptionsFormRequired() const
	{
		return PrivacySnapshot.IsConsentStatusFreshAt(FDateTime::UtcNow())
			&& PrivacySnapshot.UsPrivacy.PrivacyOptionsRequirement
			== EOpenMobileAdsPrivacyOptionsRequirement::Required;
	}

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Ads")
	bool IsPrivacyOptionsFormAvailable() const
	{
		return PrivacySnapshot.IsConsentStatusFreshAt(FDateTime::UtcNow())
			&& PrivacySnapshot.UsPrivacy.bPrivacyOptionsFormAvailable;
	}

	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Ads",
		meta = (DisplayName = "Present Privacy Options Form")
	)
	FOpenMobileAdsOperationResult PresentPrivacyOptionsForm();

	FOpenMobileAdsOperationResult UpdatePrivacySnapshot(
		FOpenMobileAdsPrivacySnapshot Snapshot
	);
	void ApplyConsentStatusUpdate(FOpenMobileAdsConsentStatusUpdate Update);

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

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdsConsentStatusDynamicEvent OnConsentStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdsTrackingAuthorizationStatusDynamicEvent
	OnTrackingAuthorizationStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdsConsentSignalDeliveryDynamicEvent
	OnConsentSignalDeliveryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Ads")
	FOpenMobileAdsCanRequestAdsDynamicEvent OnCanRequestAdsChanged;

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
	FOpenMobileAdsCanRequestAdsResult EvaluateCanRequestAds(
		IOpenMobileAdsProvider* KnownProvider
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
	void BroadcastConsentStatus();
	void RefreshTrackingAuthorizationStatus();
	void ApplyTrackingAuthorizationStatus(
		EOpenMobileAdsTrackingAuthorizationStatus Status
	);
	void PropagateConsentSignals(
		IOpenMobileAdsProvider& Provider,
		bool bRuntimeUpdate
	);
	void BroadcastConsentSignalDeliveryStatus();
	void RefreshCanRequestAdsDecision();
	void ApplyConsentStatusUpdateOnGameThread(
		FOpenMobileAdsConsentStatusUpdate Update
	);
	void HandleConsentRefreshCompleted(
		FGuid RequestId,
		FName AdsProviderName,
		FName ConsentProviderName,
		FOpenMobileAdsConsentStatusUpdate Update
	);
	void HandleConsentFormCompleted(
		FGuid RequestId,
		FName AdsProviderName,
		FName ConsentProviderName,
		FOpenMobileAdsConsentStatusUpdate Update
	);
	void HandleConsentOperationFailed(
		FGuid RequestId,
		FName AdsProviderName,
		FName ConsentProviderName,
		FOpenMobileAdsError Error
	);
	bool StartConsentForm(
		IOpenMobileAdsProvider& Provider,
		bool bPrivacyOptions
	);
	void ClearConsentOperation(bool bEndPresentation);
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
	FName ResolveConvenienceRewardedPlacement(FOpenMobileError& OutError) const;
	bool StartConvenienceRewardedShow();
	void HandleConvenienceRewardedEvent(const FOpenMobileAdsEvent& Event);
	void ResetConvenienceRewardedOperation();
	void ReportAdFailure(FOpenMobileError Error);

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
	TSharedPtr<FOpenMobileAdsFullscreenLifecycleCoordinator> FullscreenLifecycle;
	FOpenMobileAdsNativeEvent NativeAdsEvent;
	FOpenMobileAdsInitializationStatusNativeEvent NativeInitializationStatusChanged;
	FOpenMobileAdsConsentStatusNativeEvent NativeConsentStatusChanged;
	FOpenMobileAdsTrackingAuthorizationStatusNativeEvent
		NativeTrackingAuthorizationStatusChanged;
	FOpenMobileAdsConsentSignalDeliveryNativeEvent
		NativeConsentSignalDeliveryChanged;
	FOpenMobileAdsCanRequestAdsNativeEvent NativeCanRequestAdsChanged;
	FDelegateHandle ProviderUnregisteredHandle;
	FDelegateHandle NetworkConnectionChangedHandle;
	FDelegateHandle ApplicationWillDeactivateHandle;
	FDelegateHandle ApplicationHasReactivatedHandle;
	FDelegateHandle ApplicationWillEnterBackgroundHandle;
	FDelegateHandle ApplicationHasEnteredForegroundHandle;
	FTSTicker::FDelegateHandle CacheExpirationTickerHandle;
	TSharedPtr<IOpenMobileAdsProviderInitializationSink, ESPMode::ThreadSafe> InitializationSink;
	TSharedPtr<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> ConsentOperationSink;
	FName SelectedProviderName;
	FName ActiveConsentAdsProviderName;
	FName ActiveConsentProviderName;
	FName ActiveConvenienceRewardedPlacement;
	FGuid InitializationRequestId;
	FGuid ActiveConsentRequestId;
	FOpenMobileAdsConsentRequest ActiveConsentRequest;
	FGuid ConvenienceRewardedLoadRequestId;
	FGuid ConvenienceRewardedShowRequestId;
	FOpenMobileAdsError InitializationError;
	FOpenMobileAdsInitializationStatusSnapshot InitializationStatus;
	FOpenMobileAdsPrivacySnapshot PrivacySnapshot;
	EOpenMobileAdsTrackingAuthorizationStatus TrackingAuthorizationStatus =
		EOpenMobileAdsTrackingAuthorizationStatus::Unsupported;
	FOpenMobileAdsConsentSignalDeliverySnapshot ConsentSignalDeliveryStatus;
	FOpenMobileAdsConsentSignals LastPropagatedConsentSignals;
	FOpenMobileAdsCanRequestAdsResult LastCanRequestAdsDecision;
	double InitializationStartedSeconds = 0.0;
	EOpenMobileAdsServiceState ServiceState = EOpenMobileAdsServiceState::Uninitialized;
	bool bProviderInitializationStarted = false;
	bool bChildDirectedTreatmentLocked = false;
	bool bUnderAgeOfConsentLocked = false;
	bool bPrivacyOptionsPresentationActive = false;
	bool bPrivacySnapshotInitialized = false;
	bool bTrackingAuthorizationStatusInitialized = false;
	bool bConsentSignalsPropagated = false;
	bool bCanRequestAdsDecisionInitialized = false;
	bool bRuntimeInitialized = false;
	bool bDeinitialized = false;
	bool bApplicationActive = true;
	bool bApplicationInForeground = true;
	TAtomic<bool> bPlatformOffline {false};
};
