#include "OpenMobilePermissions.h"

#include "IOpenMobilePermissionProvider.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileAsync.h"

namespace OpenMobilePermissionsPrivate
{
	struct FRequestState
	{
		FOpenMobilePermissionRequestHandle Handle;
		FName Permission;
		IOpenMobilePermissionProvider* Provider = nullptr;
		FName ProviderName;
		FOnOpenMobilePermissionRequestComplete Completion;
	};

	FCriticalSection RequestsMutex;
	TMap<FGuid, TSharedPtr<FRequestState, ESPMode::ThreadSafe>> Requests;
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

	void CompleteRequest(
		const TSharedPtr<FRequestState, ESPMode::ThreadSafe>& State,
		FOpenMobilePermissionResult Result
	)
	{
		OpenMobile::DispatchToGameThread(
			[State, Result = MoveTemp(Result)]() mutable
			{
				FOnOpenMobilePermissionRequestComplete Completion;
				{
					FScopeLock Lock(&RequestsMutex);
					TSharedPtr<FRequestState, ESPMode::ThreadSafe>* Current =
						Requests.Find(State->Handle.GetIdentifier());
					if (!Current || *Current != State)
					{
						return;
					}

					Requests.Remove(State->Handle.GetIdentifier());
					Completion = MoveTemp(State->Completion);
				}
				Completion.ExecuteIfBound(Result);
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
	{
		FScopeLock Lock(&RequestsMutex);
		Requests.Add(Handle.Identifier, State);
	}

	FOpenMobileError StartError;
	const bool bAccepted = Provider->RequestPermission(
		Permission,
		Handle.Identifier,
		FOpenMobileNativePermissionCompletion::CreateLambda(
			[State](
				EOpenMobilePermissionStatus Status,
				FOpenMobileError Error
			) mutable
			{
				FOpenMobilePermissionResult Result;
				Result.Permission = State->Permission;
				Result.Status = Status;
				Result.Error = MoveTemp(Error);
				if (Result.Error.IsSet() && Result.Error.Provider.IsEmpty())
				{
					Result.Error.Provider = State->ProviderName.ToString();
				}
				CompleteRequest(State, MoveTemp(Result));
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
		FOpenMobilePermissionResult Result;
		Result.Permission = Permission;
		Result.Error = MoveTemp(StartError);
		CompleteRequest(State, MoveTemp(Result));
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
	}

	State->Provider->CancelRequest(Handle.Identifier);
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
		for (const TPair<FGuid, TSharedPtr<FRequestState, ESPMode::ThreadSafe>>& Entry
			: Requests)
		{
			Providers.AddUnique(Entry.Value->Provider);
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
	TArray<TSharedPtr<FRequestState, ESPMode::ThreadSafe>> Failed;
	{
		FScopeLock Lock(&RequestsMutex);
		for (auto Iterator = Requests.CreateIterator(); Iterator; ++Iterator)
		{
			if (Iterator.Value()->Provider != &Provider)
			{
				continue;
			}
			Failed.Add(Iterator.Value());
			Iterator.RemoveCurrent();
		}
	}

	for (const TSharedPtr<FRequestState, ESPMode::ThreadSafe>& State : Failed)
	{
		Provider.CancelRequest(State->Handle.Identifier);
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
