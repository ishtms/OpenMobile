#include "OpenMobileDeviceClipboardService.h"

#include "IOpenMobileDeviceBackend.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceClipboardPolicy.h"
#include "OpenMobileDeviceSnapshotService.h"

namespace OpenMobileDeviceClipboardServicePrivate
{
	bool bApplicationActive = true;
	bool bStarted = false;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;
	FDelegateHandle ReactivatedHandle;

	void SetApplicationActive(bool bActive)
	{
		check(IsInGameThread());
		bApplicationActive = bActive;
	}

	void HandleBackground()
	{
		SetApplicationActive(false);
	}

	void HandleForeground()
	{
		SetApplicationActive(true);
	}

	void Stamp(FOpenMobileClipboardOperationResult& Result)
	{
		FOpenMobileDeviceSnapshotService::StampClipboardContent(Result.Content);
	}

	FOpenMobileClipboardOperationResult MakeUnavailable()
	{
		FOpenMobileClipboardOperationResult Result;
		Result.State = EOpenMobileClipboardOperationState::Unavailable;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("Clipboard operations require an active foreground application.")
		);
		Stamp(Result);
		return Result;
	}

	FOpenMobileClipboardOperationResult MakeUnsupported()
	{
		FOpenMobileClipboardOperationResult Result;
		Result.State = EOpenMobileClipboardOperationState::Unsupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The active Device backend does not support this clipboard operation.")
		);
		Stamp(Result);
		return Result;
	}

	bool CanOperate()
	{
		return bApplicationActive
			&& !FOpenMobileDeviceBackendRegistry::IsShuttingDown();
	}

	void EnforceReadPayloadLimit(
		EOpenMobileClipboardContentType ContentType,
		FOpenMobileClipboardOperationResult& Result
	)
	{
		if (Result.State != EOpenMobileClipboardOperationState::Succeeded)
		{
			return;
		}
		const FOpenMobileDeviceOptionalString& Value =
			ContentType == EOpenMobileClipboardContentType::Text
				? Result.Content.Text
				: Result.Content.Url;
		if (!Value.bIsAvailable)
		{
			Result.State = EOpenMobileClipboardOperationState::Failed;
			Result.Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("The clipboard backend returned no value for a successful read.")
			);
			Result.Content.Text = {};
			Result.Content.Url = {};
			return;
		}
		if (FOpenMobileDeviceClipboardPolicy::GetPayloadSizeBytes(Value.Value)
			> FOpenMobileDeviceClipboardPolicy::MaximumPayloadBytes)
		{
			Result.State = EOpenMobileClipboardOperationState::TooLarge;
			Result.Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("Clipboard payload exceeds the 256 KiB UTF-8 limit.")
			);
			Result.Content.Text = {};
			Result.Content.Url = {};
			return;
		}
		if (ContentType == EOpenMobileClipboardContentType::Url
			&& !FOpenMobileDeviceClipboardPolicy::IsAbsoluteUrl(Value.Value))
		{
			Result.State = EOpenMobileClipboardOperationState::InvalidValue;
			Result.Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("Clipboard contained a malformed URL value.")
			);
			Result.Content.Url = {};
		}
	}
}

void FOpenMobileDeviceClipboardService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceClipboardServicePrivate;
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

void FOpenMobileDeviceClipboardService::Shutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceClipboardServicePrivate;
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

FOpenMobileClipboardOperationResult
FOpenMobileDeviceClipboardService::CheckContentTypes()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceClipboardServicePrivate;
	if (!CanOperate())
	{
		return MakeUnavailable();
	}
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		return MakeUnsupported();
	}
	FOpenMobileClipboardOperationResult Result =
		Backend->CheckClipboardContentTypes();
	Result.Content.Text = {};
	Result.Content.Url = {};
	Result.Content.bReadWasUserInitiated = false;
	Stamp(Result);
	return Result;
}

FOpenMobileClipboardOperationResult FOpenMobileDeviceClipboardService::Write(
	const FOpenMobileClipboardWriteRequest& Request
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceClipboardServicePrivate;
	if (!CanOperate())
	{
		return MakeUnavailable();
	}
	FOpenMobileClipboardOperationResult Result;
	if (FOpenMobileDeviceClipboardPolicy::GetPayloadSizeBytes(Request.Value)
		> FOpenMobileDeviceClipboardPolicy::MaximumPayloadBytes)
	{
		Result.State = EOpenMobileClipboardOperationState::TooLarge;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("Clipboard payload exceeds the 256 KiB UTF-8 limit.")
		);
		Stamp(Result);
		return Result;
	}
	if (!FOpenMobileDeviceClipboardPolicy::ValidateWrite(Request, Result.Error))
	{
		Result.State = EOpenMobileClipboardOperationState::InvalidValue;
		Stamp(Result);
		return Result;
	}
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		return MakeUnsupported();
	}
	Result = Backend->WriteClipboard(Request);
	Result.Content.bReadWasUserInitiated = false;
	Stamp(Result);
	return Result;
}

FOpenMobileClipboardOperationResult FOpenMobileDeviceClipboardService::Read(
	EOpenMobileClipboardContentType ContentType
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceClipboardServicePrivate;
	if (!CanOperate())
	{
		return MakeUnavailable();
	}
	FOpenMobileClipboardOperationResult Result;
	if (!FOpenMobileDeviceClipboardPolicy::ValidateReadType(
		ContentType,
		Result.Error
	))
	{
		Result.State = EOpenMobileClipboardOperationState::InvalidValue;
		Stamp(Result);
		return Result;
	}
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		return MakeUnsupported();
	}
	Result = Backend->ReadClipboard(ContentType);
	Result.Content.bReadWasUserInitiated = false;
	EnforceReadPayloadLimit(ContentType, Result);
	Stamp(Result);
	return Result;
}

FOpenMobileClipboardOperationResult FOpenMobileDeviceClipboardService::Clear()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceClipboardServicePrivate;
	if (!CanOperate())
	{
		return MakeUnavailable();
	}
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		return MakeUnsupported();
	}
	FOpenMobileClipboardOperationResult Result = Backend->ClearClipboard();
	Result.Content.bReadWasUserInitiated = false;
	Stamp(Result);
	return Result;
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileDeviceClipboardService::SetApplicationActiveForTests(
	bool bActive
)
{
	OpenMobileDeviceClipboardServicePrivate::SetApplicationActive(bActive);
}
#endif
