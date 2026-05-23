#include "OpenMobileHapticsBackendRegistry.h"

#include "Modules/ModuleManager.h"

class FOpenMobileHapticsModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FOpenMobileHapticsBackendRegistry::Start();
	}

	virtual void ShutdownModule() override
	{
		FOpenMobileHapticsBackendRegistry::BeginShutdown();
	}
};

IMPLEMENT_MODULE(FOpenMobileHapticsModule, OpenMobileHaptics)
