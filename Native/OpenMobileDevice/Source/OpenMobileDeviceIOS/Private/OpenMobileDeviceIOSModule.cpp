#include "OpenMobileDeviceIOSBackend.h"

#include "Modules/ModuleManager.h"
#include "OpenMobileDeviceBackendRegistry.h"

class FOpenMobileDeviceIOSModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileDeviceIOSBackend>();
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
	TUniquePtr<FOpenMobileDeviceIOSBackend> Backend;
};

IMPLEMENT_MODULE(FOpenMobileDeviceIOSModule, OpenMobileDeviceIOS)
