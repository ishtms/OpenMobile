#include "OpenMobileDeviceUserInitiatedPasteAsyncAction.h"

#include "OpenMobileDeviceUserInitiatedPasteService.h"

UOpenMobileDeviceUserInitiatedPasteAsyncAction*
UOpenMobileDeviceUserInitiatedPasteAsyncAction::RequestUserInitiatedPaste(
	UObject* WorldContextObject,
	const FOpenMobileUserInitiatedPasteRequest& Request
)
{
	UOpenMobileDeviceUserInitiatedPasteAsyncAction* Action =
		NewObject<UOpenMobileDeviceUserInitiatedPasteAsyncAction>();
	Action->WorldContextObject = WorldContextObject;
	Action->Request = Request;
	return Action;
}

void UOpenMobileDeviceUserInitiatedPasteAsyncAction::Activate()
{
	if (!InitializeAction(WorldContextObject))
	{
		return;
	}
	TWeakObjectPtr<UOpenMobileDeviceUserInitiatedPasteAsyncAction> WeakThis(this);
	FOpenMobileError Error;
	if (!FOpenMobileDeviceUserInitiatedPasteService::Begin(
		Request,
		OperationId,
		[WeakThis](FOpenMobileUserInitiatedPasteResult InResult)
		{
			UOpenMobileDeviceUserInitiatedPasteAsyncAction* Action =
				WeakThis.Get();
			if (!Action || Action->IsFinished())
			{
				return;
			}
			Action->OperationId.Invalidate();
			Action->Result = MoveTemp(InResult);
			switch (Action->Result.State)
			{
			case EOpenMobileUserInitiatedPasteState::Success:
				Action->FinishSucceeded();
				break;
			case EOpenMobileUserInitiatedPasteState::Cancelled:
				Action->FinishCancelled(Action->Result.Error);
				break;
			case EOpenMobileUserInitiatedPasteState::Denied:
			case EOpenMobileUserInitiatedPasteState::Failed:
			case EOpenMobileUserInitiatedPasteState::Unknown:
				Action->FinishFailed(Action->Result.Error);
				break;
			}
		},
		Error
	))
	{
		Result.State = EOpenMobileUserInitiatedPasteState::Failed;
		Result.Error = Error;
		FinishFailed(Error);
	}
}

void UOpenMobileDeviceUserInitiatedPasteAsyncAction::CancelNativeOperation()
{
	if (OperationId.IsValid())
	{
		FOpenMobileDeviceUserInitiatedPasteService::Cancel(OperationId);
		OperationId.Invalidate();
	}
}

void UOpenMobileDeviceUserInitiatedPasteAsyncAction::OnActionFailed(
	const FOpenMobileError& Error
)
{
	if (Result.State == EOpenMobileUserInitiatedPasteState::Unknown)
	{
		Result.State = EOpenMobileUserInitiatedPasteState::Failed;
		Result.Error = Error;
		Result.Content = {};
	}
}

void UOpenMobileDeviceUserInitiatedPasteAsyncAction::OnActionCancelled(
	const FOpenMobileError& Error
)
{
	if (Result.State == EOpenMobileUserInitiatedPasteState::Unknown)
	{
		Result.State = EOpenMobileUserInitiatedPasteState::Cancelled;
		Result.Error = Error;
		Result.Content = {};
	}
}
