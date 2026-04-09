#include "OpenMobileAdsAdMobAndroidBackend.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

class FOpenMobileAdsAdMobAndroidModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileAdsAdMobAndroidBackend>();
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
	TUniquePtr<FOpenMobileAdsAdMobAndroidBackend> Backend;
};

IMPLEMENT_MODULE(FOpenMobileAdsAdMobAndroidModule, OpenMobileAdsAdMobAndroid)
