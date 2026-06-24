#include "OpenMobileDeviceOrientationControlService.h"

#include "IOpenMobileDeviceBackend.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceOrientationControlPolicy.h"

namespace OpenMobileDeviceOrientationControlServicePrivate
{
	FOpenMobileDeviceOrientationRequestStack Requests;
	TMap<FGuid, uint64> RequestSequences;
	uint64 NextRequestSequence = 1;
	bool bStarted = false;
	bool bInBackground = false;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;
	FDelegateHandle ReactivatedHandle;
	FDelegateHandle SurfaceChangedHandle;

	void ClearNativePolicy()
	{
		if (IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend())
		{
			Backend->ClearOrientationPolicy();
		}
	}

	void ReapplyEffectiveRequest()
	{
		if (bInBackground)
		{
			return;
		}
		const TOptional<FOpenMobileOrientationPolicyRequest> Effective =
			Requests.GetEffectiveRequest();
		if (!Effective.IsSet())
		{
			ClearNativePolicy();
			return;
		}
		if (IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend())
		{
			Backend->ApplyOrientationPolicy(Effective.GetValue());
		}
	}

	void HandleBackground()
	{
		bInBackground = true;
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

void FOpenMobileDeviceOrientationControlService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceOrientationControlServicePrivate;
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

void FOpenMobileDeviceOrientationControlService::Shutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceOrientationControlServicePrivate;
	if (!Requests.IsEmpty())
	{
		ClearNativePolicy();
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

FGuid FOpenMobileDeviceOrientationControlService::AddRequest(
	const FOpenMobileOrientationPolicyRequest& Request,
	FOpenMobileOrientationPolicyResult& OutResult
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceOrientationControlServicePrivate;
	FOpenMobileError ValidationError;
	if (!FOpenMobileDeviceOrientationControlPolicy::Validate(
		Request,
		ValidationError
	))
	{
		OutResult = {};
		OutResult.Request = Request;
		OutResult.State = EOpenMobileOrientationPolicyApplyState::Rejected;
		OutResult.Error = MoveTemp(ValidationError);
		return {};
	}
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		OutResult = {};
		OutResult.Request = Request;
		OutResult.State = EOpenMobileOrientationPolicyApplyState::Unsupported;
		OutResult.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("No Device backend is available for the orientation request.")
		);
		return {};
	}
	OutResult = Backend->ApplyOrientationPolicy(Request);
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

void FOpenMobileDeviceOrientationControlService::RemoveRequest(
	const FGuid& RequestId
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceOrientationControlServicePrivate;
	const uint64* Sequence = RequestSequences.Find(RequestId);
	if (!Sequence)
	{
		return;
	}
	const TOptional<FOpenMobileOrientationPolicyRequest> Before =
		Requests.GetEffectiveRequest();
	Requests.Remove(*Sequence);
	RequestSequences.Remove(RequestId);
	const TOptional<FOpenMobileOrientationPolicyRequest> After =
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
		ClearNativePolicy();
	}
}

int32 FOpenMobileDeviceOrientationControlService::GetActiveRequestCountForDiagnostics()
{
	check(IsInGameThread());
	return OpenMobileDeviceOrientationControlServicePrivate::RequestSequences.Num();
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileDeviceOrientationControlService::NotifySurfaceChangedForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceOrientationControlServicePrivate::HandleSurfaceChanged();
}

void FOpenMobileDeviceOrientationControlService::NotifyBackgroundForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceOrientationControlServicePrivate::HandleBackground();
}

void FOpenMobileDeviceOrientationControlService::NotifyForegroundForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceOrientationControlServicePrivate::HandleForeground();
}

void FOpenMobileDeviceOrientationControlService::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceOrientationControlServicePrivate;
	if (!Requests.IsEmpty())
	{
		ClearNativePolicy();
	}
	Requests.Reset();
	RequestSequences.Reset();
	NextRequestSequence = 1;
	bInBackground = false;
}
#endif
