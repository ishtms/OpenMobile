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
class FOpenMobileAdsCooldownTracker;
class FOpenMobileAdsFullscreenLifecycleCoordinator;
class FOpenMobileAdsFrequencyCapTracker;
class IOpenMobileAdsClock;
class IOpenMobileAdsRetryRandomSource;
class IOpenMobileAdsRetryScheduler;
struct FOpenMobileAdsActiveRequestContext;
struct FOpenMobileAdsAutomaticPreloadContext;
struct FOpenMobileAdsClockTestAccess;
struct FOpenMobileAdsRetryTestAccess;
enum class ENetworkConnectionType : uint8;

enum class EOpenMobileAdsAppOpenOpportunity : uint8
{
	None,
	ColdStart,
	Foreground
};

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

/** Owns provider-neutral Ads state for one Game Instance while provider SDKs stay in separate plugins. */
UCLASS()
class OPENMOBILEADS_API UOpenMobileAdsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Binds provider discovery, lifecycle, connectivity, retry, and cache tracking for this Game Instance. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** Seals callback sinks and releases provider-owned work before the Game Instance goes away. */
	virtual void Deinitialize() override;

	/** Starts provider setup after configured privacy gates pass; repeated calls return the current request. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Ads|Setup", meta = (DisplayName = "Initialize Ads"))
	FOpenMobileAdsOperationResult InitializeAds();

	/** Returns the service lifecycle state without treating partial provider readiness as success. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Setup", meta = (DisplayName = "Get Ads Service State"))
	EOpenMobileAdsServiceState GetServiceState() const { return ServiceState; }

	/** Copies provider and adapter initialization rows for Blueprint callers. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Setup", meta = (DisplayName = "Get Ads Initialization Status"))
	FOpenMobileAdsInitializationStatusSnapshot GetInitializationStatus() const
	{
		return InitializationStatus;
	}
	/** Exposes the stored snapshot to native callers that can keep access within the subsystem lifetime. */
	const FOpenMobileAdsInitializationStatusSnapshot& GetInitializationStatusRef() const
	{
		return InitializationStatus;
	}

	/** Lets native tooling observe initialization updates without routing through reflected delegates. */
	FOpenMobileAdsInitializationStatusNativeEvent& OnNativeInitializationStatusChanged()
	{
		return NativeInitializationStatusChanged;
	}

	/** Copies the latest normalized consent and US privacy state for Blueprint callers. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Consent", meta = (DisplayName = "Get Ads Privacy Snapshot"))
	FOpenMobileAdsPrivacySnapshot GetConsentStatus() const
	{
		return PrivacySnapshot;
	}

	/** Exposes the stored privacy snapshot to native code without another copy. */
	const FOpenMobileAdsPrivacySnapshot& GetPrivacySnapshot() const
	{
		return PrivacySnapshot;
	}

	/** Lets native consumers observe consent state after provider values have been normalized. */
	FOpenMobileAdsConsentStatusNativeEvent& OnNativeConsentStatusChanged()
	{
		return NativeConsentStatusChanged;
	}

	/** Returns the cached platform authorization answer and never opens the system prompt. */
	UFUNCTION(
		BlueprintPure,
		Category = "OpenMobile|Ads|Consent",
		meta = (DisplayName = "Get Tracking Authorization Status")
	)
	EOpenMobileAdsTrackingAuthorizationStatus
	GetTrackingAuthorizationStatus() const
	{
		return TrackingAuthorizationStatus;
	}

	/** Lets native consumers observe authorization changes without polling the platform backend. */
	FOpenMobileAdsTrackingAuthorizationStatusNativeEvent&
	OnNativeTrackingAuthorizationStatusChanged()
	{
		return NativeTrackingAuthorizationStatusChanged;
	}

	FOpenMobileAdsTrackingAuthorizationRequestNativeEvent&
	OnNativeTrackingAuthorizationRequestCompleted()
	{
		return NativeTrackingAuthorizationRequestCompleted;
	}

	/** Reports availability only when platform authorization and identifier access both allow it. */
	UFUNCTION(
		BlueprintPure,
		Category = "OpenMobile|Ads|Consent",
		meta = (DisplayName = "Is Advertising Identifier Available")
	)
	bool IsAdvertisingIdentifierAvailable() const;

	/** Opens the platform tracking prompt when configured, otherwise returns the current typed refusal. */
	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Ads|Consent",
		meta = (DisplayName = "Request iOS Tracking Authorization")
	)
	FOpenMobileAdsOperationResult RequestTrackingAuthorization();

	/** Shows how every provider and mediation consumer handled the latest configured privacy signals. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Diagnostics")
	FOpenMobileAdsConsentSignalDeliverySnapshot
	GetConsentSignalDeliveryStatus() const
	{
		return ConsentSignalDeliveryStatus;
	}

	/** Lets native diagnostics observe per-consumer privacy delivery updates. */
	FOpenMobileAdsConsentSignalDeliveryNativeEvent&
	OnNativeConsentSignalDeliveryChanged()
	{
		return NativeConsentSignalDeliveryChanged;
	}

	/** Evaluates initialization, consent, tracking, and provider policy without starting an ad request. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Setup", meta = (DisplayName = "Get Ads Request Eligibility"))
	FOpenMobileAdsCanRequestAdsResult CanRequestAds() const;

	/** Lets native callers react when the service-wide request decision changes. */
	FOpenMobileAdsCanRequestAdsNativeEvent& OnNativeCanRequestAdsChanged()
	{
		return NativeCanRequestAdsChanged;
	}

	/** Refreshes provider consent state and presents a required form when the provider asks for one. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Ads|Consent", meta = (DisplayName = "Refresh Ads Consent"))
	FOpenMobileAdsOperationResult RefreshConsent();

	/** Clears provider consent only in development test mode, production state stays protected. */
	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Ads|Consent",
		meta = (DisplayName = "Reset Consent for Testing", DevelopmentOnly)
	)
	FOpenMobileAdsOperationResult ResetConsentForTesting();

	/** Requires a fresh provider snapshot before claiming the privacy options form is required. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Consent")
	bool IsPrivacyOptionsFormRequired() const
	{
		return PrivacySnapshot.IsConsentStatusFreshAt(FDateTime::UtcNow())
			&& PrivacySnapshot.UsPrivacy.PrivacyOptionsRequirement
			== EOpenMobileAdsPrivacyOptionsRequirement::Required;
	}

	/** Requires a fresh provider snapshot before claiming the privacy options form can be shown. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Consent")
	bool IsPrivacyOptionsFormAvailable() const
	{
		return PrivacySnapshot.IsConsentStatusFreshAt(FDateTime::UtcNow())
			&& PrivacySnapshot.UsPrivacy.bPrivacyOptionsFormAvailable;
	}

	/** Presents the provider's privacy options form without starting a normal consent refresh. */
	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Ads|Consent",
		meta = (DisplayName = "Present Ads Privacy Options Form")
	)
	FOpenMobileAdsOperationResult PresentPrivacyOptionsForm();

	/** Normalizes an externally supplied snapshot and re-evaluates request eligibility. */
	FOpenMobileAdsOperationResult UpdatePrivacySnapshot(
		FOpenMobileAdsPrivacySnapshot Snapshot
	);
	/** Applies an asynchronous provider consent update on the owning Game Instance. */
	void ApplyConsentStatusUpdate(FOpenMobileAdsConsentStatusUpdate Update);

	/** Loads one configured placement after provider, privacy, retry, and cache policy agree. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Ads|Placements", meta = (DisplayName = "Load Ad"))
	FOpenMobileAdsOperationResult LoadAd(
		UPARAM(meta = (GetOptions = "GetConfiguredAdsPlacementNames"))
		FName Placement,
		FOpenMobileAdsLoadOptions Options
	);

	/** Uses ordinary load options for native callers that don't need to force a reload. */
	FOpenMobileAdsOperationResult LoadAd(FName Placement)
	{
		return LoadAd(Placement, FOpenMobileAdsLoadOptions());
	}

	/** Replaces any reusable cached ad for the placement before starting a fresh provider load. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Ads|Placements", meta = (DisplayName = "Reload Ad"))
	FOpenMobileAdsOperationResult ReloadAd(
		UPARAM(meta = (GetOptions = "GetConfiguredAdsPlacementNames"))
		FName Placement
	);

	/** Presents one ready ad only after format, privacy, cooldown, cap, and lifecycle checks pass. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Ads|Placements", meta = (DisplayName = "Show Ad"))
	FOpenMobileAdsOperationResult ShowAd(
		UPARAM(meta = (GetOptions = "GetConfiguredAdsPlacementNames"))
		FName Placement,
		FOpenMobileAdsShowOptions Options
	);

	/** Uses ordinary show options for native callers that don't need per-show verification data. */
	FOpenMobileAdsOperationResult ShowAd(FName Placement)
	{
		return ShowAd(Placement, FOpenMobileAdsShowOptions());
	}

	/** Hides a provider-supported visible placement and follows its configured cache policy. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Ads|Placements", meta = (DisplayName = "Hide Ad"))
	FOpenMobileAdsOperationResult HideAd(
		UPARAM(meta = (GetOptions = "GetConfiguredAdsPlacementNames"))
		FName Placement
	);

	/** Releases the placement's provider object, cached identity, and pending automatic work. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Ads|Placements", meta = (DisplayName = "Destroy Ad"))
	FOpenMobileAdsOperationResult DestroyAd(
		UPARAM(meta = (GetOptions = "GetConfiguredAdsPlacementNames"))
		FName Placement
	);

	/** Releases every provider-owned placement while preserving the selected provider itself. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Ads|Placements", meta = (DisplayName = "Destroy All Ads"))
	FOpenMobileAdsOperationResult DestroyAllAds();

	/** Cancels one accepted load or show request and rejects stale request IDs. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Ads|Advanced", meta = (DisplayName = "Cancel Ads Request"))
	FOpenMobileAdsOperationResult CancelRequest(FGuid RequestId);

	/** Checks current cache state and expiry without asking the provider to load anything. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Placements", meta = (DisplayName = "Is Ad Ready"))
	bool IsReady(
		UPARAM(meta = (GetOptions = "GetConfiguredAdsPlacementNames"))
		FName Placement
	) const;

	/** Explains the first policy reason a placement can't be presented right now. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Placements", meta = (DisplayName = "Can Show Placement"))
	FOpenMobileAdsCanShowResult CanShow(
		UPARAM(meta = (GetOptions = "GetConfiguredAdsPlacementNames"))
		FName Placement
	) const;

	/** Copies service-tracked placement state without making a native SDK query. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Placements", meta = (DisplayName = "Get Ad Placement Status"))
	FOpenMobileAdsPlacementStatus GetPlacementStatus(
		UPARAM(meta = (GetOptions = "GetConfiguredAdsPlacementNames"))
		FName Placement
	) const;

	/** Tells automatic App Open policy whether the game's own startup presentation is ready. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Ads|App Open", meta = (DisplayName = "Set App Open Readiness"))
	void SetAppOpenPresentationState(
		FOpenMobileAdsAppOpenPresentationState PresentationState
	);

	/** Returns the latest game-supplied App Open presentation gates. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|App Open", meta = (DisplayName = "Get App Open Readiness"))
	FOpenMobileAdsAppOpenPresentationState GetAppOpenPresentationState() const
	{
		return AppOpenPresentationState;
	}

	/** Returns selected-provider support without assuming every format shares the same limits. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Advanced", meta = (DisplayName = "Get Ads Provider Capabilities"))
	FOpenMobileAdsProviderCapabilities GetProviderCapabilities() const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Advanced", meta = (DisplayName = "Get Ads Format Capabilities"))
	bool GetAdsFormatCapabilities(
		EOpenMobileAdFormat Format,
		FOpenMobileAdFormatCapabilities& Capabilities
	) const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Diagnostics", meta = (DisplayName = "Get Ads Initialization Component"))
	bool GetAdsInitializationComponent(
		EOpenMobileAdsInitializationComponentType Type,
		FName Name,
		FName Parent,
		FOpenMobileAdsInitializationComponentStatus& Component
	) const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Diagnostics", meta = (DisplayName = "Get Ads Consent Signal Consumer Status"))
	bool GetAdsConsentSignalConsumerStatus(
		EOpenMobileAdsConsentSignalConsumerType Type,
		FName Name,
		FName Parent,
		FOpenMobileAdsConsentSignalDeliveryStatus& Status
	) const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Diagnostics", meta = (DisplayName = "Ads Revenue Micros to Major Units", ToolTip = "Converts authoritative revenue micros to a display value. Floating-point output can lose accounting precision."))
	static double AdsRevenueMicrosToMajorUnits(int64 ValueMicros);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Advanced", meta = (BlueprintInternalUseOnly = "true"))
	static TArray<FName> GetConfiguredAdsPlacementNames();

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Advanced", meta = (BlueprintInternalUseOnly = "true"))
	static TArray<FName> GetRegisteredAdsProviderNames();

	/** Exposes the ordered provider-neutral event stream to native consumers. */
	FOpenMobileAdsNativeEvent& OnNativeAdsEvent() { return NativeAdsEvent; }

	/** Loads and presents the configured convenience rewarded placement as one legacy operation. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Ads|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use named placement load and rewarded show async nodes.", DisplayName = "Request and Show Convenience Rewarded Ad"))
	bool RequestAndShowRewardedAd();

	/** Reports whether the selected provider can serve the convenience rewarded placement. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use Get Ads Provider Capabilities and named placements.", DisplayName = "Is Convenience Rewarded Supported"))
	bool IsSupported() const;

	/** Tracks only the convenience rewarded operation, other placements can still be active. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use request-local async action state.", DisplayName = "Is Convenience Rewarded Busy"))
	bool IsBusy() const { return State != EOpenMobileRewardedAdState::Idle; }

	/** Returns the legacy rewarded flow state without collapsing loading and presentation together. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Legacy", meta = (DeprecatedFunction, DeprecationMessage = "Use named placement status and request-local async actions.", DisplayName = "Get Convenience Rewarded State"))
	EOpenMobileRewardedAdState GetState() const { return State; }

	/** Returns the provider selected for this Game Instance after configuration resolution. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Ads|Setup", meta = (DisplayName = "Get Active Ads Provider"))
	FName GetActiveProviderName() const;

	/** Fires when the legacy convenience rewarded placement finishes loading. */
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Use placement-aware async nodes or focused placement delegates."))
	FOpenMobileAdSimpleEvent OnAdLoaded;

	/** Fires when the legacy convenience rewarded placement reaches visible presentation. */
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Use placement-aware async nodes or focused placement delegates."))
	FOpenMobileAdSimpleEvent OnAdShown;

	/** Carries the normalized reward from the legacy convenience flow, local receipt isn't backend verification. */
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Use Show Rewarded Ad Async."))
	FOpenMobileRewardEarnedEvent OnRewardEarned;

	/** Fires when the legacy convenience rewarded presentation has closed. */
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Use Show Rewarded Ad Async or the placement dismissed delegate."))
	FOpenMobileAdSimpleEvent OnAdClosed;

	/** Reports a common typed failure from the legacy convenience rewarded flow. */
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Use placement-aware async failure outputs."))
	FOpenMobileAdFailedEvent OnAdFailed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Placements")
	FOpenMobileAdsDynamicEvent OnPlacementLoaded;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Placements")
	FOpenMobileAdsDynamicEvent OnPlacementShown;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Placements")
	FOpenMobileAdsDynamicEvent OnPlacementRewardEarned;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Placements")
	FOpenMobileAdsDynamicEvent OnPlacementDismissed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Placements")
	FOpenMobileAdsDynamicEvent OnPlacementRevenuePaid;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Placements")
	FOpenMobileAdsDynamicEvent OnPlacementFailed;

	/** Broadcasts the ordered provider-neutral stream for every configured placement. */
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Advanced")
	FOpenMobileAdsDynamicEvent OnAdsEvent;

	/** Broadcasts provider, network, and adapter startup changes during initialization. */
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Setup")
	FOpenMobileAdsInitializationStatusDynamicEvent OnInitializationStatusChanged;

	/** Broadcasts normalized consent changes after freshness and provider details are applied. */
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Consent")
	FOpenMobileAdsConsentStatusDynamicEvent OnConsentStatusChanged;

	/** Broadcasts platform tracking authorization changes without exposing the native framework. */
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Consent")
	FOpenMobileAdsTrackingAuthorizationStatusDynamicEvent
	OnTrackingAuthorizationStatusChanged;

	/** Broadcasts how each provider or adapter handled the latest privacy signal set. */
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Diagnostics")
	FOpenMobileAdsConsentSignalDeliveryDynamicEvent
	OnConsentSignalDeliveryChanged;

	/** Broadcasts only when the combined service-wide request decision actually changes. */
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Setup")
	FOpenMobileAdsCanRequestAdsDynamicEvent OnCanRequestAdsChanged;

