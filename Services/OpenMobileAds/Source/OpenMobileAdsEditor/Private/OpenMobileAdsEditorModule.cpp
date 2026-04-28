#include "Editor.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Features/IModularFeatures.h"
#include "IDetailCustomization.h"
#include "IOpenMobileAdsProvider.h"
#include "Logging/MessageLog.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileAdsConfiguration.h"
#include "PropertyEditorModule.h"
#include "Widgets/Text/STextBlock.h"

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

	TArray<FOpenMobileAdsConfigurationIssue> ValidateSettings(
		const UOpenMobileAdsSettings& Settings
	)
	{
		TArray<FOpenMobileAdsConfigurationIssue> Issues =
			FOpenMobileAdsConfigurationValidator::ValidateSettings(Settings, false);
		if (IOpenMobileAdsProvider* Provider = FindConfiguredProvider(Settings))
		{
			Issues.Append(
				FOpenMobileAdsConfigurationValidator::ValidateProviderCapabilities(
					Settings.Placements,
					Provider->GetCapabilities(),
					Settings.PreloadPolicy.bEnabled
				)
			);
		}
		return Issues;
	}
}

class FOpenMobileAdsSettingsCustomization final : public IDetailCustomization
{
public:
	virtual ~FOpenMobileAdsSettingsCustomization() override
	{
		if (Settings.IsValid() && SettingsChangedHandle.IsValid())
		{
			Settings->OnSettingChanged().Remove(SettingsChangedHandle);
		}
	}

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FOpenMobileAdsSettingsCustomization>();
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		if (Settings.IsValid() && SettingsChangedHandle.IsValid())
		{
			Settings->OnSettingChanged().Remove(SettingsChangedHandle);
			SettingsChangedHandle.Reset();
		}
		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailBuilder.GetObjectsBeingCustomized(Objects);
		Settings = Objects.Num() == 1
			? Cast<UOpenMobileAdsSettings>(Objects[0].Get())
			: nullptr;
		RefreshValidation();
		if (Settings.IsValid())
		{
			SettingsChangedHandle = Settings->OnSettingChanged().AddSP(
				this,
				&FOpenMobileAdsSettingsCustomization::HandleSettingsChanged
			);
		}

		IDetailCategoryBuilder& ValidationCategory = DetailBuilder.EditCategory(
			TEXT("Validation"),
			LOCTEXT("ValidationCategory", "Validation"),
			ECategoryPriority::Important
		);
		ValidationCategory.AddCustomRow(LOCTEXT("ValidationSearch", "Validation"))
			.WholeRowContent()
			[
				SNew(STextBlock)
					.Text(this, &FOpenMobileAdsSettingsCustomization::GetValidationText)
					.ColorAndOpacity(this, &FOpenMobileAdsSettingsCustomization::GetValidationColor)
					.AutoWrapText(true)
			];
	}

private:
	void HandleSettingsChanged(UObject*, FPropertyChangedEvent&)
	{
		RefreshValidation();
	}

	void RefreshValidation()
	{
		ValidationText = LOCTEXT("ValidationUnavailable", "Configuration validation is unavailable.");
		bHasErrors = true;
		bHasWarnings = false;
		if (!Settings.IsValid())
		{
			return;
		}
		bHasErrors = false;

		const TArray<FOpenMobileAdsConfigurationIssue> Issues =
			OpenMobileAdsEditorPrivate::ValidateSettings(*Settings);
		if (Issues.IsEmpty())
		{
			ValidationText = LOCTEXT("ValidationPassed", "Configuration is valid.");
			return;
		}

		FString Message;
		for (const FOpenMobileAdsConfigurationIssue& Issue : Issues)
		{
			bHasErrors |= Issue.Severity ==
				EOpenMobileAdsConfigurationIssueSeverity::Error;
			bHasWarnings |= Issue.Severity ==
				EOpenMobileAdsConfigurationIssueSeverity::Warning;
			if (!Message.IsEmpty())
			{
				Message.AppendChar(TEXT('\n'));
			}
			Message.Append(Issue.Message);
		}
		ValidationText = FText::FromString(MoveTemp(Message));
	}

	FText GetValidationText() const
	{
		return ValidationText;
	}

	FSlateColor GetValidationColor() const
	{
		if (bHasErrors)
		{
			return FLinearColor(0.9f, 0.2f, 0.2f);
		}
		if (bHasWarnings)
		{
			return FLinearColor(1.0f, 0.65f, 0.1f);
		}
		return FLinearColor(0.2f, 0.75f, 0.35f);
	}

	TWeakObjectPtr<UOpenMobileAdsSettings> Settings;
	FDelegateHandle SettingsChangedHandle;
	FText ValidationText;
	bool bHasErrors = false;
	bool bHasWarnings = false;
};

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
		FPropertyEditorModule& PropertyEditorModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditorModule.RegisterCustomClassLayout(
			UOpenMobileAdsSettings::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(
				&FOpenMobileAdsSettingsCustomization::MakeInstance
			)
		);
		PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddRaw(
			this,
			&FOpenMobileAdsEditorModule::HandlePreBeginPIE
		);
	}

	virtual void ShutdownModule() override
	{
		FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
		if (FPropertyEditorModule* PropertyEditorModule =
			FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
		{
			PropertyEditorModule->UnregisterCustomClassLayout(
				UOpenMobileAdsSettings::StaticClass()->GetFName()
			);
		}
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
			OpenMobileAdsEditorPrivate::ValidateSettings(*Settings);
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
