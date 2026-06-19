#include "OpenMobileDeviceApplicationSettingsService.h"

#include "IOpenMobileDeviceBackend.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceCapabilities.h"
#include "OpenMobileDeviceMonitoringService.h"

namespace OpenMobileDeviceApplicationSettingsServicePrivate
{
	bool bApplicationActive = true;
	bool bAwaitingSettingsReturn = false;
	bool bStarted = false;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle DeactivatedHandle;
	FDelegateHandle ForegroundHandle;
	FDelegateHandle ReactivatedHandle;
	FOpenMobileDeviceApplicationSettingsReturned Returned;

	FOpenMobileApplicationSettingsOpenResult MakeResult(
		EOpenMobileApplicationSettingsOpenState State,
		EOpenMobileErrorCode ErrorCode,
		const TCHAR* Message
	)
	{
		FOpenMobileApplicationSettingsOpenResult Result;
		Result.State = State;
		Result.Error = FOpenMobileError::Make(ErrorCode, Message);
		return Result;
	}

	FOpenMobileApplicationSettingsOpenResult MakeUnsupported()
	{
		return MakeResult(
			EOpenMobileApplicationSettingsOpenState::Unsupported,
			EOpenMobileErrorCode::NotSupported,
			TEXT("The active Device backend does not support the application settings page.")
		);
	}

	FOpenMobileApplicationSettingsOpenResult MakeNoPresenter()
	{
		return MakeResult(
			EOpenMobileApplicationSettingsOpenState::NoPresenter,
			EOpenMobileErrorCode::Unavailable,
			TEXT("Opening application settings requires an active foreground presenter.")
		);
	}

	void Normalize(FOpenMobileApplicationSettingsOpenResult& Result)
	{
		if (Result.State == EOpenMobileApplicationSettingsOpenState::Accepted)
		{
			Result.Error = {};
			return;
		}
		if (Result.State == EOpenMobileApplicationSettingsOpenState::Unknown)
		{
			Result.State =
				EOpenMobileApplicationSettingsOpenState::NativeFailure;
		}
		if (Result.Error.IsSet())
		{
			return;
		}
		if (Result.State
			== EOpenMobileApplicationSettingsOpenState::Unsupported)
		{
			Result = MakeUnsupported();
		}
		else if (Result.State
			== EOpenMobileApplicationSettingsOpenState::NoPresenter)
		{
			Result = MakeNoPresenter();
		}
		else
		{
			Result = MakeResult(
				EOpenMobileApplicationSettingsOpenState::NativeFailure,
				EOpenMobileErrorCode::NativeFailure,
				TEXT("The platform failed to submit the application settings request.")
			);
		}
	}

	void SetApplicationActive(bool bActive)
	{
		check(IsInGameThread());
		if (bApplicationActive == bActive)
		{
			return;
		}
		bApplicationActive = bActive;
		if (!bApplicationActive || !bAwaitingSettingsReturn)
		{
			return;
		}
		bAwaitingSettingsReturn = false;
		FOpenMobileDeviceMonitoringService::RefreshActiveGroups();
		Returned.Broadcast();
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

void FOpenMobileDeviceApplicationSettingsService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceApplicationSettingsServicePrivate;
	if (bStarted)
	{
		return;
	}
	bStarted = true;
	bApplicationActive = true;
	BackgroundHandle =
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddStatic(
			&HandleBackground
		);
	DeactivatedHandle =
		FCoreDelegates::ApplicationWillDeactivateDelegate.AddStatic(
			&HandleBackground
		);
	ForegroundHandle =
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddStatic(
			&HandleForeground
		);
	ReactivatedHandle =
		FCoreDelegates::ApplicationHasReactivatedDelegate.AddStatic(
			&HandleForeground
		);
}

void FOpenMobileDeviceApplicationSettingsService::Shutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceApplicationSettingsServicePrivate;
	if (BackgroundHandle.IsValid())
	{
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(
			BackgroundHandle
		);
		BackgroundHandle.Reset();
	}
	if (DeactivatedHandle.IsValid())
	{
		FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(
			DeactivatedHandle
		);
		DeactivatedHandle.Reset();
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
	Returned.Clear();
	bApplicationActive = true;
	bAwaitingSettingsReturn = false;
	bStarted = false;
}

FOpenMobileApplicationSettingsOpenResult
FOpenMobileDeviceApplicationSettingsService::Open()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceApplicationSettingsServicePrivate;
	if (!bApplicationActive
		|| FOpenMobileDeviceBackendRegistry::IsShuttingDown())
	{
		return MakeNoPresenter();
	}
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		return MakeUnsupported();
	}
	const FOpenMobileDeviceCapability Capability = Backend->GetCapability(
		FOpenMobileDeviceCapabilityNames::OpenApplicationSettings
	);
	if (Capability.State != EOpenMobileCapabilityState::Available)
	{
		return MakeUnsupported();
	}
	FOpenMobileApplicationSettingsOpenResult Result =
		Backend->OpenApplicationSettings();
	Normalize(Result);
	if (Result.IsAccepted())
	{
		bAwaitingSettingsReturn = true;
	}
	return Result;
}

FOpenMobileDeviceApplicationSettingsReturned&
FOpenMobileDeviceApplicationSettingsService::OnReturned()
{
	return OpenMobileDeviceApplicationSettingsServicePrivate::Returned;
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileDeviceApplicationSettingsService::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceApplicationSettingsServicePrivate;
	bApplicationActive = true;
	bAwaitingSettingsReturn = false;
	Returned.Clear();
}

void FOpenMobileDeviceApplicationSettingsService::SetApplicationActiveForTests(
	bool bActive
)
{
	OpenMobileDeviceApplicationSettingsServicePrivate::SetApplicationActive(
		bActive
	);
}
#endif
