#include "Android/OpenMobileAdsAdMobChartboostConsentSignalConsumer.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

/** Owns Chartboost's Android privacy bridge while the mediation adapter is enabled. */
class FOpenMobileAdsAdMobChartboostAndroidModule final : public IModuleInterface
{
public:
	/** Registers the Chartboost consumer before AdMob provider initialization. */
	virtual void StartupModule() override
	{
		ConsentConsumer =
			MakeUnique<FOpenMobileAdsAdMobChartboostConsentSignalConsumer>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsConsentSignalConsumer::GetModularFeatureName(),
			ConsentConsumer.Get()
		);
	}

	/** Unregisters Chartboost delivery before dropping its JNI-facing object. */
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
