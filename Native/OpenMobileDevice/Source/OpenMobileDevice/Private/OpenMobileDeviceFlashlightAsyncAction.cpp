#include "OpenMobileDeviceFlashlightAsyncAction.h"

#include "Async/Async.h"
#include "IOpenMobileDeviceBackend.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceFlashlightControlPolicy.h"
#include "OpenMobileDeviceFlashlightControlService.h"

namespace OpenMobileDeviceFlashlightAsyncActionPrivate
{
	FOpenMobileError MakeOperationError(
		EOpenMobileFlashlightOperationState State
	)
	{
		switch (State)
		{
		case EOpenMobileFlashlightOperationState::Unsupported:
			return FOpenMobileError::Make(
				EOpenMobileErrorCode::NotSupported,
				TEXT("Flashlight control is not supported.")
			);
		case EOpenMobileFlashlightOperationState::Busy:
			return FOpenMobileError::Make(
				EOpenMobileErrorCode::Busy,
				TEXT("The flashlight or camera resource is busy.")
			);
		case EOpenMobileFlashlightOperationState::PermissionRequired:
		case EOpenMobileFlashlightOperationState::PermissionDenied:
		case EOpenMobileFlashlightOperationState::Restricted:
		case EOpenMobileFlashlightOperationState::Rejected:
			return FOpenMobileError::Make(
				EOpenMobileErrorCode::Unavailable,
				TEXT("The flashlight operation is unavailable.")
			);
		case EOpenMobileFlashlightOperationState::Unknown:
		case EOpenMobileFlashlightOperationState::Applied:
			break;
		}
		return FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("The platform flashlight operation failed.")
		);
	}
}

UOpenMobileDeviceFlashlightAsyncAction*
UOpenMobileDeviceFlashlightAsyncAction::SetFlashlight(
	const UObject* WorldContextObject,
	const FOpenMobileFlashlightRequest& Request
)
{
	UOpenMobileDeviceFlashlightAsyncAction* Action =
		NewObject<UOpenMobileDeviceFlashlightAsyncAction>();
	Action->WorldContextObject = const_cast<UObject*>(WorldContextObject);
	Action->Request = Request;
	Action->Result.Request = Request;
	return Action;
}

void UOpenMobileDeviceFlashlightAsyncAction::Activate()
{
	check(IsInGameThread());
	if (!InitializeAction(WorldContextObject))
	{
		return;
	}
	FOpenMobileError Error;
	if (!FOpenMobileDeviceFlashlightControlPolicy::Validate(Request, Error))
	{
		Result.State = EOpenMobileFlashlightOperationState::Rejected;
		Result.Error = Error;
		FinishFailed(Error);
		return;
	}
	if (!FOpenMobileDeviceBackendRegistry::FindBackend())
	{
		Result.State = EOpenMobileFlashlightOperationState::Unsupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("No Device backend is available for flashlight control.")
		);
		FinishFailed(Result.Error);
		return;
	}
	BackendGeneration =
		FOpenMobileDeviceBackendRegistry::CaptureCallbackToken().Generation;
	OperationId =
		FOpenMobileDeviceFlashlightControlService::BeginOperation(Error);
	if (!OperationId.IsValid())
	{
		Result.State = Error.Code == EOpenMobileErrorCode::Busy
			? EOpenMobileFlashlightOperationState::Busy
			: EOpenMobileFlashlightOperationState::Rejected;
		Result.Error = Error;
		FinishFailed(Error);
		return;
	}

	const TWeakObjectPtr<UOpenMobileDeviceFlashlightAsyncAction> WeakThis(this);
	AsyncTask(ENamedThreads::GameThread, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->ExecuteOperation();
		}
	});
}

void UOpenMobileDeviceFlashlightAsyncAction::CancelNativeOperation()
{
	if (OperationId.IsValid())
	{
		FOpenMobileDeviceFlashlightControlService::CancelOperation(OperationId);
		OperationId.Invalidate();
	}
}

void UOpenMobileDeviceFlashlightAsyncAction::ExecuteOperation()
{
	check(IsInGameThread());
	if (IsFinished())
	{
		return;
	}
	if (!FOpenMobileDeviceFlashlightControlService::IsOperationCurrent(
		OperationId
	))
	{
		Result.State = EOpenMobileFlashlightOperationState::Rejected;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The flashlight operation was invalidated by an application lifecycle change.")
		);
		OperationId.Invalidate();
		FinishFailed(Result.Error);
		return;
	}
	FOpenMobileDeviceCallbackToken Token;
	Token.Generation = BackendGeneration;
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend
		|| !FOpenMobileDeviceBackendRegistry::IsCallbackCurrent(Token))
	{
		FOpenMobileDeviceFlashlightControlService::CancelOperation(OperationId);
		OperationId.Invalidate();
		Result.State = EOpenMobileFlashlightOperationState::Rejected;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Device backend changed during the flashlight operation.")
		);
		FinishFailed(Result.Error);
		return;
	}

	Result = Backend->ApplyFlashlight(Request);
	FOpenMobileDeviceFlashlightControlService::CompleteOperation(
		OperationId,
		Result
	);
	OperationId.Invalidate();
	if (Result.IsApplied())
	{
		FinishSucceeded();
		return;
	}
	if (!Result.Error.IsSet())
	{
		Result.Error =
			OpenMobileDeviceFlashlightAsyncActionPrivate::MakeOperationError(
				Result.State
			);
	}
	FinishFailed(Result.Error);
}
