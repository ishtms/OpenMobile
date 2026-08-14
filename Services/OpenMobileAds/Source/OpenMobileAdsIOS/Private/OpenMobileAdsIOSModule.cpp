#include "IOS/OpenMobileAdsIOSTrackingAuthorizationBackend.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

/** Owns Apple's tracking authorization backend without adding platform frameworks to the core Ads module. */
class FOpenMobileAdsIOSModule final : public IModuleInterface
{
public:
	/** Registers the iOS authorization backend for platform-neutral discovery. */
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileAdsIOSTrackingAuthorizationBackend>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsTrackingAuthorizationBackend::GetModularFeatureName(),
			Backend.Get()
		);
	}

	/** Unregisters the backend before its platform object is released. */
	virtual void ShutdownModule() override
	{
		if (Backend)
		{
			IModularFeatures::Get().UnregisterModularFeature(
				IOpenMobileAdsTrackingAuthorizationBackend::GetModularFeatureName(),
				Backend.Get()
			);
			Backend.Reset();
		}
	}

private:
	TUniquePtr<FOpenMobileAdsIOSTrackingAuthorizationBackend> Backend;
};

IMPLEMENT_MODULE(FOpenMobileAdsIOSModule, OpenMobileAdsIOS)
