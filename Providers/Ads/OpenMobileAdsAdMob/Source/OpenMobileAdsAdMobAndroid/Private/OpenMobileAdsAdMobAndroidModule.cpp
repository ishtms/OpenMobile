#include "OpenMobileAdsAdMobAndroidBackend.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

/** Owns the AdMob Android backend while the platform-specific provider module is loaded. */
class FOpenMobileAdsAdMobAndroidModule final : public IModuleInterface
{
public:
	/** Registers the JNI backend for AdMob platform selection. */
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileAdsAdMobAndroidBackend>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsAdMobBackend::GetModularFeatureName(),
			Backend.Get()
		);
	}

	/** Shuts down Java state before unregistering and releasing the backend. */
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
