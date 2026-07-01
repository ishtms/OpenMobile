#include "Android/OpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

class FOpenMobileAdsAdMobLiftoffMonetizeAndroidModule final
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
	FOpenMobileAdsAdMobLiftoffMonetizeAndroidModule,
	OpenMobileAdsAdMobLiftoffMonetizeAndroid
)
