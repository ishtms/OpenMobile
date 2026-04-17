#include "OpenMobileAdsAdMobPlatform.h"

#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsAdMobBackend.h"
#include "OpenMobileAsync.h"

namespace OpenMobileAdsAdMobPlatformPrivate
{
	struct FRewardedLoadOperation
	{
		FGuid RequestId;
		FName Placement;
		FOnOpenMobileAdMobRewardedLoaded Loaded;
		FOnOpenMobileAdMobRewardedFailed Failed;
	};

	TArray<FOnOpenMobileAdMobInitialized> InitializationDelegates;
	TArray<FOnOpenMobileAdMobInitializationStatus> InitializationStatusDelegates;
	TArray<FOpenMobileAdsInitializationComponentStatus> InitializationStatuses;
	FOnOpenMobileAdMobRewardedLoaded LoadedDelegate;
	FOnOpenMobileAdMobRewardedShown ShownDelegate;
	FOnOpenMobileAdMobRewardedEarned EarnedDelegate;
	FOnOpenMobileAdMobRewardedClosed ClosedDelegate;
	FOnOpenMobileAdMobRewardedFailed FailedDelegate;
	TMap<int64, FRewardedLoadOperation> RewardedLoadOperations;
	TMap<FGuid, int64> NativeLoadRequestIds;
	TMap<FName, int64> LoadedRewardedAdRequestIds;
	int64 NextRequestId = 0;
	int64 ActiveInitializationRequestId = 0;
	int64 ActiveRequestId = 0;
	bool bInitializationInProgress = false;
	bool bRequestInProgress = false;
	bool bRewardDispatched = false;
	bool bInitialized = false;

	void ResetInitialization()
	{
		InitializationDelegates.Reset();
		InitializationStatusDelegates.Reset();
		InitializationStatuses.Reset();
		ActiveInitializationRequestId = 0;
		bInitializationInProgress = false;
	}

	void BroadcastInitializationStatus(
		FOpenMobileAdsInitializationComponentStatus Status
	)
	{
		const int32 ExistingIndex = InitializationStatuses.IndexOfByPredicate(
			[&Status](const FOpenMobileAdsInitializationComponentStatus& Candidate)
			{
				return Candidate.Type == Status.Type
					&& Candidate.Name == Status.Name
					&& Candidate.Parent == Status.Parent;
			}
		);
		const int32 StatusIndex = ExistingIndex == INDEX_NONE
			? InitializationStatuses.Add(MoveTemp(Status))
			: ExistingIndex;
		if (ExistingIndex != INDEX_NONE)
		{
			InitializationStatuses[ExistingIndex] = MoveTemp(Status);
		}

		const FOpenMobileAdsInitializationComponentStatus& CurrentStatus =
			InitializationStatuses[StatusIndex];
		for (FOnOpenMobileAdMobInitializationStatus& Delegate : InitializationStatusDelegates)
		{
			Delegate.ExecuteIfBound(CurrentStatus);
		}
	}

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

	void ResetLoads()
	{
		RewardedLoadOperations.Reset();
		NativeLoadRequestIds.Reset();
		LoadedRewardedAdRequestIds.Reset();
	}

