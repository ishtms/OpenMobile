#include "OpenMobilePermissions.h"

#include "IOpenMobilePermissionProvider.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileAsync.h"

namespace OpenMobilePermissionsPrivate
{
	struct FRequestGroup;

	struct FRequestState
	{
		FOpenMobilePermissionRequestHandle Handle;
		FName Permission;
		IOpenMobilePermissionProvider* Provider = nullptr;
		FName ProviderName;
		FOnOpenMobilePermissionRequestComplete Completion;
		TWeakPtr<FRequestGroup, ESPMode::ThreadSafe> Group;
	};

	struct FRequestGroup
	{
		FGuid NativeIdentifier;
		FName Permission;
		IOpenMobilePermissionProvider* Provider = nullptr;
		FName ProviderName;
		TArray<TSharedPtr<FRequestState, ESPMode::ThreadSafe>> Requests;
	};

	FCriticalSection RequestsMutex;
	TMap<FGuid, TSharedPtr<FRequestState, ESPMode::ThreadSafe>> Requests;
	TArray<TSharedPtr<FRequestGroup, ESPMode::ThreadSafe>> RequestGroups;
	TAtomic<uint32> NextGeneration(1);
	TAtomic<bool> bShuttingDown(false);

	uint32 TakeGeneration()
	{
		uint32 Generation = NextGeneration++;
		if (Generation == 0)
		{
			Generation = NextGeneration++;
		}
		return Generation;
	}

	FOpenMobilePermissionResult MakeFailure(
		FName Permission,
		EOpenMobileErrorCode Code,
		FString Message,
		FString Provider = FString()
	)
	{
		FOpenMobilePermissionResult Result;
		Result.Permission = Permission;
		Result.Error = FOpenMobileError::Make(
			Code,
			MoveTemp(Message),
			FString(),
			MoveTemp(Provider)
		);
		return Result;
	}

	void CompleteRequestGroup(
		const TSharedPtr<FRequestGroup, ESPMode::ThreadSafe>& Group,
		EOpenMobilePermissionStatus Status,
		FOpenMobileError Error
	)
	{
		OpenMobile::DispatchToGameThread(
			[Group, Status, Error = MoveTemp(Error)]() mutable
			{
				TArray<FOnOpenMobilePermissionRequestComplete> Completions;
				{
					FScopeLock Lock(&RequestsMutex);
					const int32 GroupIndex = RequestGroups.IndexOfByKey(Group);
					if (GroupIndex == INDEX_NONE)
					{
						return;
					}
					RequestGroups.RemoveAtSwap(GroupIndex, 1, EAllowShrinking::No);
					Completions.Reserve(Group->Requests.Num());
					for (const TSharedPtr<FRequestState, ESPMode::ThreadSafe>& State
						: Group->Requests)
					{
						TSharedPtr<FRequestState, ESPMode::ThreadSafe>* Current =
							Requests.Find(State->Handle.GetIdentifier());
						if (!Current || *Current != State)
						{
							continue;
						}
						Requests.Remove(State->Handle.GetIdentifier());
						Completions.Add(MoveTemp(State->Completion));
					}
					Group->Requests.Reset();
				}
				FOpenMobilePermissionResult Result;
				Result.Permission = Group->Permission;
				Result.Status = Status;
				Result.Error = MoveTemp(Error);
				if (Result.Error.IsSet() && Result.Error.Provider.IsEmpty())
				{
					Result.Error.Provider = Group->ProviderName.ToString();
				}
				for (FOnOpenMobilePermissionRequestComplete& Completion
					: Completions)
				{
					Completion.ExecuteIfBound(Result);
				}
			}
		);
	}

	void CompleteDetached(
		FOnOpenMobilePermissionRequestComplete&& Completion,
		FOpenMobilePermissionResult Result
	)
	{
		OpenMobile::DispatchToGameThread(
			[Completion = MoveTemp(Completion), Result = MoveTemp(Result)]() mutable
			{
				Completion.ExecuteIfBound(Result);
			}
		);
	}
}

