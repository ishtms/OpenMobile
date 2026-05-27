#include "OpenMobileHapticsAndroidBackend.h"

#include "Modules/ModuleManager.h"
#include "OpenMobileHapticsBackendRegistry.h"

class FOpenMobileHapticsAndroidModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileHapticsAndroidBackend>();
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
	TUniquePtr<FOpenMobileHapticsAndroidBackend> Backend;
};

IMPLEMENT_MODULE(FOpenMobileHapticsAndroidModule, OpenMobileHapticsAndroid)