	bool RemoveLoadOperation(
		int64 NativeRequestId,
		FRewardedLoadOperation& OutOperation
	)
	{
		if (!RewardedLoadOperations.RemoveAndCopyValue(NativeRequestId, OutOperation))
		{
			return false;
		}
		NativeLoadRequestIds.Remove(OutOperation.RequestId);
		return true;
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

bool FOpenMobileAdsAdMobPlatform::Initialize(
	const FOpenMobileAdsInitializationRequest& Request,
	FOnOpenMobileAdMobInitializationStatus&& OnStatus,
	FOnOpenMobileAdMobInitialized&& OnCompleted,
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
	if (bInitialized)
	{
		for (const FOpenMobileAdsInitializationComponentStatus& Status : InitializationStatuses)
		{
			OnStatus.ExecuteIfBound(Status);
		}
		InitializationStatusDelegates.Add(MoveTemp(OnStatus));
		OnCompleted.ExecuteIfBound(FOpenMobileAdsError());
		return true;
	}

	InitializationStatusDelegates.Add(MoveTemp(OnStatus));
	InitializationDelegates.Add(MoveTemp(OnCompleted));
	if (bInitializationInProgress)
	{
		return true;
	}

	++NextRequestId;
	if (NextRequestId <= 0)
	{
		NextRequestId = 1;
	}
	ActiveInitializationRequestId = NextRequestId;
	bInitializationInProgress = true;
	InitializationStatuses.Reset();
	if (!Backend->Initialize(Request, ActiveInitializationRequestId, OutError))
	{
		ResetInitialization();
		return false;
	}
	return true;
}

void FOpenMobileAdsAdMobPlatform::Shutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileAdsAdMobPlatformPrivate;
	ResetRequest();
	ResetLoads();
	if (bInitialized || bInitializationInProgress)
	{
		if (IOpenMobileAdsAdMobBackend* Backend = FindBackend())
		{
			Backend->Shutdown();
		}
		bInitialized = false;
	}
	ResetInitialization();
}

bool FOpenMobileAdsAdMobPlatform::BeginLoad(
	const FOpenMobileAdsLoadRequest& Request,
	FOnOpenMobileAdMobRewardedLoaded&& OnLoaded,
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
	if (!bInitialized)
	{
		OutError = TEXT("The AdMob SDK has not finished initializing.");
		return false;
	}
	if (!Request.RequestId.IsValid())
	{
		OutError = TEXT("The AdMob rewarded load request ID is invalid.");
		return false;
	}
	if (Request.Placement.AdUnitId.IsEmpty())
	{
		OutError = TEXT("The AdMob rewarded-ad unit ID is empty.");
		return false;
	}
	if (NativeLoadRequestIds.Contains(Request.RequestId))
	{
		OutError = TEXT("The AdMob rewarded load request is already active.");
		return false;
	}

	++NextRequestId;
	if (NextRequestId <= 0)
	{
		NextRequestId = 1;
	}
	const int64 NativeRequestId = NextRequestId;
	FRewardedLoadOperation Operation;
	Operation.RequestId = Request.RequestId;
	Operation.Placement = Request.Placement.Placement;
	Operation.Loaded = MoveTemp(OnLoaded);
	Operation.Failed = MoveTemp(OnFailed);
	RewardedLoadOperations.Add(NativeRequestId, MoveTemp(Operation));
	NativeLoadRequestIds.Add(Request.RequestId, NativeRequestId);

	if (!Backend->LoadRewardedAd(
		Request.Placement.AdUnitId,
		NativeRequestId,
		OutError
	))
	{
		FRewardedLoadOperation Removed;
		RemoveLoadOperation(NativeRequestId, Removed);
		return false;
	}
	return true;
}

void FOpenMobileAdsAdMobPlatform::CancelLoad(FGuid RequestId)
{
	check(IsInGameThread());
	using namespace OpenMobileAdsAdMobPlatformPrivate;
	int64 NativeRequestId = 0;
	if (!NativeLoadRequestIds.RemoveAndCopyValue(RequestId, NativeRequestId))
	{
		return;
	}
	RewardedLoadOperations.Remove(NativeRequestId);
	if (IOpenMobileAdsAdMobBackend* Backend = FindBackend())
	{
		Backend->CancelRewardedAd(NativeRequestId);
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
		OutError = TEXT("The AdMob SDK has not finished initializing.");
		return false;
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

void FOpenMobileAdsAdMobPlatform::NativeInitializationCompleted(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		if (!bInitializationInProgress || ActiveInitializationRequestId != RequestId)
		{
			return;
		}
		TArray<FOnOpenMobileAdMobInitialized> Completions = MoveTemp(InitializationDelegates);
		bInitializationInProgress = false;
		bInitialized = true;
		for (FOnOpenMobileAdMobInitialized& Completion : Completions)
		{
			Completion.ExecuteIfBound(FOpenMobileAdsError());
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeInitializationFailed(
	int64 RequestId,
	FString ErrorMessage
)
{
	OpenMobile::DispatchToGameThread([RequestId, ErrorMessage = MoveTemp(ErrorMessage)]() mutable
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		if (!bInitializationInProgress || ActiveInitializationRequestId != RequestId)
		{
			return;
		}
		TArray<FOnOpenMobileAdMobInitialized> Completions = MoveTemp(InitializationDelegates);
		InitializationStatusDelegates.Reset();
		InitializationStatuses.Reset();
		ActiveInitializationRequestId = 0;
		bInitializationInProgress = false;
		bInitialized = false;
		for (FOnOpenMobileAdMobInitialized& Completion : Completions)
		{
			Completion.ExecuteIfBound(FOpenMobileAdsError::Make(
				EOpenMobileAdsErrorCode::NativeFailure,
				EOpenMobileAdsFailureStage::Initialization,
				NAME_None,
				ErrorMessage.IsEmpty()
					? TEXT("The AdMob SDK failed to initialize.")
					: ErrorMessage,
				TEXT("AdMob")
			));
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeAdapterInitializationStatus(
	int64 RequestId,
	FString AdapterName,
	bool bReady,
	double LatencyMilliseconds,
	FString Description
)
{
	OpenMobile::DispatchToGameThread(
		[
			RequestId,
			AdapterName = MoveTemp(AdapterName),
			bReady,
			LatencyMilliseconds,
			Description = MoveTemp(Description)
		]() mutable
		{
			using namespace OpenMobileAdsAdMobPlatformPrivate;
			if (
				ActiveInitializationRequestId != RequestId
				|| (!bInitializationInProgress && !bInitialized)
				|| AdapterName.IsEmpty()
			)
			{
				return;
			}

			FOpenMobileAdsInitializationComponentStatus Status;
			Status.Type = EOpenMobileAdsInitializationComponentType::Adapter;
			Status.Name = FName(*AdapterName);
			Status.Parent = TEXT("AdMob");
			Status.State = bReady
				? EOpenMobileAdsInitializationState::Ready
				: EOpenMobileAdsInitializationState::Failed;
			Status.LatencyMilliseconds = FMath::Max(0.0, LatencyMilliseconds);
			if (!bReady)
			{
				Status.Error = FOpenMobileAdsError::Make(
					EOpenMobileAdsErrorCode::ProviderFailure,
					EOpenMobileAdsFailureStage::Initialization,
					NAME_None,
					Description.IsEmpty()
						? TEXT("The AdMob adapter did not initialize.")
						: MoveTemp(Description),
					TEXT("AdMob")
				);
			}
			BroadcastInitializationStatus(MoveTemp(Status));
		}
	);
}

void FOpenMobileAdsAdMobPlatform::NativeRewardedLoadCompleted(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FRewardedLoadOperation Operation;
		if (RemoveLoadOperation(RequestId, Operation))
		{
			if (int64* PreviousRequestId = LoadedRewardedAdRequestIds.Find(
				Operation.Placement
			))
			{
				if (*PreviousRequestId != RequestId)
				{
					if (IOpenMobileAdsAdMobBackend* Backend = FindBackend())
					{
						Backend->CancelRewardedAd(*PreviousRequestId);
					}
					*PreviousRequestId = RequestId;
				}
			}
			else
			{
				LoadedRewardedAdRequestIds.Add(Operation.Placement, RequestId);
			}
			Operation.Loaded.ExecuteIfBound();
		}
	});
}

void FOpenMobileAdsAdMobPlatform::NativeRewardedLoadFailed(
	int64 RequestId,
	FString ErrorMessage
)
{
	OpenMobile::DispatchToGameThread([RequestId, ErrorMessage = MoveTemp(ErrorMessage)]() mutable
	{
		using namespace OpenMobileAdsAdMobPlatformPrivate;
		FRewardedLoadOperation Operation;
		if (RemoveLoadOperation(RequestId, Operation))
		{
			Operation.Failed.ExecuteIfBound(MoveTemp(ErrorMessage));
		}
	});
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
