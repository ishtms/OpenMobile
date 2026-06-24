#include "OpenMobileDeviceAsyncActionBase.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenMobileDeviceSubsystem.h"
#include "OpenMobileDeviceDiagnosticsSource.h"

bool UOpenMobileDeviceAsyncActionBase::InitializeAction(
	const UObject* WorldContextObject
)
{
	check(IsInGameThread());
	if (IsFinished())
	{
		return false;
	}
	if (!WorldContextObject || !GEngine)
	{
		FinishFailed(FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("The Device async action requires a valid world context object.")
		));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(
		WorldContextObject,
		EGetWorldErrorMode::ReturnNull
	);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!World || !GameInstance)
	{
		FinishFailed(FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Device async action could not resolve a Game Instance.")
		));
		return false;
	}

	StoredWorldContextObject = const_cast<UObject*>(WorldContextObject);
	RegisterWithGameInstance(WorldContextObject);
	TargetWorld = World;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this,
		&UOpenMobileDeviceAsyncActionBase::HandleWorldCleanup
	);
	Subsystem = GameInstance->GetSubsystem<UOpenMobileDeviceSubsystem>();
	if (!Subsystem.IsValid())
	{
		FinishFailed(FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Open Mobile Device subsystem is unavailable.")
		));
		return false;
	}
	Subsystem->RegisterAsyncAction(this);
	return !IsFinished();
}

void UOpenMobileDeviceAsyncActionBase::Cancel()
{
	check(IsInGameThread());
	if (IsFinished())
	{
		return;
	}
	CancelNativeOperation();
	FinishCancelled(FOpenMobileError::Make(
		EOpenMobileErrorCode::Cancelled,
		TEXT("The Device async action was cancelled.")
	));
}

void UOpenMobileDeviceAsyncActionBase::FinishSucceeded()
{
	check(IsInGameThread());
	if (!TrySetTerminalState(EOpenMobileDeviceAsyncTerminalState::Succeeded))
	{
		return;
	}
	const FOpenMobileError NoError;
	Cleanup();
	OnActionSucceeded();
	Success.Broadcast();
	NativeTerminal.Broadcast(EOpenMobileDeviceAsyncTerminalState::Succeeded, NoError);
	SetReadyToDestroy();
}

void UOpenMobileDeviceAsyncActionBase::FinishFailed(FOpenMobileError Error)
{
	check(IsInGameThread());
	if (!TrySetTerminalState(EOpenMobileDeviceAsyncTerminalState::Failed))
	{
		return;
	}
	FOpenMobileDeviceDiagnosticsSource::RecordError(GetClass()->GetFName(), Error);
	Cleanup();
	OnActionFailed(Error);
	Failed.Broadcast(Error);
	NativeTerminal.Broadcast(EOpenMobileDeviceAsyncTerminalState::Failed, Error);
	SetReadyToDestroy();
}

void UOpenMobileDeviceAsyncActionBase::FinishCancelled(FOpenMobileError Error)
{
	check(IsInGameThread());
	if (!TrySetTerminalState(EOpenMobileDeviceAsyncTerminalState::Cancelled))
	{
		return;
	}
	FOpenMobileDeviceDiagnosticsSource::RecordError(GetClass()->GetFName(), Error);
	Cleanup();
	OnActionCancelled(Error);
	Cancelled.Broadcast(Error);
	NativeTerminal.Broadcast(EOpenMobileDeviceAsyncTerminalState::Cancelled, Error);
	SetReadyToDestroy();
}

bool UOpenMobileDeviceAsyncActionBase::TrySetTerminalState(
	EOpenMobileDeviceAsyncTerminalState State
)
{
	if (TerminalState != EOpenMobileDeviceAsyncTerminalState::Pending)
	{
		return false;
	}
	TerminalState = State;
	return true;
}

void UOpenMobileDeviceAsyncActionBase::HandleWorldCleanup(
	UWorld* World,
	bool bSessionEnded,
	bool bCleanupResources
)
{
	static_cast<void>(bSessionEnded);
	static_cast<void>(bCleanupResources);
	if (IsFinished() || World != TargetWorld.Get())
	{
		return;
	}
	CancelNativeOperation();
	FinishCancelled(FOpenMobileError::Make(
		EOpenMobileErrorCode::Cancelled,
		TEXT("The Device async action was cancelled because its world is shutting down.")
	));
}

void UOpenMobileDeviceAsyncActionBase::HandleGameInstanceTeardown()
{
	if (IsFinished())
	{
		return;
	}
	CancelNativeOperation();
	FinishCancelled(FOpenMobileError::Make(
		EOpenMobileErrorCode::Cancelled,
		TEXT("The Device async action was cancelled because its Game Instance is shutting down.")
	));
}

void UOpenMobileDeviceAsyncActionBase::Cleanup()
{
	if (WorldCleanupHandle.IsValid())
	{
		FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		WorldCleanupHandle.Reset();
	}
	if (Subsystem.IsValid())
	{
		Subsystem->UnregisterAsyncAction(this);
		Subsystem.Reset();
	}
	TargetWorld.Reset();
	StoredWorldContextObject = nullptr;
}
