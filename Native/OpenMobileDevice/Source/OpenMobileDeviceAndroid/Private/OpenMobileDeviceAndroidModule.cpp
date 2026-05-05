#include "OpenMobileDeviceAndroidBackend.h"

#include "Modules/ModuleManager.h"
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
	}

	virtual void ShutdownModule() override
	{
		if (Backend)
		{
			FOpenMobileDeviceBackendRegistry::UnregisterBackend(*Backend);
			Backend.Reset();
		}
	}

private:
	TUniquePtr<FOpenMobileDeviceAndroidBackend> Backend;
};

IMPLEMENT_MODULE(FOpenMobileDeviceAndroidModule, OpenMobileDeviceAndroid)
