#include "OpenMobileDeviceBrightnessControlService.h"

#include "IOpenMobileDeviceBackend.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceBrightnessControlPolicy.h"

namespace OpenMobileDeviceBrightnessControlServicePrivate
{
	FOpenMobileDeviceBrightnessRequestStack Requests;
	TMap<FGuid, uint64> RequestSequences;
	uint64 NextRequestSequence = 1;
	bool bStarted = false;
	bool bInBackground = false;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;
	FDelegateHandle ReactivatedHandle;
	FDelegateHandle SurfaceChangedHandle;

	void ClearNativeBrightness()
	{
		if (IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend())
		{
			Backend->ClearBrightness();
		}
	}

	void ReapplyEffectiveRequest()
	{
		if (bInBackground)
		{
			return;
		}
		const TOptional<FOpenMobileBrightnessRequest> Effective =
			Requests.GetEffectiveRequest();
		if (!Effective.IsSet())
		{
			ClearNativeBrightness();
			return;
		}
		if (IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend())
		{
			Backend->ApplyBrightness(Effective.GetValue());
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
			ClearNativeBrightness();
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

void FOpenMobileDeviceBrightnessControlService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceBrightnessControlServicePrivate;
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

void FOpenMobileDeviceBrightnessControlService::Shutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceBrightnessControlServicePrivate;
	if (!Requests.IsEmpty())
	{
		ClearNativeBrightness();
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

FGuid FOpenMobileDeviceBrightnessControlService::AddRequest(
	const FOpenMobileBrightnessRequest& Request,
	FOpenMobileBrightnessResult& OutResult
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceBrightnessControlServicePrivate;
	FOpenMobileError ValidationError;
	if (!FOpenMobileDeviceBrightnessControlPolicy::Validate(
		Request,
		ValidationError
	))
	{
		OutResult = {};
		OutResult.Request = Request;
		OutResult.State = EOpenMobileBrightnessApplyState::Rejected;
		OutResult.Error = MoveTemp(ValidationError);
		return {};
	}
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		OutResult = {};
		OutResult.Request = Request;
		OutResult.State = EOpenMobileBrightnessApplyState::Unsupported;
		OutResult.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("No Device backend is available for the brightness request.")
		);
		return {};
	}
	if (bInBackground)
	{
		OutResult = {};
		OutResult.Request = Request;
		OutResult.State = EOpenMobileBrightnessApplyState::Rejected;
		OutResult.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("Brightness overrides cannot start while the application is backgrounded.")
		);
		return {};
	}
	OutResult = Backend->ApplyBrightness(Request);
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

void FOpenMobileDeviceBrightnessControlService::RemoveRequest(
	const FGuid& RequestId
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceBrightnessControlServicePrivate;
	const uint64* Sequence = RequestSequences.Find(RequestId);
	if (!Sequence)
	{
		return;
	}
	const TOptional<FOpenMobileBrightnessRequest> Before =
		Requests.GetEffectiveRequest();
	Requests.Remove(*Sequence);
	RequestSequences.Remove(RequestId);
	const TOptional<FOpenMobileBrightnessRequest> After =
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
		ClearNativeBrightness();
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileDeviceBrightnessControlService::NotifySurfaceChangedForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceBrightnessControlServicePrivate::HandleSurfaceChanged();
}

void FOpenMobileDeviceBrightnessControlService::NotifyBackgroundForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceBrightnessControlServicePrivate::HandleBackground();
}

void FOpenMobileDeviceBrightnessControlService::NotifyForegroundForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceBrightnessControlServicePrivate::HandleForeground();
}

void FOpenMobileDeviceBrightnessControlService::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceBrightnessControlServicePrivate;
	if (!Requests.IsEmpty())
	{
		ClearNativeBrightness();
	}
	Requests.Reset();
	RequestSequences.Reset();
	NextRequestSequence = 1;
	bInBackground = false;
}
#endif
