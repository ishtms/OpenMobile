#include "OpenMobileMediaAndroidBackend.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

class FOpenMobileMediaAndroidModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileMediaAndroidBackend>();
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
	TUniquePtr<FOpenMobileMediaAndroidBackend> Backend;
};

IMPLEMENT_MODULE(FOpenMobileMediaAndroidModule, OpenMobileMediaAndroid)
