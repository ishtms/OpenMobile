#include "OpenMobileDeviceUserInitiatedPasteService.h"

#include "Async/Async.h"
#include "IOpenMobileDeviceBackend.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceClipboardPolicy.h"
#include "OpenMobileDeviceSnapshotService.h"
#include "OpenMobileDeviceUserInitiatedPastePolicy.h"

namespace OpenMobileDeviceUserInitiatedPasteServicePrivate
{
	FGuid ActiveOperationId;
	EOpenMobileClipboardContentType ActiveContentType =
		EOpenMobileClipboardContentType::Unknown;
	FOpenMobileDeviceCallbackToken ActiveBackendToken;
	FOpenMobileDeviceUserInitiatedPasteCompletion ActiveCompletion;
	bool bApplicationActive = true;
	bool bStarted = false;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;
	FDelegateHandle ReactivatedHandle;

	FOpenMobileUserInitiatedPasteResult MakeCancelled(const FString& Message)
	{
		FOpenMobileUserInitiatedPasteResult Result;
		Result.State = EOpenMobileUserInitiatedPasteState::Cancelled;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Cancelled,
			Message
		);
		Result.Content.bReadWasUserInitiated = true;
		FOpenMobileDeviceSnapshotService::StampClipboardContent(Result.Content);
		return Result;
	}

	void ResetActiveState()
	{
		ActiveOperationId.Invalidate();
		ActiveContentType = EOpenMobileClipboardContentType::Unknown;
		ActiveBackendToken = {};
		ActiveCompletion = {};
	}

	void Deliver(FOpenMobileUserInitiatedPasteResult Result)
	{
		FOpenMobileDeviceUserInitiatedPasteCompletion Completion =
			MoveTemp(ActiveCompletion);
		ActiveOperationId.Invalidate();
		ActiveContentType = EOpenMobileClipboardContentType::Unknown;
		ActiveBackendToken = {};
		if (Completion)
		{
			Completion(MoveTemp(Result));
		}
	}

	void NormalizeSuccessfulResult(FOpenMobileUserInitiatedPasteResult& Result)
	{
		if (Result.State != EOpenMobileUserInitiatedPasteState::Success)
		{
			Result.Content = {};
			if (Result.State == EOpenMobileUserInitiatedPasteState::Unknown)
			{
				Result.State = EOpenMobileUserInitiatedPasteState::Failed;
			}
			if (!Result.Error.IsSet())
			{
				const bool bCancelled = Result.State
					== EOpenMobileUserInitiatedPasteState::Cancelled;
				Result.Error = FOpenMobileError::Make(
					bCancelled
						? EOpenMobileErrorCode::Cancelled
						: EOpenMobileErrorCode::NativeFailure,
					bCancelled
						? TEXT("User-initiated paste was cancelled.")
						: TEXT("User-initiated paste did not return a value.")
				);
			}
			return;
		}
		Result.Error = {};
		FOpenMobileDeviceOptionalString& Value =
			ActiveContentType == EOpenMobileClipboardContentType::Text
				? Result.Content.Text
				: Result.Content.Url;
		if (!Value.bIsAvailable)
		{
			Result.State = EOpenMobileUserInitiatedPasteState::Failed;
			Result.Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("User-initiated paste returned no value.")
			);
			Result.Content = {};
			return;
		}
		if (FOpenMobileDeviceClipboardPolicy::GetPayloadSizeBytes(Value.Value)
			> FOpenMobileDeviceClipboardPolicy::MaximumPayloadBytes)
		{
			Result.State = EOpenMobileUserInitiatedPasteState::Failed;
			Result.Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("Pasted content exceeds the 256 KiB UTF-8 limit.")
			);
			Result.Content = {};
			return;
		}
		if (ActiveContentType == EOpenMobileClipboardContentType::Url
			&& !FOpenMobileDeviceClipboardPolicy::IsAbsoluteUrl(Value.Value))
		{
			Result.State = EOpenMobileUserInitiatedPasteState::Failed;
			Result.Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("Pasted URL is malformed.")
			);
			Result.Content = {};
			return;
		}
		if (ActiveContentType == EOpenMobileClipboardContentType::Text)
		{
			Result.Content.Url = {};
		}
		else
		{
			Result.Content.Text = {};
		}
		Result.Content.bContentTypesAvailable = true;
		Result.Content.ContentTypes = {ActiveContentType};
	}

	void CompleteOnGameThread(
		const FGuid& OperationId,
		const FOpenMobileDeviceCallbackToken& BackendToken,
		FOpenMobileUserInitiatedPasteResult Result
	)
	{
		check(IsInGameThread());
		if (OperationId != ActiveOperationId)
		{
			return;
		}
		if (!FOpenMobileDeviceBackendRegistry::IsCallbackCurrent(BackendToken))
		{
			Deliver(MakeCancelled(
				TEXT("User-initiated paste was cancelled because the Device backend changed.")
			));
			return;
		}
		NormalizeSuccessfulResult(Result);
		Result.Content.bReadWasUserInitiated = true;
		FOpenMobileDeviceSnapshotService::StampClipboardContent(Result.Content);
		Deliver(MoveTemp(Result));
	}

	void HandleNativeCompletion(
		const FGuid& OperationId,
		const FOpenMobileDeviceCallbackToken& BackendToken,
		FOpenMobileUserInitiatedPasteResult Result
	)
	{
		if (IsInGameThread())
		{
			CompleteOnGameThread(
				OperationId,
				BackendToken,
				MoveTemp(Result)
			);
			return;
		}
		AsyncTask(
			ENamedThreads::GameThread,
			[OperationId, BackendToken, Result = MoveTemp(Result)]() mutable
			{
				CompleteOnGameThread(
					OperationId,
					BackendToken,
					MoveTemp(Result)
				);
			}
		);
	}

	void CancelActive(bool bNotify, const FString& Message)
	{
		check(IsInGameThread());
		if (!ActiveOperationId.IsValid())
		{
			return;
		}
		const FGuid OperationId = ActiveOperationId;
		FOpenMobileDeviceUserInitiatedPasteCompletion Completion =
			MoveTemp(ActiveCompletion);
		ActiveOperationId.Invalidate();
		ActiveContentType = EOpenMobileClipboardContentType::Unknown;
		const FOpenMobileDeviceCallbackToken BackendToken = ActiveBackendToken;
		ActiveBackendToken = {};
		if (FOpenMobileDeviceBackendRegistry::IsCallbackCurrent(BackendToken))
		{
			if (IOpenMobileDeviceBackend* Backend =
				FOpenMobileDeviceBackendRegistry::FindBackend())
			{
				Backend->CancelUserInitiatedPaste(OperationId);
			}
		}
		if (bNotify && Completion)
		{
			Completion(MakeCancelled(Message));
		}
	}

	void SetApplicationActive(bool bActive)
	{
		check(IsInGameThread());
		bApplicationActive = bActive;
		if (!bApplicationActive)
		{
			CancelActive(
				true,
				TEXT("User-initiated paste was cancelled because the application entered the background.")
			);
		}
	}

	void HandleBackground()
	{
		SetApplicationActive(false);
	}

	void HandleForeground()
	{
		SetApplicationActive(true);
	}
}

