#include "OpenMobileHapticsIOSBackend.h"

#include "Modules/ModuleManager.h"
#include "OpenMobileHapticsBackendRegistry.h"

class FOpenMobileHapticsIOSModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileHapticsIOSBackend>();
		if (!FOpenMobileHapticsBackendRegistry::RegisterBackend(*Backend))
		{
			Backend.Reset();
		}
	}

	virtual void ShutdownModule() override
	{
		if (Backend)
		{
			FOpenMobileHapticsBackendRegistry::UnregisterBackend(*Backend);
			Backend.Reset();
		}
	}

private:
	TUniquePtr<FOpenMobileHapticsIOSBackend> Backend;
};

IMPLEMENT_MODULE(FOpenMobileHapticsIOSModule, OpenMobileHapticsIOS)
