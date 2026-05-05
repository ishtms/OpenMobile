#include "OpenMobileDeviceBackendRegistry.h"

#include "Modules/ModuleManager.h"

class FOpenMobileDeviceModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FOpenMobileDeviceBackendRegistry::Start();
	}

	virtual void ShutdownModule() override
	{
		FOpenMobileDeviceBackendRegistry::BeginShutdown();
	}
};

IMPLEMENT_MODULE(FOpenMobileDeviceModule, OpenMobileDevice)
