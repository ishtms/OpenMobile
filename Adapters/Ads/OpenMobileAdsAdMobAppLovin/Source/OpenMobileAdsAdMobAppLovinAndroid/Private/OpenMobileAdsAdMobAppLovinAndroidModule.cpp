#include "Android/OpenMobileAdsAdMobAppLovinConsentSignalConsumer.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

/** Owns the Android AppLovin privacy consumer while this optional adapter module is enabled. */
class FOpenMobileAdsAdMobAppLovinAndroidModule final : public IModuleInterface
{
public:
	/** Registers AppLovin privacy delivery before AdMob discovers mediated consumers. */
	virtual void StartupModule() override
	{
		ConsentConsumer =
			MakeUnique<FOpenMobileAdsAdMobAppLovinConsentSignalConsumer>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsConsentSignalConsumer::GetModularFeatureName(),
			ConsentConsumer.Get()
		);
	}

	/** Unregisters the consumer before releasing its Android implementation. */
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
