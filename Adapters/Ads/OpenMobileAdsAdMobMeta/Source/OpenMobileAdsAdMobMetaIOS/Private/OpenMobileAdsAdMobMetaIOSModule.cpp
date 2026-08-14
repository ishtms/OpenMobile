#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsInitializationParticipant.h"
#include "IOS/OpenMobileAdsAdMobMetaIOSInitializationParticipant.h"
#include "Modules/ModuleManager.h"

/** Owns Meta's pre-initialization tracking participant for the optional iOS adapter. */
class FOpenMobileAdsAdMobMetaIOSModule final : public IModuleInterface
{
public:
	/** Registers Meta setup so AdMob runs it before Google SDK initialization. */
	virtual void StartupModule() override
	{
		Participant =
			MakeUnique<FOpenMobileAdsAdMobMetaIOSInitializationParticipant>();
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileAdsInitializationParticipant::GetModularFeatureName(),
			Participant.Get()
		);
	}

	/** Removes Meta's participant before releasing its implementation object. */
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
