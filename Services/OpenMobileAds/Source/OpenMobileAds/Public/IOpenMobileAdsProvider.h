#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobileAdsCapabilities.h"
#include "OpenMobileAdsErrors.h"
#include "OpenMobileAdsEvents.h"
#include "OpenMobileAdsInitialization.h"
#include "OpenMobileAdsOperations.h"
#include "OpenMobileAdsPrivacy.h"
#include "OpenMobileCoreTypes.h"

DECLARE_DELEGATE(FOpenMobileRewardedAdLoadedCallback);
DECLARE_DELEGATE(FOpenMobileRewardedAdShownCallback);
DECLARE_DELEGATE_TwoParams(FOpenMobileRewardedAdEarnedCallback, int32, FString);
DECLARE_DELEGATE(FOpenMobileRewardedAdClosedCallback);
DECLARE_DELEGATE_OneParam(FOpenMobileRewardedAdFailedCallback, FOpenMobileError);

struct OPENMOBILEADS_API FOpenMobileRewardedAdCallbacks
{
	FOpenMobileRewardedAdLoadedCallback OnLoaded;
	FOpenMobileRewardedAdShownCallback OnShown;
	FOpenMobileRewardedAdEarnedCallback OnEarned;
	FOpenMobileRewardedAdClosedCallback OnClosed;
	FOpenMobileRewardedAdFailedCallback OnFailed;
};

/** Accepts provider events only while the subsystem still owns the matching request. */
class OPENMOBILEADS_API IOpenMobileAdsProviderEventSink
{
public:
	virtual ~IOpenMobileAdsProviderEventSink() = default;
	/** Queues one provider event for ordered delivery on the owning game thread. */
	virtual void Submit(FOpenMobileAdsEvent Event) = 0;
	/** Seals the sink so callbacks arriving after cancellation or teardown are ignored. */
	virtual void Invalidate() = 0;
};

/** Carries progressive provider and adapter startup state without exposing subsystem lifetime. */
class OPENMOBILEADS_API IOpenMobileAdsProviderInitializationSink
{
public:
	virtual ~IOpenMobileAdsProviderInitializationSink() = default;
	/** Replaces one component row while provider initialization is still active. */
	virtual void UpdateStatus(FOpenMobileAdsInitializationComponentStatus Status) = 0;
	/** Finishes provider initialization once, an empty error means the provider is ready. */
	virtual void Complete(FOpenMobileAdsError Error) = 0;
	/** Seals the sink before provider shutdown can race a late SDK callback. */
	virtual void Invalidate() = 0;
};

/** Owns one asynchronous consent operation and rejects callbacks after it has ended. */
class OPENMOBILEADS_API IOpenMobileAdsConsentProviderSink
{
public:
	virtual ~IOpenMobileAdsConsentProviderSink() = default;
	/** Applies one normalized consent update to the request that created this sink. */
	virtual void Complete(FOpenMobileAdsConsentStatusUpdate Update) = 0;
	/** Finishes the active consent operation with provider-specific failure context. */
	virtual void Fail(FOpenMobileAdsError Error) = 0;
	/** Seals the sink before cancellation, replacement, or subsystem teardown. */
	virtual void Invalidate() = 0;
};

/** Defines the provider contract used by independently enabled Ads SDK plugins. */
class OPENMOBILEADS_API IOpenMobileAdsProvider : public IModularFeature
{
public:
	virtual ~IOpenMobileAdsProvider() = default;