FOpenMobilePermissionResult FOpenMobilePermissions::GetStatus(FName Permission)
{
	check(IsInGameThread());
	using namespace OpenMobilePermissionsPrivate;
	if (Permission.IsNone())
	{
		return MakeFailure(
			Permission,
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("A permission name is required.")
		);
	}
	if (bShuttingDown.Load())
	{
		return MakeFailure(
			Permission,
			EOpenMobileErrorCode::Unavailable,
			TEXT("The permission service is shutting down.")
		);
	}

	IOpenMobilePermissionProvider* Provider =
		FOpenMobilePermissionProviderRegistry::FindProvider(Permission);
	if (!Provider)
	{
		return MakeFailure(
			Permission,
			EOpenMobileErrorCode::NotSupported,
			TEXT("No permission provider supports this permission.")
		);
	}

	FOpenMobilePermissionResult Result = Provider->GetStatus(Permission);
	Result.Permission = Permission;
	if (Result.Error.IsSet() && Result.Error.Provider.IsEmpty())
	{
		Result.Error.Provider = Provider->GetProviderName().ToString();
	}
	return Result;
}

FOpenMobilePermissionRequestHandle FOpenMobilePermissions::RequestPermission(
	FName Permission,
	FOnOpenMobilePermissionRequestComplete&& Completion
)
{
	check(IsInGameThread());
	using namespace OpenMobilePermissionsPrivate;
	if (!Completion.IsBound())
	{
		return {};
	}
	if (Permission.IsNone())
	{
		CompleteDetached(
			MoveTemp(Completion),
			MakeFailure(
				Permission,
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("A permission name is required.")
			)
		);
		return {};
	}
	if (bShuttingDown.Load())
	{
		CompleteDetached(
			MoveTemp(Completion),
			MakeFailure(
				Permission,
				EOpenMobileErrorCode::Unavailable,
				TEXT("The permission service is shutting down.")
			)
		);
		return {};
	}

	IOpenMobilePermissionProvider* Provider =
		FOpenMobilePermissionProviderRegistry::FindProvider(Permission);
	if (!Provider)
	{
		CompleteDetached(
			MoveTemp(Completion),
			MakeFailure(
				Permission,
				EOpenMobileErrorCode::NotSupported,
				TEXT("No permission provider supports this permission.")
			)
		);
		return {};
	}

	FOpenMobilePermissionRequestHandle Handle;
	Handle.Identifier = FGuid::NewGuid();
	Handle.Generation = TakeGeneration();

	TSharedPtr<FRequestState, ESPMode::ThreadSafe> State =
		MakeShared<FRequestState, ESPMode::ThreadSafe>();
	State->Handle = Handle;
	State->Permission = Permission;
	State->Provider = Provider;
	State->ProviderName = Provider->GetProviderName();
	State->Completion = MoveTemp(Completion);
	TSharedPtr<FRequestGroup, ESPMode::ThreadSafe> Group;
	bool bStartNativeRequest = false;
	{
		FScopeLock Lock(&RequestsMutex);
		for (const TSharedPtr<FRequestGroup, ESPMode::ThreadSafe>& Candidate
			: RequestGroups)
		{
			if (Candidate->Provider == Provider
				&& Candidate->Permission == Permission)
			{
				Group = Candidate;
				break;
			}
		}
		if (!Group)
		{
			Group = MakeShared<FRequestGroup, ESPMode::ThreadSafe>();
			Group->NativeIdentifier = FGuid::NewGuid();
			Group->Permission = Permission;
			Group->Provider = Provider;
			Group->ProviderName = Provider->GetProviderName();
			RequestGroups.Add(Group);
			bStartNativeRequest = true;
		}
		State->Group = Group;
		Group->Requests.Add(State);
		Requests.Add(Handle.Identifier, State);
	}
	if (!bStartNativeRequest)
	{
		return Handle;
	}

	FOpenMobileError StartError;
	const bool bAccepted = Provider->RequestPermission(
		Permission,
		Group->NativeIdentifier,
		FOpenMobileNativePermissionCompletion::CreateLambda(
			[Group](
				EOpenMobilePermissionStatus Status,
				FOpenMobileError Error
			) mutable
			{
				CompleteRequestGroup(Group, Status, MoveTemp(Error));
			}
		),
		StartError
	);
	if (!bAccepted)
	{
		if (!StartError.IsSet())
		{
			StartError = FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("The permission provider did not accept the request."),
				FString(),
				Provider->GetProviderName().ToString()
			);
		}
		CompleteRequestGroup(
			Group,
			EOpenMobilePermissionStatus::NotDetermined,
			MoveTemp(StartError)
		);
		return {};
	}

	return Handle;
}

