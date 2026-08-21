#include "OpenMobileMediaIOSBackend.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

class FOpenMobileMediaIOSModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileMediaIOSBackend>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileMediaBackend::GetModularFeatureName(),
			Backend.Get()
		);
	}

	virtual void ShutdownModule() override
	{
		if (Backend)
		{
			IModularFeatures::Get().UnregisterModularFeature(
				IOpenMobileMediaBackend::GetModularFeatureName(),
				Backend.Get()
			);
			Backend.Reset();
		}
	}

private:
	TUniquePtr<FOpenMobileMediaIOSBackend> Backend;
};

IMPLEMENT_MODULE(FOpenMobileMediaIOSModule, OpenMobileMediaIOS)
