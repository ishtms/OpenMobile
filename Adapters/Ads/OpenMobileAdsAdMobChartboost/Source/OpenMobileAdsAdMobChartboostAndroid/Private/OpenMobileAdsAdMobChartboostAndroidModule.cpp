#include "Android/OpenMobileAdsAdMobChartboostConsentSignalConsumer.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

class FOpenMobileAdsAdMobChartboostAndroidModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		ConsentConsumer =
			MakeUnique<FOpenMobileAdsAdMobChartboostConsentSignalConsumer>();
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
	TUniquePtr<FOpenMobileAdsAdMobChartboostConsentSignalConsumer> ConsentConsumer;
};

IMPLEMENT_MODULE(
	FOpenMobileAdsAdMobChartboostAndroidModule,
	OpenMobileAdsAdMobChartboostAndroid
)
