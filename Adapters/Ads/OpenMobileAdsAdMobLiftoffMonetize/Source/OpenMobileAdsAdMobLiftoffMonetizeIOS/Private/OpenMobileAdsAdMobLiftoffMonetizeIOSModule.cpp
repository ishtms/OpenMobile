#include "Features/IModularFeatures.h"
#include "IOS/OpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer.h"
#include "Modules/ModuleManager.h"

/** Owns Liftoff Monetize privacy delivery for the optional iOS adapter. */
class FOpenMobileAdsAdMobLiftoffMonetizeIOSModule final
	: public IModuleInterface
{
public:
	/** Registers the Liftoff iOS consumer for AdMob's startup privacy pass. */
	virtual void StartupModule() override
	{
		ConsentConsumer = MakeUnique<
			FOpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsConsentSignalConsumer::GetModularFeatureName(),
			ConsentConsumer.Get()
		);
	}

	/** Removes Liftoff from modular discovery before its consumer is destroyed. */
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
