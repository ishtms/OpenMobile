#include "OpenMobileDeviceFlashlightControlService.h"

#include "IOpenMobileDeviceBackend.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileDeviceBackendRegistry.h"

namespace OpenMobileDeviceFlashlightControlServicePrivate
{
	FGuid ActiveOperationId;
	bool bTorchOn = false;
	bool bApplicationActive = true;
	bool bStarted = false;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;
	FDelegateHandle ReactivatedHandle;

	void ClearOwnedTorch()
	{
		if (!bTorchOn)
		{
			return;
		}
		if (IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend())
		{
			Backend->ClearFlashlight();
		}
		bTorchOn = false;
	}

	void SetApplicationActive(bool bActive)
	{
		check(IsInGameThread());
		bApplicationActive = bActive;
		if (!bApplicationActive)
		{
			ActiveOperationId.Invalidate();
			ClearOwnedTorch();
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

void FOpenMobileDeviceFlashlightControlService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceFlashlightControlServicePrivate;
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

void FOpenMobileDeviceFlashlightControlService::Shutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceFlashlightControlServicePrivate;
	ActiveOperationId.Invalidate();
	ClearOwnedTorch();
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

FGuid FOpenMobileDeviceFlashlightControlService::BeginOperation(
	FOpenMobileError& OutError
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceFlashlightControlServicePrivate;
	OutError = {};
	if (FOpenMobileDeviceBackendRegistry::IsShuttingDown()
		|| !bApplicationActive)
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("Flashlight operations cannot start while the application is inactive or shutting down.")
		);
		return {};
	}
	if (ActiveOperationId.IsValid())
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::Busy,
			TEXT("Another flashlight operation is already in progress.")
		);
		return {};
	}
	ActiveOperationId = FGuid::NewGuid();
	return ActiveOperationId;
}

bool FOpenMobileDeviceFlashlightControlService::IsOperationCurrent(
	const FGuid& OperationId
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceFlashlightControlServicePrivate;
	return bApplicationActive && OperationId.IsValid()
		&& OperationId == ActiveOperationId;
}

void FOpenMobileDeviceFlashlightControlService::CompleteOperation(
	const FGuid& OperationId,
	const FOpenMobileFlashlightOperationResult& Result
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceFlashlightControlServicePrivate;
	if (OperationId != ActiveOperationId)
	{
		return;
	}
	ActiveOperationId.Invalidate();
	if (!Result.IsApplied())
	{
		return;
	}
	bTorchOn = Result.EffectiveTorchState
		== EOpenMobileFlashlightTorchState::On
		|| (Result.EffectiveTorchState
			== EOpenMobileFlashlightTorchState::Unknown
			&& Result.Request.Operation
				!= EOpenMobileFlashlightOperation::Off);
}

void FOpenMobileDeviceFlashlightControlService::CancelOperation(
	const FGuid& OperationId
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceFlashlightControlServicePrivate;
	if (OperationId == ActiveOperationId)
	{
		ActiveOperationId.Invalidate();
	}
}

void FOpenMobileDeviceFlashlightControlService::HandleGameInstanceTeardown()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceFlashlightControlServicePrivate;
	ActiveOperationId.Invalidate();
	ClearOwnedTorch();
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileDeviceFlashlightControlService::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceFlashlightControlServicePrivate;
	ActiveOperationId.Invalidate();
	ClearOwnedTorch();
	bApplicationActive = true;
}

void FOpenMobileDeviceFlashlightControlService::SetApplicationActiveForTests(
	bool bActive
)
{
	OpenMobileDeviceFlashlightControlServicePrivate::SetApplicationActive(
		bActive
	);
}

bool FOpenMobileDeviceFlashlightControlService::IsTorchOnForTests()
{
	return OpenMobileDeviceFlashlightControlServicePrivate::bTorchOn;
}
#endif
