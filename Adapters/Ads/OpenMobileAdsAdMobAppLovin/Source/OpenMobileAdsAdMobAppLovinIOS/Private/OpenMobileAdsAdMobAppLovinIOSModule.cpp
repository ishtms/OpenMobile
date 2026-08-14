#include "Features/IModularFeatures.h"
#include "IOS/OpenMobileAdsAdMobAppLovinConsentSignalConsumer.h"
#include "Modules/ModuleManager.h"

/** Owns the iOS AppLovin privacy consumer only while its adapter plugin is active. */
class FOpenMobileAdsAdMobAppLovinIOSModule final : public IModuleInterface
{
public:
	/** Registers AppLovin's iOS signal consumer for AdMob startup discovery. */
	virtual void StartupModule() override
	{
		ConsentConsumer =
			MakeUnique<FOpenMobileAdsAdMobAppLovinConsentSignalConsumer>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsConsentSignalConsumer::GetModularFeatureName(),
			ConsentConsumer.Get()
		);
	}

	/** Removes the modular feature before the native consumer is destroyed. */
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
	FOpenMobileAdsAdMobAppLovinIOSModule,
	OpenMobileAdsAdMobAppLovinIOS
)
