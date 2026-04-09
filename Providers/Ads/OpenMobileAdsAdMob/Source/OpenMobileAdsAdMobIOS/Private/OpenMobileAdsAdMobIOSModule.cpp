#include "OpenMobileAdsAdMobIOSBackend.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

class FOpenMobileAdsAdMobIOSModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileAdsAdMobIOSBackend>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsAdMobBackend::GetModularFeatureName(),
			Backend.Get()
		);
	}

	virtual void ShutdownModule() override
	{
		if (Backend)
		{
			Backend->Shutdown();
			IModularFeatures::Get().UnregisterModularFeature(
				IOpenMobileAdsAdMobBackend::GetModularFeatureName(),
				Backend.Get()
			);
			Backend.Reset();
		}
	}

private:
	TUniquePtr<FOpenMobileAdsAdMobIOSBackend> Backend;
};

IMPLEMENT_MODULE(FOpenMobileAdsAdMobIOSModule, OpenMobileAdsAdMobIOS)
