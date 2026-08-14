#include "Features/IModularFeatures.h"
#include "IOS/OpenMobileAdsAdMobChartboostConsentSignalConsumer.h"
#include "Modules/ModuleManager.h"

/** Owns Chartboost's iOS privacy consumer for the lifetime of its optional adapter module. */
class FOpenMobileAdsAdMobChartboostIOSModule final : public IModuleInterface
{
public:
	/** Publishes Chartboost signal delivery to the AdMob provider before startup. */
	virtual void StartupModule() override
	{
		ConsentConsumer =
			MakeUnique<FOpenMobileAdsAdMobChartboostConsentSignalConsumer>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsConsentSignalConsumer::GetModularFeatureName(),
			ConsentConsumer.Get()
		);
	}

	/** Removes Chartboost from modular discovery before releasing the consumer. */
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