bool FOpenMobilePermissions::CancelRequest(
	const FOpenMobilePermissionRequestHandle& Handle
)
{
	check(IsInGameThread());
	using namespace OpenMobilePermissionsPrivate;
	if (!Handle.IsValid())
	{
		return false;
	}

	TSharedPtr<FRequestState, ESPMode::ThreadSafe> State;
	TSharedPtr<FRequestGroup, ESPMode::ThreadSafe> Group;
	bool bCancelNativeRequest = false;
	{
		FScopeLock Lock(&RequestsMutex);
		TSharedPtr<FRequestState, ESPMode::ThreadSafe>* Found =
			Requests.Find(Handle.Identifier);
		if (!Found || (*Found)->Handle != Handle)
		{
			return false;
		}
		State = *Found;
		Requests.Remove(Handle.Identifier);
		Group = State->Group.Pin();
		if (Group)
		{
			Group->Requests.RemoveSingleSwap(State, EAllowShrinking::No);
			if (Group->Requests.IsEmpty())
			{
				RequestGroups.RemoveSingleSwap(Group, EAllowShrinking::No);
				bCancelNativeRequest = true;
			}
		}
	}

	if (bCancelNativeRequest)
	{
		State->Provider->CancelRequest(Group->NativeIdentifier);
	}
	CompleteDetached(
		MoveTemp(State->Completion),
		MakeFailure(
			State->Permission,
			EOpenMobileErrorCode::Cancelled,
			TEXT("The permission request was cancelled."),
			State->Provider->GetProviderName().ToString()
		)
	);
	return true;
}

void FOpenMobilePermissions::Start()
{
	check(IsInGameThread());
	OpenMobilePermissionsPrivate::bShuttingDown.Store(false);
}

void FOpenMobilePermissions::BeginShutdown()
{
	check(IsInGameThread());
	using namespace OpenMobilePermissionsPrivate;
	if (bShuttingDown.Exchange(true))
	{
		return;
	}

	TArray<IOpenMobilePermissionProvider*> Providers;
	{
		FScopeLock Lock(&RequestsMutex);
		for (const TSharedPtr<FRequestGroup, ESPMode::ThreadSafe>& Group
			: RequestGroups)
		{
			Providers.AddUnique(Group->Provider);
		}
	}
	for (IOpenMobilePermissionProvider* Provider : Providers)
	{
		if (Provider)
		{
			FailRequestsForProvider(
				*Provider,
				FOpenMobileError::Make(
					EOpenMobileErrorCode::Cancelled,
					TEXT("The permission service is shutting down."),
					FString(),
					Provider->GetProviderName().ToString()
				)
			);
		}
	}
}

void FOpenMobilePermissions::FailRequestsForProvider(
	IOpenMobilePermissionProvider& Provider,
	const FOpenMobileError& Error
)
{
	check(IsInGameThread());
	using namespace OpenMobilePermissionsPrivate;
	TArray<TSharedPtr<FRequestGroup, ESPMode::ThreadSafe>> FailedGroups;
	TArray<TSharedPtr<FRequestState, ESPMode::ThreadSafe>> Failed;
	{
		FScopeLock Lock(&RequestsMutex);
		for (auto GroupIterator = RequestGroups.CreateIterator();
			GroupIterator;
			++GroupIterator)
		{
			const TSharedPtr<FRequestGroup, ESPMode::ThreadSafe>& Group =
				*GroupIterator;
			if (Group->Provider != &Provider)
			{
				continue;
			}
			FailedGroups.Add(Group);
			for (const TSharedPtr<FRequestState, ESPMode::ThreadSafe>& State
				: Group->Requests)
			{
				TSharedPtr<FRequestState, ESPMode::ThreadSafe>* Current =
					Requests.Find(State->Handle.GetIdentifier());
				if (Current && *Current == State)
				{
					Requests.Remove(State->Handle.GetIdentifier());
					Failed.Add(State);
				}
			}
			Group->Requests.Reset();
			GroupIterator.RemoveCurrent();
		}
	}

	for (const TSharedPtr<FRequestGroup, ESPMode::ThreadSafe>& Group
		: FailedGroups)
	{
		Provider.CancelRequest(Group->NativeIdentifier);
	}
	for (const TSharedPtr<FRequestState, ESPMode::ThreadSafe>& State : Failed)
	{
		FOpenMobilePermissionResult Result;
		Result.Permission = State->Permission;
		Result.Error = Error;
		CompleteDetached(MoveTemp(State->Completion), MoveTemp(Result));
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobilePermissions::ResetForTests()
{
	check(IsInGameThread());
	BeginShutdown();
	OpenMobilePermissionsPrivate::bShuttingDown.Store(false);
}
#endif
