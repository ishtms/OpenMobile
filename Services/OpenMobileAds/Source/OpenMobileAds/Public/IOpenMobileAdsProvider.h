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

class OPENMOBILEADS_API IOpenMobileAdsProviderEventSink
{
public:
	virtual ~IOpenMobileAdsProviderEventSink() = default;
	virtual void Submit(FOpenMobileAdsEvent Event) = 0;
	virtual void Invalidate() = 0;
};

class OPENMOBILEADS_API IOpenMobileAdsProviderInitializationSink
{
public:
	virtual ~IOpenMobileAdsProviderInitializationSink() = default;
	virtual void UpdateStatus(FOpenMobileAdsInitializationComponentStatus Status) = 0;
	virtual void Complete(FOpenMobileAdsError Error) = 0;
	virtual void Invalidate() = 0;
};

class OPENMOBILEADS_API IOpenMobileAdsConsentProviderSink
{
public:
	virtual ~IOpenMobileAdsConsentProviderSink() = default;
	virtual void Complete(FOpenMobileAdsConsentStatusUpdate Update) = 0;
	virtual void Fail(FOpenMobileAdsError Error) = 0;
	virtual void Invalidate() = 0;
};

/** Public, versioned SPI implemented by independently enabled ad-provider plugins. */
class OPENMOBILEADS_API IOpenMobileAdsProvider : public IModularFeature
{
public:
	virtual ~IOpenMobileAdsProvider() = default;

	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("OpenMobile.Ads.Provider"));
		return FeatureName;
	}

	virtual FName GetProviderName() const = 0;
	virtual int32 GetPriority() const { return 0; }
	virtual bool IsSupported() const = 0;
	virtual FOpenMobileAdsProviderCapabilities GetCapabilities() const;
	virtual FOpenMobileAdsProviderRequestPolicy GetRequestPolicy(
		const FOpenMobileAdsProviderRequestContext& Context
	) const;
	virtual FName GetConsentProviderName() const { return NAME_None; }
	virtual bool RefreshConsent(
		const FOpenMobileAdsConsentRequest& Request,
		TSharedRef<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> CompletionSink,
		FOpenMobileAdsError& OutError
	);
	virtual bool PresentRequiredConsentForm(
		const FOpenMobileAdsConsentRequest& Request,
		TSharedRef<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> CompletionSink,
		FOpenMobileAdsError& OutError
	);
	virtual void CancelConsent(FGuid RequestId) {}

	virtual bool Initialize(
		const FOpenMobileAdsInitializationRequest& Request,
		TSharedRef<IOpenMobileAdsProviderInitializationSink, ESPMode::ThreadSafe> CompletionSink,
		FOpenMobileAdsError& OutError
	);

	virtual void Shutdown() {}

	virtual bool Load(
		const FOpenMobileAdsLoadRequest& Request,
		TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
		FOpenMobileAdsError& OutError
	);

	virtual bool Show(
		const FOpenMobileAdsShowRequest& Request,
		TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
		FOpenMobileAdsError& OutError
	);

	virtual bool Destroy(
		const FOpenMobileAdsDestroyRequest& Request,
		TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
		FOpenMobileAdsError& OutError
	);

	virtual void Cancel(FGuid RequestId) {}
	virtual void ReleaseCachedAd(FGuid CachedAdId) = 0;

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
	static FOpenMobileAdsProviderSelection Resolve(
		const TArray<IOpenMobileAdsProvider*>& Providers,
		FName PreferredProvider
	);
};
