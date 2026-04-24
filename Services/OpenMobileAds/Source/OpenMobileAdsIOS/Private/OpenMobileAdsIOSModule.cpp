#include "IOS/OpenMobileAdsIOSTrackingAuthorizationBackend.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

class FOpenMobileAdsIOSModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Backend = MakeUnique<FOpenMobileAdsIOSTrackingAuthorizationBackend>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsTrackingAuthorizationBackend::GetModularFeatureName(),
			Backend.Get()
		);
	}

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
