#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsErrors.h"
#include "OpenMobileAdsInitialization.h"
#include "OpenMobileAdsOperations.h"

DECLARE_DELEGATE_OneParam(
	FOnOpenMobileAdMobInitializationStatus,
	const FOpenMobileAdsInitializationComponentStatus&
);
DECLARE_DELEGATE_OneParam(FOnOpenMobileAdMobInitialized, FOpenMobileAdsError);
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
	static bool BeginLoad(
		const FOpenMobileAdsLoadRequest& Request,
		FOnOpenMobileAdMobRewardedCached&& OnLoaded,
		FOnOpenMobileAdMobRewardedFailed&& OnFailed,
		FString& OutError
	);
	static void CancelLoad(FGuid RequestId);
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
	static void NativeEarned(int64 RequestId, int32 NetworkAmount, FString NetworkRewardType);
	static void NativeClosed(int64 RequestId);
	static void NativeFailed(int64 RequestId, FString ErrorMessage);
};
