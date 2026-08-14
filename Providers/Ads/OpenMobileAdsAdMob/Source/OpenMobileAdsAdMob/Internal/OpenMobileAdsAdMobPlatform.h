#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsErrors.h"
#include "OpenMobileAdsInitialization.h"
#include "OpenMobileAdsOperations.h"
#include "OpenMobileAdsRevenue.h"

class IOpenMobileAdsProviderEventSink;

DECLARE_DELEGATE_OneParam(
	FOnOpenMobileAdMobInitializationStatus,
	const FOpenMobileAdsInitializationComponentStatus&
);
DECLARE_DELEGATE_OneParam(FOnOpenMobileAdMobInitialized, FOpenMobileAdsError);
DECLARE_DELEGATE_OneParam(
	FOnOpenMobileAdMobConsentCompleted,
	FOpenMobileAdsConsentStatusUpdate
);
DECLARE_DELEGATE_OneParam(
	FOnOpenMobileAdMobConsentFailed,
	FOpenMobileAdsError
);
DECLARE_DELEGATE(FOnOpenMobileAdMobRewardedLoaded);
DECLARE_DELEGATE_ThreeParams(
	FOnOpenMobileAdMobAdCached,
	FGuid,
	int64,
	FString
);
DECLARE_DELEGATE_TwoParams(FOnOpenMobileAdMobAdLoadFailed, FString, FString);
DECLARE_DELEGATE(FOnOpenMobileAdMobRewardedShown);
DECLARE_DELEGATE_TwoParams(FOnOpenMobileAdMobRewardedEarned, int32, FString);
DECLARE_DELEGATE(FOnOpenMobileAdMobRewardedClosed);
DECLARE_DELEGATE_OneParam(FOnOpenMobileAdMobRewardedFailed, FString);

