#include "OpenMobileSensorsIOSBackend.h"

#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "IOpenMobilePermissionProvider.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileSensorScreenRotationService.h"
#include "OpenMobileSensorsBackendRegistry.h"

class FOpenMobileSensorsIOSModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileSensorsIOSBackend>();
		if (!FOpenMobileSensorsBackendRegistry::RegisterBackend(*Backend))
		{
			Backend.Reset();
			return;
		}
		if (!FOpenMobilePermissionProviderRegistry::RegisterProvider(*Backend))
		{
			FOpenMobileSensorsBackendRegistry::UnregisterBackend(*Backend);
			Backend.Reset();
			return;
		}
		OrientationChangedHandle =
			FCoreDelegates::
				ApplicationReceivedScreenOrientationChangedNotificationDelegate
				.AddRaw(this,
					&FOpenMobileSensorsIOSModule::HandleOrientationChanged);
		CaptureOrientation(FPlatformMisc::GetDeviceOrientation());
	}

	virtual void ShutdownModule() override
	{
		if (OrientationChangedHandle.IsValid())
		{
			FCoreDelegates::
				ApplicationReceivedScreenOrientationChangedNotificationDelegate
				.Remove(OrientationChangedHandle);
			OrientationChangedHandle.Reset();
		}
		FOpenMobileSensorsScreenRotationService::RemoveOwner(FGuid());
		if (Backend)
		{
			FOpenMobilePermissionProviderRegistry::UnregisterProvider(*Backend);
			FOpenMobileSensorsBackendRegistry::UnregisterBackend(*Backend);
			Backend.Reset();
		}
	}

private:
	void CaptureOrientation(EDeviceScreenOrientation Orientation)
	{
		FOpenMobileSensorsScreenRotationService::
			CapturePlatformScreenOrientation(
				Orientation,
				false,
				FPlatformTime::Seconds());
	}

	void HandleOrientationChanged(int32 Orientation)
	{
		CaptureOrientation(static_cast<EDeviceScreenOrientation>(Orientation));
	}

	TUniquePtr<FOpenMobileSensorsIOSBackend> Backend;
	FDelegateHandle OrientationChangedHandle;
};

IMPLEMENT_MODULE(FOpenMobileSensorsIOSModule, OpenMobileSensorsIOS)