void FOpenMobileDeviceUserInitiatedPasteService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceUserInitiatedPasteServicePrivate;
	if (bStarted)
	{
		return;
	}
	bStarted = true;
	bApplicationActive = true;
	BackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate
		.AddStatic(&HandleBackground);
	ForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate
		.AddStatic(&HandleForeground);
	ReactivatedHandle = FCoreDelegates::ApplicationHasReactivatedDelegate
		.AddStatic(&HandleForeground);
}

void FOpenMobileDeviceUserInitiatedPasteService::Shutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceUserInitiatedPasteServicePrivate;
	CancelActive(
		true,
		TEXT("User-initiated paste was cancelled during Device shutdown.")
	);
	if (BackgroundHandle.IsValid())
	{
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(
			BackgroundHandle
		);
		BackgroundHandle.Reset();
	}
	if (ForegroundHandle.IsValid())
	{
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(
			ForegroundHandle
		);
		ForegroundHandle.Reset();
	}
	if (ReactivatedHandle.IsValid())
	{
		FCoreDelegates::ApplicationHasReactivatedDelegate.Remove(
			ReactivatedHandle
		);
		ReactivatedHandle.Reset();
	}
	bApplicationActive = true;
	bStarted = false;
}

bool FOpenMobileDeviceUserInitiatedPasteService::Begin(
	const FOpenMobileUserInitiatedPasteRequest& Request,
	FGuid& OutOperationId,
	FOpenMobileDeviceUserInitiatedPasteCompletion&& Completion,
	FOpenMobileError& OutError
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceUserInitiatedPasteServicePrivate;
	OutOperationId.Invalidate();
	OutError = {};
	if (!FOpenMobileDeviceUserInitiatedPastePolicy::Validate(Request, OutError))
	{
		return false;
	}
	if (!bApplicationActive || FOpenMobileDeviceBackendRegistry::IsShuttingDown())
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("User-initiated paste requires an active foreground application.")
		);
		return false;
	}
	if (ActiveOperationId.IsValid())
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::Busy,
			TEXT("Another user-initiated paste operation is already active.")
		);
		return false;
	}
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("No Device backend is available for user-initiated paste.")
		);
		return false;
	}
	ActiveOperationId = FGuid::NewGuid();
	ActiveContentType = Request.ContentType;
	ActiveBackendToken =
		FOpenMobileDeviceBackendRegistry::CaptureCallbackToken();
	ActiveCompletion = MoveTemp(Completion);
	OutOperationId = ActiveOperationId;
	const FGuid OperationId = ActiveOperationId;
	const FOpenMobileDeviceCallbackToken BackendToken = ActiveBackendToken;
	if (Backend->BeginUserInitiatedPaste(
		Request,
		OperationId,
		[OperationId, BackendToken](FOpenMobileUserInitiatedPasteResult Result)
		{
			HandleNativeCompletion(
				OperationId,
				BackendToken,
				MoveTemp(Result)
			);
		},
		OutError
	))
	{
		return true;
	}
	if (OperationId == ActiveOperationId)
	{
		ResetActiveState();
	}
	OutOperationId.Invalidate();
	return false;
}

void FOpenMobileDeviceUserInitiatedPasteService::Cancel(
	const FGuid& OperationId
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceUserInitiatedPasteServicePrivate;
	if (OperationId == ActiveOperationId)
	{
		CancelActive(false, FString());
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileDeviceUserInitiatedPasteService::SetApplicationActiveForTests(
	bool bActive
)
{
	OpenMobileDeviceUserInitiatedPasteServicePrivate::SetApplicationActive(
		bActive
	);
}

bool FOpenMobileDeviceUserInitiatedPasteService::HasActiveOperationForTests()
{
	return OpenMobileDeviceUserInitiatedPasteServicePrivate::ActiveOperationId
		.IsValid();
}
#endif
