#include "OpenMobileHapticsIOSBackend.h"

#include "Modules/ModuleManager.h"
#include "OpenMobileHapticsBackendRegistry.h"

class FOpenMobileHapticsIOSModule final : public IModuleInterface
{
public:
	/** Creates and registers the iOS backend after Core Haptics bridge code is loaded. */
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileHapticsIOSBackend>();
		if (!FOpenMobileHapticsBackendRegistry::RegisterBackend(*Backend))
		{
			Backend.Reset();
		}
	}

	/** Shuts native callbacks down before unregistering and releasing the iOS backend. */
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
