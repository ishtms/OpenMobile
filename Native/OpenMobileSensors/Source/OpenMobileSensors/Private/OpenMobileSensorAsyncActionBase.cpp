#include "OpenMobileSensorAsyncActionBase.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenMobileSensorsSubsystem.h"

bool UOpenMobileSensorAsyncActionBase::InitializeAction(
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
			TEXT("The Sensors async action requires a valid world context object.")
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
			TEXT("The Sensors async action could not resolve a Game Instance.")
		));
		return false;
	}

	StoredWorldContextObject = const_cast<UObject*>(WorldContextObject);
	RegisterWithGameInstance(WorldContextObject);
	TargetWorld = World;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this,
		&UOpenMobileSensorAsyncActionBase::HandleWorldCleanup
	);
	Subsystem = GameInstance->GetSubsystem<UOpenMobileSensorsSubsystem>();
	if (!Subsystem.IsValid())
	{
		FinishFailed(FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Open Mobile Sensors subsystem is unavailable.")
		));
		return false;
	}
	Subsystem->RegisterAsyncAction(this);
	return !IsFinished();
}

UOpenMobileSensorsSubsystem*
UOpenMobileSensorAsyncActionBase::GetSensorsSubsystem() const
{
	return Subsystem.Get();
}

void UOpenMobileSensorAsyncActionBase::Cancel()
{
	check(IsInGameThread());
	if (IsFinished())
	{
		return;
	}
	CancelNativeOperation();
	FinishCancelled(FOpenMobileError::Make(
		EOpenMobileErrorCode::Cancelled,
		TEXT("The Sensors async action was cancelled.")
	));
}

void UOpenMobileSensorAsyncActionBase::FinishSucceeded()
{
	check(IsInGameThread());
	if (!TrySetTerminalState(EOpenMobileSensorAsyncTerminalState::Succeeded))
	{
		return;
	}
	Cleanup();
	OnActionSucceeded();
	SetReadyToDestroy();
}

void UOpenMobileSensorAsyncActionBase::FinishFailed(FOpenMobileError Error)
{
	check(IsInGameThread());
	if (!TrySetTerminalState(EOpenMobileSensorAsyncTerminalState::Failed))
	{
		return;
	}
	Cleanup();
	OnActionFailed(Error);
	SetReadyToDestroy();
}

void UOpenMobileSensorAsyncActionBase::FinishCancelled(FOpenMobileError Error)
{
	check(IsInGameThread());
	if (!TrySetTerminalState(EOpenMobileSensorAsyncTerminalState::Cancelled))
	{
		return;
	}
	Cleanup();
	OnActionCancelled(Error);
	SetReadyToDestroy();
}

bool UOpenMobileSensorAsyncActionBase::TrySetTerminalState(
	EOpenMobileSensorAsyncTerminalState State
)
{
	if (TerminalState != EOpenMobileSensorAsyncTerminalState::Pending)
	{
		return false;
	}
	TerminalState = State;
	return true;
}

void UOpenMobileSensorAsyncActionBase::HandleWorldCleanup(
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
		TEXT("The Sensors async action was cancelled because its world is shutting down.")
	));
}

void UOpenMobileSensorAsyncActionBase::HandleGameInstanceTeardown()
{
	if (IsFinished())
	{
		return;
	}
	CancelNativeOperation();
	FinishCancelled(FOpenMobileError::Make(
		EOpenMobileErrorCode::Cancelled,
		TEXT("The Sensors async action was cancelled because its Game Instance is shutting down.")
	));
}

void UOpenMobileSensorAsyncActionBase::Cleanup()
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