	/** Uses one stable modular feature key so the service can discover enabled providers. */
	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("OpenMobile.Ads.Provider"));
		return FeatureName;
	}

	/** Returns the stable provider name used by settings, events, and diagnostics. */
	virtual FName GetProviderName() const = 0;
	/** Breaks ties only when no preferred provider was configured. */
	virtual int32 GetPriority() const { return 0; }
	/** Reports whether this build and runtime platform can use the provider SDK. */
	virtual bool IsSupported() const = 0;
	/** Describes format and mediation support without loading an ad as a probe. */
	virtual FOpenMobileAdsProviderCapabilities GetCapabilities() const;
	/** Applies provider-specific privacy gating to an already normalized request context. */
	virtual FOpenMobileAdsProviderRequestPolicy GetRequestPolicy(
		const FOpenMobileAdsProviderRequestContext& Context
	) const;
	/** Names a separate consent SDK when the Ads provider doesn't own consent itself. */
	virtual FName GetConsentProviderName() const { return NAME_None; }
	/** Reports form support separately from general consent refresh support. */
	virtual bool SupportsPrivacyOptionsForm() const { return false; }
	/** Keeps destructive consent reset hidden unless the provider explicitly supports test cleanup. */
	virtual bool SupportsConsentResetForTesting() const { return false; }
	/** Clears provider-stored consent only for the service's development-only reset path. */
	virtual bool ResetConsentForTesting(FOpenMobileAdsError& OutError);
	/** Declares privacy inputs understood directly by the provider SDK. */
	virtual int32 GetSupportedConsentSignalMask() const { return 0; }
	/** Declares which provider privacy inputs can be confirmed after application. */
	virtual int32 GetConfirmableConsentSignalMask() const { return 0; }
	/** Declares which provider privacy inputs can change after initialization. */
	virtual int32 GetRuntimeUpdatableConsentSignalMask() const { return 0; }
	/** Applies the selected privacy bits and reports exactly which ones were accepted and confirmed. */
	virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask
	);
	/** Refreshes stored consent state without assuming a form will be needed. */
	virtual bool RefreshConsent(
		const FOpenMobileAdsConsentRequest& Request,
		TSharedRef<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> CompletionSink,
		FOpenMobileAdsError& OutError
	);
	/** Presents the provider's required consent form for the active request only. */
	virtual bool PresentRequiredConsentForm(
		const FOpenMobileAdsConsentRequest& Request,
		TSharedRef<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> CompletionSink,
		FOpenMobileAdsError& OutError
	);
	/** Presents the provider's user-invoked privacy choices form when available. */
	virtual bool PresentPrivacyOptionsForm(
		const FOpenMobileAdsConsentRequest& Request,
		TSharedRef<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> CompletionSink,
		FOpenMobileAdsError& OutError
	);
	/** Cancels only the provider consent operation with the supplied request identity. */
	virtual void CancelConsent(FGuid RequestId) {}

	/** Starts provider setup after every required participant and privacy gate has passed. */
	virtual bool Initialize(
		const FOpenMobileAdsInitializationRequest& Request,
		TSharedRef<IOpenMobileAdsProviderInitializationSink, ESPMode::ThreadSafe> CompletionSink,
		FOpenMobileAdsError& OutError
	);

	/** Stops SDK work and disconnects callbacks before the provider module unloads. */
	virtual void Shutdown() {}

	/** Starts one load and uses the supplied sink for every event tied to its request. */
	virtual bool Load(
		const FOpenMobileAdsLoadRequest& Request,
		TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
		FOpenMobileAdsError& OutError
	);

	/** Presents one cached ad identity without silently loading a replacement. */
	virtual bool Show(
		const FOpenMobileAdsShowRequest& Request,
		TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
		FOpenMobileAdsError& OutError
	);

	/** Hides one visible provider object only for formats that advertise hide support. */
	virtual bool Hide(
		const FOpenMobileAdsHideRequest& Request,
		TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
		FOpenMobileAdsError& OutError
	);

	/** Releases one or all placements according to the explicit destroy request. */
	virtual bool Destroy(
		const FOpenMobileAdsDestroyRequest& Request,
		TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
		FOpenMobileAdsError& OutError
	);

	/** Cancels pending SDK work for one request while keeping cached ads separately owned. */
	virtual void Cancel(FGuid RequestId) {}
	/** Releases one provider cache entry after the service has dropped its identity. */
	virtual void ReleaseCachedAd(FGuid CachedAdId) = 0;

	/** Keeps the older rewarded callback flow available through the selected provider. */
	virtual bool RequestAndShowRewardedAd(
		FOpenMobileRewardedAdCallbacks&& Callbacks,
		FOpenMobileError& OutError
	) = 0;
};

struct OPENMOBILEADS_API FOpenMobileAdsProviderSelection
{
	IOpenMobileAdsProvider* Provider = nullptr;
	FOpenMobileAdsError Error;
};

class OPENMOBILEADS_API FOpenMobileAdsProviderResolver
{
public:
	/** Selects one supported provider deterministically or returns a typed conflict or availability error. */
	static FOpenMobileAdsProviderSelection Resolve(
		const TArray<IOpenMobileAdsProvider*>& Providers,
		FName PreferredProvider
	);
};
