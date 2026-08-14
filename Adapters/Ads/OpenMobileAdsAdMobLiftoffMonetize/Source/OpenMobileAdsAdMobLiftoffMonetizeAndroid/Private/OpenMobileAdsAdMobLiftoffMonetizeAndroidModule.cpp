#include "Android/OpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

/** Owns Liftoff Monetize privacy delivery for the optional Android adapter. */
class FOpenMobileAdsAdMobLiftoffMonetizeAndroidModule final
	: public IModuleInterface
{
public:
	/** Registers the Liftoff consumer before AdMob gathers privacy requirements. */
	virtual void StartupModule() override
	{
		ConsentConsumer = MakeUnique<
			FOpenMobileAdsAdMobLiftoffMonetizeConsentSignalConsumer>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsConsentSignalConsumer::GetModularFeatureName(),
			ConsentConsumer.Get()
		);
	}

	/** Unregisters Liftoff before releasing the Android bridge consumer. */
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
