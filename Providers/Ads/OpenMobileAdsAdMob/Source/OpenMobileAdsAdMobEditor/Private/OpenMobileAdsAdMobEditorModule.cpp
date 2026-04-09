#include "Editor.h"
#include "Logging/MessageLog.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileAdsAdMobSettings.h"
#include "OpenMobileAdsAdMobSettingsValidator.h"

#define LOCTEXT_NAMESPACE "OpenMobileAdsAdMobEditor"

namespace OpenMobileAdsAdMobEditorPrivate
{
	const FName MessageLogName(TEXT("OpenMobileAds"));
}

class FOpenMobileAdsAdMobEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FMessageLogModule& MessageLogModule =
			FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog"));
		MessageLogModule.RegisterLogListing(
			OpenMobileAdsAdMobEditorPrivate::MessageLogName,
			LOCTEXT("MessageLogLabel", "OpenMobile Ads")
		);
		PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddRaw(
			this,
			&FOpenMobileAdsAdMobEditorModule::HandlePreBeginPIE
		);
	}

	virtual void ShutdownModule() override
	{
		FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
		if (FMessageLogModule* MessageLogModule =
			FModuleManager::GetModulePtr<FMessageLogModule>(TEXT("MessageLog")))
		{
			MessageLogModule->UnregisterLogListing(
				OpenMobileAdsAdMobEditorPrivate::MessageLogName
			);
		}
	}

private:
	void HandlePreBeginPIE(bool) const
	{
		const UOpenMobileAdsAdMobSettings* Settings =
			GetDefault<UOpenMobileAdsAdMobSettings>();
		const TArray<FString> Errors =
			FOpenMobileAdsAdMobSettingsValidator::Validate(*Settings);
		if (Errors.IsEmpty())
		{
			return;
		}

		FMessageLog MessageLog(OpenMobileAdsAdMobEditorPrivate::MessageLogName);
		MessageLog.NewPage(LOCTEXT("ValidationPage", "AdMob settings validation"));
		for (const FString& Error : Errors)
		{
			MessageLog.Error(FText::FromString(Error));
		}
		MessageLog.Notify(LOCTEXT("ValidationFailed", "AdMob settings need attention."));
	}

	FDelegateHandle PreBeginPIEHandle;
};

IMPLEMENT_MODULE(FOpenMobileAdsAdMobEditorModule, OpenMobileAdsAdMobEditor)

#undef LOCTEXT_NAMESPACE
