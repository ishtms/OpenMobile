#include "OpenMobileDeviceSystemUiControlService.h"

#include "IOpenMobileDeviceBackend.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceSystemUiControlPolicy.h"

namespace OpenMobileDeviceSystemUiControlServicePrivate
{
	FOpenMobileDeviceSystemUiRequestStack Requests;
	TMap<FGuid, uint64> RequestSequences;
	uint64 NextRequestSequence = 1;
	bool bStarted = false;
	bool bInBackground = false;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;
	FDelegateHandle ReactivatedHandle;
	FDelegateHandle SurfaceChangedHandle;

	void ClearNativeMode()
	{
		if (IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend())
		{
			Backend->ClearSystemUiMode();
		}
	}

	void ReapplyEffectiveRequest()
	{
		if (bInBackground)
		{
			return;
		}
		const TOptional<FOpenMobileSystemUiRequest> Effective =
			Requests.GetEffectiveRequest();
		if (!Effective.IsSet())
		{
			ClearNativeMode();
			return;
		}
		if (IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend())
		{
			Backend->ApplySystemUiMode(Effective.GetValue());
		}
	}

	void HandleBackground()
	{
		if (bInBackground)
		{
			return;
		}
		bInBackground = true;
		if (!Requests.IsEmpty())
		{
			ClearNativeMode();
		}
	}

	void HandleForeground()
	{
		bInBackground = false;
		if (!Requests.IsEmpty())
		{
			ReapplyEffectiveRequest();
		}
	}

	void HandleSurfaceChanged()
	{
		if (!Requests.IsEmpty())
		{
			ReapplyEffectiveRequest();
		}
	}
}

void FOpenMobileDeviceSystemUiControlService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceSystemUiControlServicePrivate;
	if (bStarted)
	{
		return;
	}
	bStarted = true;
	bInBackground = false;
	BackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate
		.AddStatic(&HandleBackground);
	ForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate
		.AddStatic(&HandleForeground);
	ReactivatedHandle = FCoreDelegates::ApplicationHasReactivatedDelegate
		.AddStatic(&HandleForeground);
	SurfaceChangedHandle = FCoreDelegates::OnSafeFrameChangedEvent.AddStatic(
		&HandleSurfaceChanged
	);
}

void FOpenMobileDeviceSystemUiControlService::Shutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceSystemUiControlServicePrivate;
	if (!Requests.IsEmpty())
	{
		ClearNativeMode();
	}
	Requests.Reset();
	RequestSequences.Reset();
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
	if (SurfaceChangedHandle.IsValid())
	{
		FCoreDelegates::OnSafeFrameChangedEvent.Remove(SurfaceChangedHandle);
		SurfaceChangedHandle.Reset();
	}
	bStarted = false;
	bInBackground = false;
}

FGuid FOpenMobileDeviceSystemUiControlService::AddRequest(
	const FOpenMobileSystemUiRequest& Request,
	FOpenMobileSystemUiResult& OutResult
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceSystemUiControlServicePrivate;
	FOpenMobileError ValidationError;
	if (!FOpenMobileDeviceSystemUiControlPolicy::Validate(
		Request,
		ValidationError
	))
	{
		OutResult = {};
		OutResult.Request = Request;
		OutResult.State = EOpenMobileSystemUiApplyState::Rejected;
		OutResult.Error = MoveTemp(ValidationError);
		return {};
	}
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		OutResult = {};
		OutResult.Request = Request;
		OutResult.State = EOpenMobileSystemUiApplyState::Unsupported;
		OutResult.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("No Device backend is available for the system UI request.")
		);
		return {};
	}
	if (bInBackground)
	{
		OutResult = {};
		OutResult.Request = Request;
		OutResult.State = EOpenMobileSystemUiApplyState::Rejected;
		OutResult.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("System UI requests cannot start while the application is backgrounded.")
		);
		return {};
	}
	OutResult = Backend->ApplySystemUiMode(Request);
	OutResult.Request = Request;
	if (!OutResult.IsAccepted())
	{
		return {};
	}
	const FGuid RequestId = FGuid::NewGuid();
	const uint64 Sequence = NextRequestSequence++;
	Requests.Add(Sequence, Request);
	RequestSequences.Add(RequestId, Sequence);
	return RequestId;
}

void FOpenMobileDeviceSystemUiControlService::RemoveRequest(
	const FGuid& RequestId
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceSystemUiControlServicePrivate;
	const uint64* Sequence = RequestSequences.Find(RequestId);
	if (!Sequence)
	{
		return;
	}
	const TOptional<FOpenMobileSystemUiRequest> Before =
		Requests.GetEffectiveRequest();
	Requests.Remove(*Sequence);
	RequestSequences.Remove(RequestId);
	const TOptional<FOpenMobileSystemUiRequest> After =
		Requests.GetEffectiveRequest();
	if (Before == After)
	{
		return;
	}
	if (After.IsSet())
	{
		ReapplyEffectiveRequest();
	}
	else
	{
		ClearNativeMode();
	}
}

int32 FOpenMobileDeviceSystemUiControlService::GetActiveRequestCountForDiagnostics()
{
	check(IsInGameThread());
	return OpenMobileDeviceSystemUiControlServicePrivate::RequestSequences.Num();
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileDeviceSystemUiControlService::NotifySurfaceChangedForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceSystemUiControlServicePrivate::HandleSurfaceChanged();
}

void FOpenMobileDeviceSystemUiControlService::NotifyBackgroundForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceSystemUiControlServicePrivate::HandleBackground();
}

void FOpenMobileDeviceSystemUiControlService::NotifyForegroundForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceSystemUiControlServicePrivate::HandleForeground();
}

void FOpenMobileDeviceSystemUiControlService::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceSystemUiControlServicePrivate;
	if (!Requests.IsEmpty())
	{
		ClearNativeMode();
	}
	Requests.Reset();
	RequestSequences.Reset();
	NextRequestSequence = 1;
	bInBackground = false;
}
#endif
