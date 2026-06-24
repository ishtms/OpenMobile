#include "OpenMobileDeviceKeepScreenAwakeControlService.h"

#include "IOpenMobileDeviceBackend.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileDeviceBackendRegistry.h"

namespace OpenMobileDeviceKeepScreenAwakeControlServicePrivate
{
	TSet<FGuid> Requests;
	bool bStarted = false;
	bool bInBackground = false;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;
	FDelegateHandle ReactivatedHandle;
	FDelegateHandle SurfaceChangedHandle;

	void ClearNativeState()
	{
		if (IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend())
		{
			Backend->ClearKeepScreenAwake();
		}
	}

	void ReapplyNativeState()
	{
		if (bInBackground || Requests.IsEmpty())
		{
			return;
		}
		if (IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend())
		{
			Backend->ApplyKeepScreenAwake();
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
			ClearNativeState();
		}
	}

	void HandleForeground()
	{
		bInBackground = false;
		ReapplyNativeState();
	}

	void HandleSurfaceChanged()
	{
		ReapplyNativeState();
	}
}

void FOpenMobileDeviceKeepScreenAwakeControlService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceKeepScreenAwakeControlServicePrivate;
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

void FOpenMobileDeviceKeepScreenAwakeControlService::Shutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceKeepScreenAwakeControlServicePrivate;
	if (!Requests.IsEmpty() && !bInBackground)
	{
		ClearNativeState();
	}
	Requests.Reset();
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

FGuid FOpenMobileDeviceKeepScreenAwakeControlService::AddRequest(
	FOpenMobileKeepScreenAwakeResult& OutResult
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceKeepScreenAwakeControlServicePrivate;
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		OutResult = {};
		OutResult.State = EOpenMobileKeepScreenAwakeApplyState::Unsupported;
		OutResult.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("No Device backend is available for the keep-awake request.")
		);
		return {};
	}
	if (bInBackground)
	{
		OutResult = {};
		OutResult.State = EOpenMobileKeepScreenAwakeApplyState::Rejected;
		OutResult.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("Keep-awake requests cannot start while the application is backgrounded.")
		);
		return {};
	}
	if (Requests.IsEmpty())
	{
		OutResult = Backend->ApplyKeepScreenAwake();
		if (!OutResult.IsAccepted())
		{
			return {};
		}
	}
	else
	{
		OutResult = {};
		OutResult.State = EOpenMobileKeepScreenAwakeApplyState::Accepted;
		OutResult.bEffectiveKeepScreenAwake =
			FOpenMobileDeviceOptionalBool::MakeAvailable(true);
	}
	const FGuid RequestId = FGuid::NewGuid();
	Requests.Add(RequestId);
	return RequestId;
}

void FOpenMobileDeviceKeepScreenAwakeControlService::RemoveRequest(
	const FGuid& RequestId
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceKeepScreenAwakeControlServicePrivate;
	if (Requests.Remove(RequestId) == 0 || !Requests.IsEmpty())
	{
		return;
	}
	if (!bInBackground)
	{
		ClearNativeState();
	}
}

int32 FOpenMobileDeviceKeepScreenAwakeControlService::GetActiveRequestCountForDiagnostics()
{
	check(IsInGameThread());
	return OpenMobileDeviceKeepScreenAwakeControlServicePrivate::Requests.Num();
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileDeviceKeepScreenAwakeControlService::NotifySurfaceChangedForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceKeepScreenAwakeControlServicePrivate::HandleSurfaceChanged();
}

void FOpenMobileDeviceKeepScreenAwakeControlService::NotifyBackgroundForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceKeepScreenAwakeControlServicePrivate::HandleBackground();
}

void FOpenMobileDeviceKeepScreenAwakeControlService::NotifyForegroundForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceKeepScreenAwakeControlServicePrivate::HandleForeground();
}

void FOpenMobileDeviceKeepScreenAwakeControlService::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceKeepScreenAwakeControlServicePrivate;
	if (!Requests.IsEmpty() && !bInBackground)
	{
		ClearNativeState();
	}
	Requests.Reset();
	bInBackground = false;
}
#endif