private:
	friend class FOpenMobileAdsEventDispatcher;
	friend struct FOpenMobileAdsClockTestAccess;
	friend struct FOpenMobileAdsRetryTestAccess;

	/** Resolves the selected modular provider and returns configuration failure context when none is usable. */
	IOpenMobileAdsProvider* FindProvider(FOpenMobileAdsError* OutError = nullptr) const;
	/** Reads the configured preference without caching a provider pointer across module changes. */
	FName GetPreferredProviderName() const;
	/** Creates service-owned trackers and delegate bindings once before any request path uses them. */
	void EnsureRuntime();
	/** Returns the configured placement before platform overrides and provider checks are applied. */
	const FOpenMobileAdsPlacementSettings* FindConfiguredPlacement(FName Placement) const;
	/** Resolves provider and platform placement together so request paths share one validation result. */
	FOpenMobileAdsError ValidatePlacementForProvider(
		FName Placement,
		IOpenMobileAdsProvider*& OutProvider,
		FOpenMobileAdsResolvedPlacement& OutPlacement
	) const;
	/** Applies readiness, expiry, privacy, pacing, connectivity, and fullscreen gates in stable order. */
	FOpenMobileAdsCanShowResult EvaluateCanShow(
		FName Placement,
		IOpenMobileAdsProvider* KnownProvider,
		const FOpenMobileAdsResolvedPlacement* KnownPlacement
	) const;
	/** Combines service state, consent freshness, signal delivery, and provider policy into one request answer. */
	FOpenMobileAdsCanRequestAdsResult EvaluateCanRequestAds(
		IOpenMobileAdsProvider* KnownProvider
	) const;
	/** Assigns service ordering and routes one normalized event through state updates before delegates. */
	void SubmitServiceEvent(FOpenMobileAdsEvent Event);
	void BroadcastFocusedPlacementEvent(const FOpenMobileAdsEvent& Event);
	/** Accepts completion only for the current initialization request and selected provider. */
	void HandleInitializationCompleted(
		FGuid RequestId,
		FName ProviderName,
		FOpenMobileAdsError Error
	);
	/** Merges progressive provider or adapter startup state only while its request is current. */
	void HandleProviderInitializationStatus(
		FGuid RequestId,
		FName ProviderName,
		FOpenMobileAdsInitializationComponentStatus Status
	);
	/** Copies the current startup snapshot to native and Blueprint listeners after timestamps are updated. */
	void BroadcastInitializationStatus();
	/** Broadcasts the normalized privacy snapshot and then rechecks request eligibility. */
	void BroadcastConsentStatus();
	/** Re-queries the platform backend after lifecycle or configuration can change authorization visibility. */
	void RefreshTrackingAuthorizationStatus();
	/** Stores one authorization answer and broadcasts only when it changed. */
	void ApplyTrackingAuthorizationStatus(
		EOpenMobileAdsTrackingAuthorizationStatus Status
	);
	/** Ignores stale platform prompt callbacks by matching the active authorization request. */
	void HandleTrackingAuthorizationCompleted(
		FGuid RequestId,
		EOpenMobileAdsTrackingAuthorizationStatus Status
	);
	/** Delivers configured privacy bits to the provider and its registered adapter consumers. */
	void PropagateConsentSignals(
		IOpenMobileAdsProvider& Provider,
		bool bRuntimeUpdate
	);
	/** Publishes per-consumer delivery status after every row reflects the same signal generation. */
	void BroadcastConsentSignalDeliveryStatus();
	/** Re-evaluates the service-wide request answer and emits a change only when fields differ. */
	void RefreshCanRequestAdsDecision();
	/** Moves an asynchronous consent update to the game thread before touching subsystem state. */
	void ApplyConsentStatusUpdateOnGameThread(
		FOpenMobileAdsConsentStatusUpdate Update
	);
	/** Accepts consent refresh completion only from the active Ads and consent provider pair. */
	void HandleConsentRefreshCompleted(
		FGuid RequestId,
		FName AdsProviderName,
		FName ConsentProviderName,
		FOpenMobileAdsConsentStatusUpdate Update
	);
	/** Ends presentation ownership before applying the active form's provider update. */
	void HandleConsentFormCompleted(
		FGuid RequestId,
		FName AdsProviderName,
		FName ConsentProviderName,
		FOpenMobileAdsConsentStatusUpdate Update
	);
	/** Clears only the matching consent operation and preserves the last known privacy values on failure. */
	void HandleConsentOperationFailed(
		FGuid RequestId,
		FName AdsProviderName,
		FName ConsentProviderName,
		FOpenMobileAdsError Error
	);
	/** Starts either required or user-invoked form presentation with shared fullscreen exclusion. */
	bool StartConsentForm(
		IOpenMobileAdsProvider& Provider,
		bool bPrivacyOptions
	);
	/** Invalidates the active consent sink and optionally releases fullscreen presentation ownership. */
	void ClearConsentOperation(bool bEndPresentation);
	/** Replaces one initialization row by its type, name, and parent identity. */
	void UpsertInitializationComponent(
		FOpenMobileAdsInitializationComponentStatus Status
	);
	/** Marks partial readiness only when usable provider startup coexists with failed optional components. */
	void UpdatePartialInitializationState();
	/** Reconciles provider callbacks with service-owned request, cache, reward, and placement state. */
	void HandleProviderEvent(FOpenMobileAdsEvent Event);
	/** Records a counted impression once per cached ad before cooldown and cap policy are updated. */
	void RecordImpression(FName Placement);
	/** Drops the active provider safely when its modular feature unloads during the Game Instance lifetime. */
	void HandleProviderUnregistered(const FName& FeatureName, IModularFeature* Feature);
	/** Fails or releases outstanding work owned by a provider that can no longer receive calls. */
	void HandleProviderUnavailable(FName ProviderName);
	/** Schedules only retry-classified load failures and keeps connectivity or privacy deferral explicit. */
	bool TryScheduleLoadRetry(const FOpenMobileAdsEvent& Event);
	/** Re-submits one retry only while its request context, placement, and provider are still current. */
	void StartPendingLoadRetry(FGuid RequestId);
	/** Cancels the scheduler token before its request context can be removed or replaced. */
	void CancelRetrySchedule(FOpenMobileAdsActiveRequestContext& Context);
	/** Resumes retries held specifically for offline state after connectivity returns. */
	void ResumeConnectivityDeferredRetries();
	/** Stops retries that can no longer proceed after a blocking privacy decision. */
	void StopPrivacyBlockedRetries();
	/** Creates automatic preload ownership for every enabled placement whose policy requests it. */
	void RequestConfiguredAutomaticPreloads();
	/** Coalesces one placement's automatic preload request and respects its minimum recovery delay. */
	void RequestAutomaticPreload(FName Placement, double MinimumDelaySeconds);
	/** Arms the placement's preload callback only after current policy resolves an eligible delay. */
	void ScheduleAutomaticPreload(FName Placement);
	/** Converts automatic ownership into an ordinary load request without creating duplicate cache work. */
	void StartAutomaticPreload(FName Placement);
	/** Resolves configured, retry, lifecycle, and cache timing into one preload scheduling decision. */
	bool ResolveAutomaticPreloadDelay(
		FName Placement,
		double& OutDelaySeconds,
		bool& bOutCancel
	) const;
	/** Stops automatic preload timers while lifecycle state makes provider work unsafe. */
	void PauseAutomaticPreloads();
	/** Rechecks every owned automatic preload after lifecycle, privacy, or connectivity changes. */
	void ReevaluateAutomaticPreloads();
	/** Removes timer and ownership for one placement without cancelling a caller-owned load. */
	void CancelAutomaticPreload(FName Placement);
	/** Removes every automatic timer and context during provider replacement or teardown. */
	void CancelAllAutomaticPreloads();
	/** Emits one stored load failure after retry policy has decided no further attempt will run. */
	void SubmitPendingLoadFailure(
		FGuid RequestId,
		FOpenMobileAdsError Error
	);
	/** Cancels a replaced request context before a newer operation takes ownership of the placement. */
	void CancelSupersededRequest(FGuid RequestId);
	/** Tells the provider to release one cached identity before clearing service-side cache metadata. */
	void ReleaseCachedAd(FOpenMobileAdsPlacementStatus& Status);
	/** Remembers terminal fullscreen identity so duplicate dismiss callbacks can't finish a newer show. */
	void RememberDismissedShow(FGuid RequestId, FGuid CachedAdId);
	/** Matches both request and cached ad because providers can reuse callback objects across loads. */
	bool IsRememberedDismissedShow(FGuid RequestId, FGuid CachedAdId) const;
	/** Drops reward eligibility once the matching show has reached a terminal state. */
	void ForgetShowRewardContext(FGuid RequestId);
	/** Uses the injected clock when tests need cache expiry independent from wall time. */
	FDateTime GetCacheUtcNow() const;
	/** Uses monotonic time for deadlines so system clock changes can't extend cached ad life. */
	double GetCacheMonotonicSeconds() const;
	/** Checks the service-owned monotonic deadline without trusting a provider callback timestamp. */
	bool IsCachedAdExpired(const FOpenMobileAdsPlacementStatus& Status) const;
	/** Expires every due cached identity once and emits provider-neutral terminal events. */
	void ExpireCachedAds();
	/** Arms one ticker for the nearest cache deadline instead of polling every placement continuously. */
	void ScheduleCacheExpirationCheck();
	/** Runs cache expiry on the game thread and removes itself when no deadline remains. */
	bool HandleCacheExpirationTick(float DeltaTime);
	/** Defers or resumes retry and preload work according to the platform's latest connection class. */
	void HandleNetworkConnectionChanged(ENetworkConnectionType ConnectionType);
	/** Prevents a deactivating application from starting new fullscreen or automatic work. */
	void HandleApplicationWillDeactivate();
	/** Rechecks automatic work after the application becomes active again. */
	void HandleApplicationHasReactivated();
	/** Records background entry while preserving whether fullscreen presentation caused it. */
	void HandleApplicationWillEnterBackground();
	/** Creates a foreground App Open opportunity only after an eligible real background stay. */
	void HandleApplicationHasEnteredForeground();
	/** Presents the configured App Open placement only inside the active opportunity window. */
	void TryPresentAutomaticAppOpen();
	/** Applies cold-start or foreground timing, readiness, cache age, and game-supplied presentation gates. */
	bool IsAutomaticAppOpenEligible(
		const FOpenMobileAdsResolvedPlacement& Placement
	) const;
	/** Ends the current App Open opportunity so later lifecycle callbacks must create a fresh one. */
	void ClearAutomaticAppOpenOpportunity();
	/** Uses an explicit convenience placement or the sole enabled rewarded placement when unambiguous. */
	FName ResolveConvenienceRewardedPlacement(FOpenMobileError& OutError) const;
	/** Starts presentation only after the convenience load event confirms the expected placement is ready. */
	bool StartConvenienceRewardedShow();
	/** Translates the matching provider-neutral event stream to the legacy rewarded delegates. */
	void HandleConvenienceRewardedEvent(const FOpenMobileAdsEvent& Event);
	/** Clears legacy request identities and state without touching independent Ads operations. */
	void ResetConvenienceRewardedOperation();
	/** Finishes the legacy rewarded flow with one common error and resets its state. */
	void ReportAdFailure(FOpenMobileError Error);

	UPROPERTY(Transient)
	EOpenMobileRewardedAdState State = EOpenMobileRewardedAdState::Idle;

	TMap<FName, FOpenMobileAdsPlacementStatus> PlacementStatuses;
	TSet<FGuid> RewardedShowRequests;
	TMap<FGuid, FGuid> DismissedShowCachedAds;
	TArray<FGuid> DismissedShowRequestOrder;
	TSet<FGuid> ImpressedCachedAds;
	TMap<FGuid, double> CacheExpirationMonotonicDeadlines;
	TSet<FGuid> PendingExpiredCachedAdEvents;
	TMap<FGuid, TSharedPtr<FOpenMobileAdsActiveRequestContext, ESPMode::ThreadSafe>> ActiveRequests;
	TMap<FName, TSharedPtr<FOpenMobileAdsAutomaticPreloadContext>> AutomaticPreloads;
	TSet<FGuid> CancelledRequestEvents;
	TSharedPtr<FOpenMobileAdsEventDispatcher, ESPMode::ThreadSafe> EventDispatcher;
	TSharedPtr<FOpenMobileAdsCooldownTracker> CooldownTracker;
	TSharedPtr<FOpenMobileAdsFullscreenLifecycleCoordinator> FullscreenLifecycle;
	TSharedPtr<FOpenMobileAdsFrequencyCapTracker> FrequencyCapTracker;
	TSharedPtr<IOpenMobileAdsClock> CacheClock;
	TSharedPtr<IOpenMobileAdsRetryRandomSource> RetryRandomSource;
	TSharedPtr<IOpenMobileAdsRetryScheduler> RetryScheduler;
	FOpenMobileAdsNativeEvent NativeAdsEvent;
	FOpenMobileAdsInitializationStatusNativeEvent NativeInitializationStatusChanged;
	FOpenMobileAdsConsentStatusNativeEvent NativeConsentStatusChanged;
	FOpenMobileAdsTrackingAuthorizationStatusNativeEvent
		NativeTrackingAuthorizationStatusChanged;
	FOpenMobileAdsTrackingAuthorizationRequestNativeEvent
		NativeTrackingAuthorizationRequestCompleted;
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
	FGuid ActiveTrackingAuthorizationRequestId;
	FGuid ActiveConsentRequestId;
	FOpenMobileAdsConsentRequest ActiveConsentRequest;
	FGuid ConvenienceRewardedLoadRequestId;
	FGuid ConvenienceRewardedShowRequestId;
	FOpenMobileAdsError InitializationError;
	FOpenMobileAdsInitializationStatusSnapshot InitializationStatus;
	FOpenMobileAdsPrivacySnapshot PrivacySnapshot;
	EOpenMobileAdsTrackingAuthorizationStatus TrackingAuthorizationStatus =
		EOpenMobileAdsTrackingAuthorizationStatus::Unsupported;
	FOpenMobileAdsAppOpenPresentationState AppOpenPresentationState;
	FOpenMobileAdsConsentSignalDeliverySnapshot ConsentSignalDeliveryStatus;
	FOpenMobileAdsConsentSignals LastPropagatedConsentSignals;
	FOpenMobileAdsCanRequestAdsResult LastCanRequestAdsDecision;
	double InitializationStartedSeconds = 0.0;
	double AppOpenOpportunityStartedMonotonicSeconds = 0.0;
	double AppOpenBackgroundStartedMonotonicSeconds = 0.0;
	double AppOpenBackgroundDurationSeconds = 0.0;
	EOpenMobileAdsServiceState ServiceState = EOpenMobileAdsServiceState::Uninitialized;
	EOpenMobileAdsAppOpenOpportunity AppOpenOpportunity =
		EOpenMobileAdsAppOpenOpportunity::None;
	bool bProviderInitializationStarted = false;
	bool bFrequencyCapPersistenceWarningLogged = false;
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
	bool bAppOpenBackgroundStarted = false;
	bool bAppOpenBackgroundStartedDuringFullscreen = false;
	TAtomic<bool> bPlatformDefinitelyOffline {false};
};
