#include "OpenMobileAdsAdMobIOSBackend.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

/** Owns the AdMob iOS backend while the Objective-C provider module is loaded. */
class FOpenMobileAdsAdMobIOSModule final : public IModuleInterface
{
public:
	/** Registers the Objective-C backend for AdMob platform selection. */
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileAdsAdMobIOSBackend>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsAdMobBackend::GetModularFeatureName(),
			Backend.Get()
		);
	}

	/** Shuts down Google delegates before unregistering and releasing the backend. */
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
