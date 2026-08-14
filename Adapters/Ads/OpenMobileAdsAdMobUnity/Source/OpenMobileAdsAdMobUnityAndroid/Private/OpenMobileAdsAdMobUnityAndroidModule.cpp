#include "Android/OpenMobileAdsAdMobUnityConsentSignalConsumer.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

/** Owns Unity Ads privacy metadata delivery for the Android mediation adapter. */
class FOpenMobileAdsAdMobUnityAndroidModule final : public IModuleInterface
{
public:
	/** Registers Unity Ads as an AdMob consent signal consumer. */
	virtual void StartupModule() override
	{
		ConsentConsumer =
			MakeUnique<FOpenMobileAdsAdMobUnityConsentSignalConsumer>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsConsentSignalConsumer::GetModularFeatureName(),
			ConsentConsumer.Get()
		);
	}

	/** Unregisters Unity Ads before releasing its Android metadata bridge. */
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
	FOpenMobileAdsAdMobUnityAndroidModule,
	OpenMobileAdsAdMobUnityAndroid
)
