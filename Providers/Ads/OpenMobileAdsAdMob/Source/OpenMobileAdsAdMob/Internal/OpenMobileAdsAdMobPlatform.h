#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsErrors.h"
#include "OpenMobileAdsInitialization.h"
#include "OpenMobileAdsOperations.h"

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
DECLARE_DELEGATE_OneParam(FOnOpenMobileAdMobRewardedCached, FGuid);
DECLARE_DELEGATE(FOnOpenMobileAdMobRewardedShown);
DECLARE_DELEGATE_TwoParams(FOnOpenMobileAdMobRewardedEarned, int32, FString);
DECLARE_DELEGATE(FOnOpenMobileAdMobRewardedClosed);
DECLARE_DELEGATE_OneParam(FOnOpenMobileAdMobRewardedFailed, FString);

class OPENMOBILEADSADMOB_API FOpenMobileAdsAdMobPlatform
{
public:
	static bool IsSupported();
	static bool Initialize(
		const FOpenMobileAdsInitializationRequest& Request,
		FOnOpenMobileAdMobInitializationStatus&& OnStatus,
		FOnOpenMobileAdMobInitialized&& OnCompleted,
		FString& OutError
	);
	static void Shutdown();
	static bool BeginConsentRefresh(
		const FOpenMobileAdsConsentRequest& Request,
		FOnOpenMobileAdMobConsentCompleted&& OnCompleted,
		FOnOpenMobileAdMobConsentFailed&& OnFailed,
		FString& OutError
	);
	static bool BeginRequiredConsentForm(
		const FOpenMobileAdsConsentRequest& Request,
		FOnOpenMobileAdMobConsentCompleted&& OnCompleted,
		FOnOpenMobileAdMobConsentFailed&& OnFailed,
		FString& OutError
	);
	static bool BeginPrivacyOptionsForm(
		const FOpenMobileAdsConsentRequest& Request,
		FOnOpenMobileAdMobConsentCompleted&& OnCompleted,
		FOnOpenMobileAdMobConsentFailed&& OnFailed,
		FString& OutError
	);
	static bool ApplyConsentSignals(
		const FOpenMobileAdsConsentSignals& Signals,
		int32 SignalMask,
		FString& OutError
	);
	static void CancelConsent(FGuid RequestId);
	static bool BeginLoad(
		const FOpenMobileAdsLoadRequest& Request,
		FOnOpenMobileAdMobRewardedCached&& OnLoaded,
		FOnOpenMobileAdMobRewardedFailed&& OnFailed,
		FString& OutError
	);
	static bool BeginShow(
		const FOpenMobileAdsShowRequest& Request,
		TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
		FString& OutError
	);
	static void Cancel(FGuid RequestId);
	static void ReleaseCachedAd(FGuid CachedAdId);

	static bool BeginRequest(
		const FString& AdUnitId,
		FOnOpenMobileAdMobRewardedLoaded&& OnLoaded,
		FOnOpenMobileAdMobRewardedShown&& OnShown,
		FOnOpenMobileAdMobRewardedEarned&& OnEarned,
		FOnOpenMobileAdMobRewardedClosed&& OnClosed,
		FOnOpenMobileAdMobRewardedFailed&& OnFailed,
		FString& OutError
	);

	static void NativeInitializationCompleted(int64 RequestId);
	static void NativeInitializationFailed(int64 RequestId, FString ErrorMessage);
	static void NativeConsentInfoUpdated(
		int64 RequestId,
		int32 ConsentStatus,
		bool bCanRequestAds,
		int32 PrivacyOptionsRequirement
	);
	static void NativeConsentFormDismissed(
		int64 RequestId,
		int32 ConsentStatus,
		bool bCanRequestAds,
		int32 PrivacyOptionsRequirement
	);
	static void NativeConsentFailed(
		int64 RequestId,
		FString ErrorCode,
		FString ErrorMessage
	);
	static void NativeAdapterInitializationStatus(
		int64 RequestId,
		FString AdapterName,
		bool bReady,
		double LatencyMilliseconds,
		FString Description
	);
	static void NativeRewardedLoadCompleted(int64 RequestId);
	static void NativeRewardedLoadFailed(int64 RequestId, FString ErrorMessage);
	static void NativeLoaded(int64 RequestId);
	static void NativeShown(int64 RequestId);
	static void NativeImpression(int64 RequestId);
	static void NativeClicked(int64 RequestId);
	static void NativeRevenuePaid(
		int64 RequestId,
		int64 ValueMicros,
		FString CurrencyCode,
		int32 Precision
	);
	static void NativeEarned(int64 RequestId, int32 NetworkAmount, FString NetworkRewardType);
	static void NativeClosed(int64 RequestId);
	static void NativeFailed(int64 RequestId, FString ErrorMessage);
};
