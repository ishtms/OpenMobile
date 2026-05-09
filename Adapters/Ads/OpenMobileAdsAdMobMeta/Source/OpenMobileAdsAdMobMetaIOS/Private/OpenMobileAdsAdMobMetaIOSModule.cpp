#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsInitializationParticipant.h"
#include "IOS/OpenMobileAdsAdMobMetaIOSInitializationParticipant.h"
#include "Modules/ModuleManager.h"

class FOpenMobileAdsAdMobMetaIOSModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Participant =
			MakeUnique<FOpenMobileAdsAdMobMetaIOSInitializationParticipant>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsInitializationParticipant::GetModularFeatureName(),
			Participant.Get()
		);
	}

	virtual void ShutdownModule() override
	{
		if (Participant)
		{
			IModularFeatures::Get().UnregisterModularFeature(
				IOpenMobileAdsInitializationParticipant::GetModularFeatureName(),
				Participant.Get()
			);
			Participant.Reset();
		}
	}

private:
	TUniquePtr<FOpenMobileAdsAdMobMetaIOSInitializationParticipant> Participant;
};

IMPLEMENT_MODULE(FOpenMobileAdsAdMobMetaIOSModule, OpenMobileAdsAdMobMetaIOS)
