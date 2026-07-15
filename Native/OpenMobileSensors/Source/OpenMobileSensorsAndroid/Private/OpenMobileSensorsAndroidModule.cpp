#include "OpenMobileSensorsAndroidBackend.h"

#include "IOpenMobilePermissionProvider.h"
#include "Modules/ModuleManager.h"
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
		}
	}

	virtual void ShutdownModule() override
	{
		if (Backend)
		{
			FOpenMobilePermissionProviderRegistry::UnregisterProvider(*Backend);
			FOpenMobileSensorsBackendRegistry::UnregisterBackend(*Backend);
			Backend.Reset();
		}
	}

private:
	TUniquePtr<FOpenMobileSensorsAndroidBackend> Backend;
};

IMPLEMENT_MODULE(FOpenMobileSensorsAndroidModule, OpenMobileSensorsAndroid)
