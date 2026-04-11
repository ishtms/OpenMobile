#include "Editor.h"
#include "Features/IModularFeatures.h"
#include "IOpenMobileAdsProvider.h"
#include "Logging/MessageLog.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileAdsConfiguration.h"

#define LOCTEXT_NAMESPACE "OpenMobileAdsEditor"

namespace OpenMobileAdsEditorPrivate
{
	const FName MessageLogName(TEXT("OpenMobileAds"));

	IOpenMobileAdsProvider* FindConfiguredProvider(
		const UOpenMobileAdsSettings& Settings
	)
	{
		const TArray<IOpenMobileAdsProvider*> Providers =
			IModularFeatures::Get().GetModularFeatureImplementations<IOpenMobileAdsProvider>(
				IOpenMobileAdsProvider::GetModularFeatureName()
			);
		if (!Settings.PreferredProvider.IsNone())
		{
			for (IOpenMobileAdsProvider* Provider : Providers)
			{
				if (Provider && Provider->GetProviderName() == Settings.PreferredProvider)
				{
					return Provider;
				}
			}
			return nullptr;
		}
		return Providers.Num() == 1 ? Providers[0] : nullptr;
	}
}

class FOpenMobileAdsEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FMessageLogModule& MessageLogModule =
			FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog"));
		MessageLogModule.RegisterLogListing(
			OpenMobileAdsEditorPrivate::MessageLogName,
			LOCTEXT("MessageLogLabel", "OpenMobile Ads")
		);
		PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddRaw(
			this,
			&FOpenMobileAdsEditorModule::HandlePreBeginPIE
		);
	}

	virtual void ShutdownModule() override
	{
		FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
		if (FMessageLogModule* MessageLogModule =
			FModuleManager::GetModulePtr<FMessageLogModule>(TEXT("MessageLog")))
		{
			MessageLogModule->UnregisterLogListing(
				OpenMobileAdsEditorPrivate::MessageLogName
			);
		}
	}

private:
	void HandlePreBeginPIE(bool) const
	{
		const UOpenMobileAdsSettings* Settings = GetDefault<UOpenMobileAdsSettings>();
		TArray<FOpenMobileAdsConfigurationIssue> Issues =
			FOpenMobileAdsConfigurationValidator::Validate(Settings->Placements);
		if (IOpenMobileAdsProvider* Provider =
			OpenMobileAdsEditorPrivate::FindConfiguredProvider(*Settings))
		{
			Issues.Append(
				FOpenMobileAdsConfigurationValidator::ValidateProviderCapabilities(
					Settings->Placements,
					Provider->GetCapabilities()
				)
			);
		}
		if (Issues.IsEmpty())
		{
			return;
		}

		FMessageLog MessageLog(OpenMobileAdsEditorPrivate::MessageLogName);
		MessageLog.NewPage(LOCTEXT("ValidationPage", "Placement settings validation"));
		for (const FOpenMobileAdsConfigurationIssue& Issue : Issues)
		{
			const FText Message = FText::FromString(Issue.Message);
			if (Issue.Severity == EOpenMobileAdsConfigurationIssueSeverity::Error)
			{
				MessageLog.Error(Message);
			}
			else
			{
				MessageLog.Warning(Message);
			}
		}
		MessageLog.Notify(LOCTEXT("ValidationFailed", "Ads placement settings need attention."));
	}

	FDelegateHandle PreBeginPIEHandle;
};

IMPLEMENT_MODULE(FOpenMobileAdsEditorModule, OpenMobileAdsEditor)

#undef LOCTEXT_NAMESPACE
