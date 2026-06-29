#include "Android/OpenMobileAdsAdMobUnityConsentSignalConsumer.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

class FOpenMobileAdsAdMobUnityAndroidModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		ConsentConsumer =
			MakeUnique<FOpenMobileAdsAdMobUnityConsentSignalConsumer>();
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
	TUniquePtr<FOpenMobileAdsAdMobUnityConsentSignalConsumer> ConsentConsumer;
};

IMPLEMENT_MODULE(
	FOpenMobileAdsAdMobUnityAndroidModule,
	OpenMobileAdsAdMobUnityAndroid
)
