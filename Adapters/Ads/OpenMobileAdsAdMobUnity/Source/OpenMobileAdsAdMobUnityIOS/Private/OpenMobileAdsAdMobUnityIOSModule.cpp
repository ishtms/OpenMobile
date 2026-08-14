#include "Features/IModularFeatures.h"
#include "IOS/OpenMobileAdsAdMobUnityConsentSignalConsumer.h"
#include "Modules/ModuleManager.h"

/** Owns Unity Ads privacy metadata delivery for the iOS mediation adapter. */
class FOpenMobileAdsAdMobUnityIOSModule final : public IModuleInterface
{
public:
	/** Registers Unity Ads for AdMob's iOS privacy signal pass. */
	virtual void StartupModule() override
	{
		ConsentConsumer =
			MakeUnique<FOpenMobileAdsAdMobUnityConsentSignalConsumer>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsConsentSignalConsumer::GetModularFeatureName(),
			ConsentConsumer.Get()
		);
	}

	/** Unregisters Unity Ads before releasing its Objective-C consumer. */
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
	TUniquePtr<FOpenMobileAdsAdMobUnityConsentSignalConsumer> ConsentConsumer;
};

IMPLEMENT_MODULE(
	FOpenMobileAdsAdMobUnityIOSModule,
	OpenMobileAdsAdMobUnityIOS
)
