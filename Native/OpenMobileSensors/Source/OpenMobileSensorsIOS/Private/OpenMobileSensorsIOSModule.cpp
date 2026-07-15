#include "OpenMobileSensorsIOSBackend.h"

#include "IOpenMobilePermissionProvider.h"
#include "Modules/ModuleManager.h"
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
	TUniquePtr<FOpenMobileSensorsIOSBackend> Backend;
};

IMPLEMENT_MODULE(FOpenMobileSensorsIOSModule, OpenMobileSensorsIOS)
