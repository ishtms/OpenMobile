#include "OpenMobileDeviceAndroidBackend.h"

#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileDeviceAndroidBattery.h"
#include "OpenMobileDeviceBackendRegistry.h"

class FOpenMobileDeviceAndroidModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileDeviceAndroidBackend>();
		if (!FOpenMobileDeviceBackendRegistry::RegisterBackend(*Backend))
		{
			Backend.Reset();
		}
		BackgroundHandle =
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddStatic(
				&ResetOpenMobileDeviceAndroidThermalHeadroomTrend
			);
		ForegroundHandle =
			FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddStatic(
				&ResetOpenMobileDeviceAndroidThermalHeadroomTrend
			);
	}

	virtual void ShutdownModule() override
	{
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
		ResetOpenMobileDeviceAndroidThermalHeadroom();
		if (Backend)
		{
			FOpenMobileDeviceBackendRegistry::UnregisterBackend(*Backend);
			Backend.Reset();
		}
	}

private:
	TUniquePtr<FOpenMobileDeviceAndroidBackend> Backend;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;
};

IMPLEMENT_MODULE(FOpenMobileDeviceAndroidModule, OpenMobileDeviceAndroid)
