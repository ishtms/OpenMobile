#include "Features/IModularFeatures.h"
#include "IOS/OpenMobileAdsAdMobChartboostConsentSignalConsumer.h"
#include "Modules/ModuleManager.h"

class FOpenMobileAdsAdMobChartboostIOSModule final : public IModuleInterface
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
	FOpenMobileAdsAdMobChartboostIOSModule,
	OpenMobileAdsAdMobChartboostIOS
)
