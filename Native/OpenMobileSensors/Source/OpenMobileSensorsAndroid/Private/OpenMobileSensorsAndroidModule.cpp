#include "OpenMobileSensorsAndroidBackend.h"
#include "OpenMobileSensorsAndroidBridge.h"

#include "IOpenMobilePermissionProvider.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileSensorScreenRotationService.h"
#include "OpenMobileSensorsBackendRegistry.h"

class FOpenMobileSensorsAndroidModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileSensorsAndroidBackend>();
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
					&FOpenMobileSensorsAndroidModule::HandleOrientationChanged);
		FOpenMobileSensorsAndroidBridge::RefreshApplicationWindowRotation();
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
	void HandleOrientationChanged(int32 Orientation)
	{
		static_cast<void>(Orientation);
		FOpenMobileSensorsAndroidBridge::RefreshApplicationWindowRotation();
	}

	TUniquePtr<FOpenMobileSensorsAndroidBackend> Backend;
	FDelegateHandle OrientationChangedHandle;
};

IMPLEMENT_MODULE(FOpenMobileSensorsAndroidModule, OpenMobileSensorsAndroid)
