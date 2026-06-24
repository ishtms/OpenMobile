#include "OpenMobileDeviceRefreshRateControlService.h"

#include "IOpenMobileDeviceBackend.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceRefreshRateControlPolicy.h"

namespace OpenMobileDeviceRefreshRateControlServicePrivate
{
	FOpenMobileDeviceRefreshRateRequestStack Requests;
	TMap<FGuid, uint64> RequestSequences;
	uint64 NextRequestSequence = 1;
	bool bStarted = false;
	bool bInBackground = false;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;
	FDelegateHandle ReactivatedHandle;
	FDelegateHandle SurfaceChangedHandle;

	void ClearNativePreference()
	{
		if (IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend())
		{
			Backend->ClearPreferredRefreshRate();
		}
	}

	void ReapplyEffectiveRequest()
	{
		if (bInBackground)
		{
			return;
		}
		const TOptional<FOpenMobilePreferredRefreshRateRequest> Effective =
			Requests.GetEffectiveRequest();
		if (!Effective.IsSet())
		{
			ClearNativePreference();
			return;
		}
		if (IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend())
		{
			Backend->ApplyPreferredRefreshRate(Effective.GetValue());
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
			ClearNativePreference();
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

void FOpenMobileDeviceRefreshRateControlService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceRefreshRateControlServicePrivate;
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

void FOpenMobileDeviceRefreshRateControlService::Shutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceRefreshRateControlServicePrivate;
	if (!Requests.IsEmpty())
	{
		ClearNativePreference();
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

FGuid FOpenMobileDeviceRefreshRateControlService::AddRequest(
	const FOpenMobilePreferredRefreshRateRequest& Request,
	FOpenMobilePreferredRefreshRateResult& OutResult
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceRefreshRateControlServicePrivate;
	FOpenMobileError ValidationError;
	if (!FOpenMobileDeviceRefreshRateControlPolicy::Validate(
		Request,
		ValidationError
	))
	{
		OutResult = {};
		OutResult.Request = Request;
		OutResult.State = EOpenMobilePreferredRefreshRateApplyState::Rejected;
		OutResult.Error = MoveTemp(ValidationError);
		return {};
	}
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		OutResult = {};
		OutResult.Request = Request;
		OutResult.State = EOpenMobilePreferredRefreshRateApplyState::Unsupported;
		OutResult.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("No Device backend is available for the refresh-rate request.")
		);
		return {};
	}
	OutResult = Backend->ApplyPreferredRefreshRate(Request);
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

void FOpenMobileDeviceRefreshRateControlService::RemoveRequest(
	const FGuid& RequestId
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceRefreshRateControlServicePrivate;
	const uint64* Sequence = RequestSequences.Find(RequestId);
	if (!Sequence)
	{
		return;
	}
	const TOptional<FOpenMobilePreferredRefreshRateRequest> Before =
		Requests.GetEffectiveRequest();
	Requests.Remove(*Sequence);
	RequestSequences.Remove(RequestId);
	const TOptional<FOpenMobilePreferredRefreshRateRequest> After =
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
		ClearNativePreference();
	}
}

int32 FOpenMobileDeviceRefreshRateControlService::GetActiveRequestCountForDiagnostics()
{
	check(IsInGameThread());
	return OpenMobileDeviceRefreshRateControlServicePrivate::RequestSequences.Num();
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileDeviceRefreshRateControlService::NotifySurfaceChangedForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceRefreshRateControlServicePrivate::HandleSurfaceChanged();
}

void FOpenMobileDeviceRefreshRateControlService::NotifyBackgroundForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceRefreshRateControlServicePrivate::HandleBackground();
}

void FOpenMobileDeviceRefreshRateControlService::NotifyForegroundForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceRefreshRateControlServicePrivate::HandleForeground();
}

void FOpenMobileDeviceRefreshRateControlService::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceRefreshRateControlServicePrivate;
	if (!Requests.IsEmpty())
	{
		ClearNativePreference();
	}
	Requests.Reset();
	RequestSequences.Reset();
	NextRequestSequence = 1;
	bInBackground = false;
}
#endif
