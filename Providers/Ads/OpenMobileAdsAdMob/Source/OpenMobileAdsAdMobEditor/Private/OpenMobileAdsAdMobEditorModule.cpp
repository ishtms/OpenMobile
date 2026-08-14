#include "Editor.h"
#include "Logging/MessageLog.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileAdsAdMobSettings.h"
#include "OpenMobileAdsAdMobSettingsValidator.h"

#define LOCTEXT_NAMESPACE "OpenMobileAdsAdMobEditor"

namespace OpenMobileAdsAdMobEditorPrivate
{
	const FName MessageLogName(TEXT("OpenMobileAds"));
}

/** Reports AdMob-specific settings failures through the shared Ads Message Log before PIE. */
class FOpenMobileAdsAdMobEditorModule final : public IModuleInterface
{
public:
	/** Loads shared Ads editor support before binding the AdMob pre-PIE validator. */
	virtual void StartupModule() override
	{
		FModuleManager::Get().LoadModuleChecked(TEXT("OpenMobileAdsEditor"));
		PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddRaw(
			this,
			&FOpenMobileAdsAdMobEditorModule::HandlePreBeginPIE
		);
	}

	/** Removes the pre-PIE hook before this provider editor module unloads. */
	virtual void ShutdownModule() override
	{
		FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
	}

private:
	/** Writes provider identifier problems to the same log used by core Ads validation. */
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