class OPENMOBILEADSADMOB_API FOpenMobileAdsAdMobPlatform
{
public:
	/** Reports whether an active AdMob backend can serve this runtime platform. */
	static bool IsSupported();
	/** Owns one provider initialization callback set till the native backend finishes. */
	static bool Initialize(
		const FOpenMobileAdsInitializationRequest& Request,
		FOnOpenMobileAdMobInitializationStatus&& OnStatus,
		FOnOpenMobileAdMobInitialized&& OnCompleted,
		FString& OutError
	);
	/** Seals provider callbacks and asks the active backend to release native state. */
	static void Shutdown();
	/** Refreshes UMP state and maps its native result to the provider-neutral consent contract. */
	static bool BeginConsentRefresh(
		const FOpenMobileAdsConsentRequest& Request,
		FOnOpenMobileAdMobConsentCompleted&& OnCompleted,
		FOnOpenMobileAdMobConsentFailed&& OnFailed,
		FString& OutError
	);
	/** Presents a required UMP form for the consent request that just refreshed. */
	static bool BeginRequiredConsentForm(
		const FOpenMobileAdsConsentRequest& Request,
		FOnOpenMobileAdMobConsentCompleted&& OnCompleted,
		FOnOpenMobileAdMobConsentFailed&& OnFailed,
		FString& OutError
	);
	/** Opens the UMP privacy options form for a user-invoked request. */
	static bool BeginPrivacyOptionsForm(
		const FOpenMobileAdsConsentRequest& Request,
		FOnOpenMobileAdMobConsentCompleted&& OnCompleted,
		FOnOpenMobileAdMobConsentFailed&& OnFailed,
		FString& OutError
	);
	/** Clears UMP development state without affecting production builds. */
	static bool ResetConsentForTesting(FString& OutError);
	/** Forwards selected privacy bits to the backend before new requests use them. */
	static bool ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask,
		FString& OutError
	);
	/** Cancels one active UMP operation by its public request identity. */
	static void CancelConsent(FGuid RequestId);
	/** Resolves the format to one backend load call and retains the resulting native cache identity. */
	static bool BeginLoad(
		const FOpenMobileAdsLoadRequest& Request,
		FOnOpenMobileAdMobAdCached&& OnLoaded,
		FOnOpenMobileAdMobAdLoadFailed&& OnFailed,
		FString& OutError
	);
	/** Resolves one cached identity and forwards provider events through its request sink. */
	static bool BeginShow(
		const FOpenMobileAdsShowRequest& Request,
		TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
		FString& OutError
	);
	/** Hides only persistent display formats whose loaded cache identity is still owned. */
	static bool BeginHide(
		const FOpenMobileAdsHideRequest& Request,
		TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
		FString& OutError
	);
	/** Cancels native work tied to one public request without releasing unrelated cached ads. */
	static void Cancel(FGuid RequestId);
	/** Releases the backend object owned by one service cache identity. */
	static void ReleaseCachedAd(FGuid CachedAdId);
	/** Maps Google revenue precision to the stable provider-neutral enum. */
	static EOpenMobileAdsRevenuePrecision MapRevenuePrecision(
		int32 ProviderPrecision
	);

	/** Keeps the legacy rewarded callback flow on top of the same platform state. */
	static bool BeginRequest(
		const FString& AdUnitId,
		FOnOpenMobileAdMobRewardedLoaded&& OnLoaded,
		FOnOpenMobileAdMobRewardedShown&& OnShown,
		FOnOpenMobileAdMobRewardedEarned&& OnEarned,
		FOnOpenMobileAdMobRewardedClosed&& OnClosed,
		FOnOpenMobileAdMobRewardedFailed&& OnFailed,
		FString& OutError
	);

	/** Completes only the matching native initialization generation. */
	static void NativeInitializationCompleted(int64 RequestId);
	/** Fails only the matching native initialization generation with sanitized context. */
	static void NativeInitializationFailed(int64 RequestId, FString ErrorMessage);
	/** Maps refreshed UMP values for the active consent information request. */
	static void NativeConsentInfoUpdated(
		int64 RequestId,
		int32 ConsentStatus,
		bool bCanRequestAds,
		int32 PrivacyOptionsRequirement
	);
	/** Maps UMP values after the active consent form has been dismissed. */
	static void NativeConsentFormDismissed(
		int64 RequestId,
		int32 ConsentStatus,
		bool bCanRequestAds,
		int32 PrivacyOptionsRequirement
	);
	/** Routes one UMP failure to the active consent request and ignores stale identities. */
	static void NativeConsentFailed(
		int64 RequestId,
		FString ErrorCode,
		FString ErrorMessage
	);
	/** Merges Google adapter startup status into provider initialization diagnostics. */
	static void NativeAdapterInitializationStatus(
		int64 RequestId,
		FString AdapterName,
		bool bReady,
		double LatencyMilliseconds,
		FString Description
	);
	/** Accepts a rewarded load only when its native identity still owns the request. */
	static void NativeRewardedLoadCompleted(int64 RequestId);
	/** Fails the matching rewarded load with its native Google code when supplied. */
	static void NativeRewardedLoadFailed(
		int64 RequestId,
		FString ErrorMessage,
		FString ErrorCode = FString()
	);
	/** Caches one completed interstitial under the matching load identity. */
	static void NativeInterstitialLoadCompleted(int64 RequestId);
	/** Fails the matching interstitial load without disturbing other format state. */
	static void NativeInterstitialLoadFailed(
		int64 RequestId,
		FString ErrorMessage,
		FString ErrorCode = FString()
	);
	/** Caches rewarded-interstitial metadata alongside the matching native object. */
	static void NativeRewardedInterstitialLoadCompleted(
		int64 RequestId,
		int64 RewardAmount,
		FString RewardType
	);
	/** Fails the matching rewarded-interstitial load with native diagnostics. */
	static void NativeRewardedInterstitialLoadFailed(
		int64 RequestId,
		FString ErrorMessage,
		FString ErrorCode = FString()
	);
	/** Caches one App Open object under the matching load identity. */
	static void NativeAppOpenLoadCompleted(int64 RequestId);
	/** Fails only the matching App Open load identity. */
	static void NativeAppOpenLoadFailed(
		int64 RequestId,
		FString ErrorMessage,
		FString ErrorCode = FString()
	);
	/** Caches one banner object before any view is attached to the viewport. */
	static void NativeBannerLoadCompleted(int64 RequestId);
	/** Fails only the matching banner load identity. */
	static void NativeBannerLoadFailed(
		int64 RequestId,
		FString ErrorMessage,
		FString ErrorCode = FString()
	);
	/** Marks the matching banner show visible after native view attachment succeeds. */
	static void NativeBannerShown(int64 RequestId);
	/** Marks the matching banner hidden while leaving its loaded object reusable. */
	static void NativeBannerHidden(int64 RequestId);
	/** Routes banner show or hide failure through the currently active operation identity. */
	static void NativeBannerOperationFailed(int64 RequestId, FString ErrorMessage);
	/** Finishes the legacy rewarded load callback for its active native identity. */
	static void NativeLoaded(int64 RequestId);
	/** Reports legacy rewarded presentation only after the native fullscreen callback arrives. */
	static void NativeShown(int64 RequestId);
	/** Emits one impression for the active show before revenue updates are accepted. */
	static void NativeImpression(int64 RequestId);
	/** Emits one click without treating it as show completion. */
	static void NativeClicked(int64 RequestId);
	/** Normalizes and routes paid-event data for the active cached ad. */
	static void NativeRevenuePaid(
		int64 RequestId,
		int64 ValueMicros,
		FString CurrencyCode,
		int32 Precision,
		FOpenMobileAdsRevenueSource Source = {}
	);
	/** Normalizes one local reward while leaving backend verification to the server callback. */
	static void NativeEarned(int64 RequestId, int32 NetworkAmount, FString NetworkRewardType);
	/** Ends the matching fullscreen show and releases its one-use native object. */
	static void NativeClosed(int64 RequestId);
	/** Fails the matching active operation and keeps stale native callbacks silent. */
	static void NativeFailed(int64 RequestId, FString ErrorMessage);
};
