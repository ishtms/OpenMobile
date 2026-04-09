#include "OpenMobileAdsAdMobPlatform.h"

#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsAdMobBackend.h"
#include "OpenMobileAsync.h"

namespace OpenMobileAdsAdMobPlatformPrivate
{
	FOnOpenMobileAdMobRewardedLoaded LoadedDelegate;
	FOnOpenMobileAdMobRewardedShown ShownDelegate;
	FOnOpenMobileAdMobRewardedEarned EarnedDelegate;
	FOnOpenMobileAdMobRewardedClosed ClosedDelegate;
	FOnOpenMobileAdMobRewardedFailed FailedDelegate;
	int64 NextRequestId = 0;
	int64 ActiveRequestId = 0;
	bool bRequestInProgress = false;
	bool bRewardDispatched = false;
	bool bInitialized = false;

	void ResetRequest()
	{
		LoadedDelegate.Unbind();
		ShownDelegate.Unbind();
		EarnedDelegate.Unbind();
		ClosedDelegate.Unbind();
		FailedDelegate.Unbind();
		ActiveRequestId = 0;
		bRequestInProgress = false;
		bRewardDispatched = false;
	}

	IOpenMobileAdsAdMobBackend* FindBackend()
	{
		TArray<IOpenMobileAdsAdMobBackend*> Backends =
			IModularFeatures::Get().GetModularFeatureImplementations<IOpenMobileAdsAdMobBackend>(
				IOpenMobileAdsAdMobBackend::GetModularFeatureName()
			);
		for (IOpenMobileAdsAdMobBackend* Backend : Backends)
		{
			if (Backend && Backend->IsAvailable())
			{
				return Backend;
			}
		}
		return nullptr;
	}
}

bool FOpenMobileAdsAdMobPlatform::IsSupported()
{
	return OpenMobileAdsAdMobPlatformPrivate::FindBackend() != nullptr;
}

void FOpenMobileAdsAdMobPlatform::Shutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileAdsAdMobPlatformPrivate;
	ResetRequest();
	if (bInitialized)
	{
		if (IOpenMobileAdsAdMobBackend* Backend = FindBackend())
		{
			Backend->Shutdown();
		}
		bInitialized = false;
	}
}

bool FOpenMobileAdsAdMobPlatform::BeginRequest(
	const FString& AdUnitId,
	FOnOpenMobileAdMobRewardedLoaded&& OnLoaded,
	FOnOpenMobileAdMobRewardedShown&& OnShown,
	FOnOpenMobileAdMobRewardedEarned&& OnEarned,
	FOnOpenMobileAdMobRewardedClosed&& OnClosed,
	FOnOpenMobileAdMobRewardedFailed&& OnFailed,
	FString& OutError
)
{
	check(IsInGameThread());
	using namespace OpenMobileAdsAdMobPlatformPrivate;

	IOpenMobileAdsAdMobBackend* Backend = FindBackend();
	if (!Backend)
	{
		OutError = TEXT("The AdMob provider has no native backend for this platform.");
		return false;
	}
	if (bRequestInProgress)
	{
		OutError = TEXT("An AdMob rewarded-ad request is already in progress.");
		return false;
	}
	if (AdUnitId.IsEmpty())
	{
		OutError = TEXT("The AdMob rewarded-ad unit ID is empty.");
		return false;
	}

	if (!bInitialized)
	{
		Backend->Initialize();
		bInitialized = true;
	}

	LoadedDelegate = MoveTemp(OnLoaded);
	ShownDelegate = MoveTemp(OnShown);
	EarnedDelegate = MoveTemp(OnEarned);
	ClosedDelegate = MoveTemp(OnClosed);
	FailedDelegate = MoveTemp(OnFailed);
	bRequestInProgress = true;
	bRewardDispatched = false;

	++NextRequestId;
	if (NextRequestId <= 0)
	{
		NextRequestId = 1;
	}
	ActiveRequestId = NextRequestId;

	if (!Backend->LaunchRewardedAd(AdUnitId, ActiveRequestId, OutError))
	{
		ResetRequest();
		return false;
	}

	return true;
}

void FOpenMobileAdsAdMobPlatform::NativeLoaded(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		if (bRequestInProgress && ActiveRequestId == RequestId)
		{
			LoadedDelegate.ExecuteIfBound();
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeShown(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		if (bRequestInProgress && ActiveRequestId == RequestId)
		{
			ShownDelegate.ExecuteIfBound();
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeEarned(
	int64 RequestId,
	int32 NetworkAmount,
	FString NetworkRewardType
)
{
	OpenMobile::DispatchToGameThread(
		[RequestId, NetworkAmount, NetworkRewardType = MoveTemp(NetworkRewardType)]() mutable
		{
			using namespace OpenMobileAdsAdMobPlatformPrivate;
			if (!bRequestInProgress || ActiveRequestId != RequestId || bRewardDispatched)
			{
				return;
			}
			bRewardDispatched = true;
			EarnedDelegate.ExecuteIfBound(NetworkAmount, MoveTemp(NetworkRewardType));
		}
	);
}

void FOpenMobileAdsAdMobPlatform::NativeClosed(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		if (!bRequestInProgress || ActiveRequestId != RequestId)
		{
			return;
		}
		FOnOpenMobileAdMobRewardedClosed Completion = MoveTemp(ClosedDelegate);
		ResetRequest();
		Completion.ExecuteIfBound();
	});
}

void FOpenMobileAdsAdMobPlatform::NativeFailed(int64 RequestId, FString ErrorMessage)
{
	OpenMobile::DispatchToGameThread([RequestId, ErrorMessage = MoveTemp(ErrorMessage)]() mutable
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		if (!bRequestInProgress || ActiveRequestId != RequestId)
		{
			return;
		}
		FOnOpenMobileAdMobRewardedFailed Completion = MoveTemp(FailedDelegate);
		ResetRequest();
		Completion.ExecuteIfBound(MoveTemp(ErrorMessage));
	});
}
