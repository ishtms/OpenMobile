#include "OpenMobileDeviceStorageQueryAsyncAction.h"

#include "Async/Async.h"
#include "IOpenMobileDeviceBackend.h"
#include "OpenMobileAsync.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceSettings.h"
#include "OpenMobileDeviceSnapshotService.h"
#include "OpenMobileDeviceStorageInfo.h"
#include "OpenMobileDeviceSubsystem.h"

UOpenMobileDeviceStorageQueryAsyncAction*
UOpenMobileDeviceStorageQueryAsyncAction::QueryStorage(
	const UObject* WorldContextObject
)
{
	UOpenMobileDeviceStorageQueryAsyncAction* Action =
		NewObject<UOpenMobileDeviceStorageQueryAsyncAction>();
	Action->WorldContextObject = const_cast<UObject*>(WorldContextObject);
	return Action;
}

void UOpenMobileDeviceStorageQueryAsyncAction::Activate()
{
	check(IsInGameThread());
	if (!InitializeAction(WorldContextObject))
	{
		return;
	}
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		FinishFailed(FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("No Device backend is available for the storage query.")
		));
		return;
	}
	const uint64 BackendGeneration =
		FOpenMobileDeviceBackendRegistry::CaptureCallbackToken().Generation;
	CancellationFlag = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
	const TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> WorkerCancellation =
		CancellationFlag;
	const TWeakObjectPtr<UOpenMobileDeviceStorageQueryAsyncAction> WeakThis(this);
	Async(EAsyncExecution::ThreadPool, [
		WeakThis,
		Backend,
		BackendGeneration,
		WorkerCancellation
	]()
	{
		if (WorkerCancellation->Load())
		{
			return;
		}
		FOpenMobileStorageSnapshot Result;
		FOpenMobileError Error;
		const bool bSucceeded = Backend->QueryStorageSnapshot(Result, Error);
		OpenMobile::DispatchToGameThread([
			WeakThis,
			Result,
			Error,
			bSucceeded,
			BackendGeneration,
			WorkerCancellation
		]() mutable
		{
			if (!WorkerCancellation->Load() && WeakThis.IsValid())
			{
				WeakThis->HandleQueryComplete(
					MoveTemp(Result),
					MoveTemp(Error),
					bSucceeded,
					BackendGeneration
				);
			}
		});
	});
}

void UOpenMobileDeviceStorageQueryAsyncAction::CancelNativeOperation()
{
	if (CancellationFlag)
	{
		CancellationFlag->Store(true);
	}
}

void UOpenMobileDeviceStorageQueryAsyncAction::HandleQueryComplete(
	FOpenMobileStorageSnapshot Result,
	FOpenMobileError Error,
	bool bSucceeded,
	uint64 BackendGeneration
)
{
	check(IsInGameThread());
	if (IsFinished())
	{
		return;
	}
	FOpenMobileDeviceCallbackToken CallbackToken;
	CallbackToken.Generation = BackendGeneration;
	if (!FOpenMobileDeviceBackendRegistry::IsCallbackCurrent(CallbackToken))
	{
		FinishFailed(FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Device backend changed during the storage query.")
		));
		return;
	}
	if (!bSucceeded)
	{
		FinishFailed(Error.IsSet()
			? MoveTemp(Error)
			: FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("The platform storage query failed.")
			));
		return;
	}
	const IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		FinishFailed(FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Device backend became unavailable during the storage query.")
		));
		return;
	}
	const UOpenMobileDeviceSettings* Settings =
		GetDefault<UOpenMobileDeviceSettings>();
	TOptional<bool> PreviousLowStorageState;
	if (UOpenMobileDeviceSubsystem* DeviceSubsystem = GetDeviceSubsystem())
	{
		const FOpenMobileStorageSnapshot Previous =
			DeviceSubsystem->GetStorageSnapshot();
		if (Previous.bIsLowStorage.bIsAvailable)
		{
			PreviousLowStorageState = Previous.bIsLowStorage.Value;
		}
	}
	FOpenMobileDeviceStorageInfo::ApplyLowStorageState(
		Result,
		Settings->ResolveLowStorageThresholdBytes(
			Backend->GetPlatformLowStorageThresholdBytes(Result)
		),
		Settings->GetValidatedLowStorageRecoveryHysteresisBytes(),
		PreviousLowStorageState
	);
	FOpenMobileDeviceSnapshotService::StampStorageSnapshot(Result);
	Snapshot = MoveTemp(Result);
	if (UOpenMobileDeviceSubsystem* DeviceSubsystem = GetDeviceSubsystem())
	{
		DeviceSubsystem->CacheStorageSnapshot(Snapshot, BackendGeneration);
	}
	FinishSucceeded();
}
