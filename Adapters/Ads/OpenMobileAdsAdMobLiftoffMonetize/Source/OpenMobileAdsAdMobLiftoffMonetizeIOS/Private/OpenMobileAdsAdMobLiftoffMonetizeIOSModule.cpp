#include "Features/IModularFeatures.h"
#include "IOS/OpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer.h"
#include "Modules/ModuleManager.h"

class FOpenMobileAdsAdMobLiftoffMonetizeIOSModule final
	: public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		ConsentConsumer = MakeUnique<
			FOpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsConsentSignalConsumer::GetModularFeatureName(),
			ConsentConsumer.Get()
		);
	}

	virtual void ShutdownModule() override
	{
		if (ConsentConsumer)
		{
			IModularFeatures::Get().UnregisterModularFeature(
				IOpenMobileAdsConsentSignalConsumer::GetModularFeatureName(),
				ConsentConsumer.Get()
			);
			ConsentConsumer.Reset();
		}
	}

private:
	TUniquePtr<FOpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer>
		ConsentConsumer;
};

IMPLEMENT_MODULE(
	FOpenMobileAdsAdMobLiftoffMonetizeIOSModule,
	OpenMobileAdsAdMobLiftoffMonetizeIOS
)
