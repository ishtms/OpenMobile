#include "Android/OpenMobileAdsAdMobAppLovinConsentSignalConsumer.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

class FOpenMobileAdsAdMobAppLovinAndroidModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		ConsentConsumer =
			MakeUnique<FOpenMobileAdsAdMobAppLovinConsentSignalConsumer>();
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
	TUniquePtr<FOpenMobileAdsAdMobAppLovinConsentSignalConsumer> ConsentConsumer;
};

IMPLEMENT_MODULE(
	FOpenMobileAdsAdMobAppLovinAndroidModule,
	OpenMobileAdsAdMobAppLovinAndroid
)
