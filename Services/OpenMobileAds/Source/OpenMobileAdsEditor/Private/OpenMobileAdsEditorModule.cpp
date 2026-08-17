#include "Editor.h"
#include "BlueprintCompilationManager.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/Blueprint.h"
#include "Features/IModularFeatures.h"
#include "IDetailCustomization.h"
#include "IOpenMobileAdsProvider.h"
#include "Interfaces/IProjectManager.h"
#include "Logging/MessageLog.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsBlueprintCompilerExtension.h"
#include "OpenMobileAdsPlacementCustomization.h"
#include "PropertyEditorModule.h"
#include "ProjectDescriptor.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "OpenMobileAdsEditor"

namespace OpenMobileAdsEditorPrivate
{
	const FName MessageLogName(TEXT("OpenMobileAds"));

	TArray<EOpenMobileAdsPlatform> GetProjectMobilePlatforms()
	{
		const FProjectDescriptor* Project =
			IProjectManager::Get().GetCurrentProject();
		if (!Project || Project->TargetPlatforms.IsEmpty())
		{
			return {
				EOpenMobileAdsPlatform::Android,
				EOpenMobileAdsPlatform::IOS
			};
		}
		TArray<EOpenMobileAdsPlatform> Platforms;
		for (const FName Target : Project->TargetPlatforms)
		{
			const FString Name = Target.ToString();
			if (Name.Equals(TEXT("Android"), ESearchCase::IgnoreCase))
			{
				Platforms.AddUnique(EOpenMobileAdsPlatform::Android);
			}
			else if (Name.Equals(TEXT("IOS"), ESearchCase::IgnoreCase))
			{
				Platforms.AddUnique(EOpenMobileAdsPlatform::IOS);
			}
		}
		return Platforms;
	}

	/** Resolves the configured provider without forcing an optional provider module to load. */
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

	/** Combines portable and selected-provider checks for editor settings feedback. */
	TArray<FOpenMobileAdsConfigurationIssue> ValidateSettings(
		const UOpenMobileAdsSettings& Settings
	)
	{
		TArray<FOpenMobileAdsConfigurationIssue> Issues =
			FOpenMobileAdsConfigurationValidator::ValidateSettingsForPlatforms(
				Settings,
				false,
				GetProjectMobilePlatforms()
			);
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

/** Adds live Ads validation under Project Settings while preserving Unreal's ordinary details layout. */
class FOpenMobileAdsSettingsCustomization final : public IDetailCustomization
{
public:
	/** Disconnects the settings delegate before the customization is released. */
	virtual ~FOpenMobileAdsSettingsCustomization() override
	{
		if (Settings.IsValid() && SettingsChangedHandle.IsValid())
		{
			Settings->OnSettingChanged().Remove(SettingsChangedHandle);
		}
	}

	/** Creates one customization instance per details panel. */
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FOpenMobileAdsSettingsCustomization>();
	}

	/** Adds the current validation result and refreshes it when settings change. */
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
		ValidationCategory.AddCustomRow(
			LOCTEXT("ProviderSelectionSearch", "Provider Selection")
		)
			.NameContent()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("ProviderSelectionLabel", "Provider Selection"))
			]
			.ValueContent()
			.MinDesiredWidth(320.0f)
			[
				SNew(STextBlock)
					.Text(
						this,
						&FOpenMobileAdsSettingsCustomization::GetProviderSelectionText
					)
					.AutoWrapText(true)
			];
	}

private:
	/** Re-runs validation after any Ads setting changes in the details panel. */
	void HandleSettingsChanged(UObject*, FPropertyChangedEvent&)
	{
		RefreshValidation();
	}

	/** Rebuilds cached issues from the currently edited settings object. */
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

	/** Formats the cached issue list without hiding warnings behind errors. */
	FText GetValidationText() const
	{
		return ValidationText;
	}

	/** Uses error, warning, or success color from the highest cached severity. */
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

	FText GetProviderSelectionText() const
	{
		if (!Settings.IsValid())
		{
			return LOCTEXT("ProviderUnavailable", "Provider selection is unavailable.");
		}
		if (!Settings->PreferredProvider.IsNone())
		{
			return FText::Format(
				LOCTEXT("ConfiguredProvider", "Configured provider: {0}"),
				FText::FromName(Settings->PreferredProvider)
			);
		}
		const TArray<IOpenMobileAdsProvider*> Providers =
			IModularFeatures::Get().GetModularFeatureImplementations<
				IOpenMobileAdsProvider
			>(IOpenMobileAdsProvider::GetModularFeatureName());
		if (Providers.Num() == 1 && Providers[0])
		{
			return FText::Format(
				LOCTEXT("AutomaticProvider", "Automatic provider: {0}"),
				FText::FromName(Providers[0]->GetProviderName())
			);
		}
		return Providers.IsEmpty()
			? LOCTEXT("NoAutomaticProvider", "Automatic provider: none registered")
			: LOCTEXT("AmbiguousAutomaticProvider", "Automatic provider: unresolved; choose a preferred provider");
	}

	TWeakObjectPtr<UOpenMobileAdsSettings> Settings;
	FDelegateHandle SettingsChangedHandle;
	FText ValidationText;
	bool bHasErrors = false;
	bool bHasWarnings = false;
};

/** Registers Ads settings validation and reports the same issues before PIE starts. */
class FOpenMobileAdsEditorModule final : public IModuleInterface
{
public:
	/** Registers Message Log, settings customization, and the pre-PIE validation hook. */
	virtual void StartupModule() override
	{
		CompilerExtension.Reset(
			NewObject<UOpenMobileAdsBlueprintCompilerExtension>()
		);
		FBlueprintCompilationManager::RegisterCompilerExtension(
			UBlueprint::StaticClass(),
			CompilerExtension.Get()
		);
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
		PropertyEditorModule.RegisterCustomPropertyTypeLayout(
			FOpenMobileAdsPlacementSettings::StaticStruct()->GetFName(),
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(
				&FOpenMobileAdsPlacementCustomization::MakeInstance
			)
		);
		PropertyEditorModule.RegisterCustomPropertyTypeLayout(
			FOpenMobileAdsPlatformPlacementOverride::StaticStruct()->GetFName(),
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(
				&FOpenMobileAdsPlatformPlacementCustomization::MakeInstance
			)
		);
		PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddRaw(
			this,
			&FOpenMobileAdsEditorModule::HandlePreBeginPIE
		);
	}

	/** Removes editor registrations only while their owning modules are still available. */
	virtual void ShutdownModule() override
	{
		FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
		if (FPropertyEditorModule* PropertyEditorModule =
			FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
		{
			PropertyEditorModule->UnregisterCustomClassLayout(
				UOpenMobileAdsSettings::StaticClass()->GetFName()
			);
			PropertyEditorModule->UnregisterCustomPropertyTypeLayout(
				FOpenMobileAdsPlacementSettings::StaticStruct()->GetFName()
			);
			PropertyEditorModule->UnregisterCustomPropertyTypeLayout(
				FOpenMobileAdsPlatformPlacementOverride::StaticStruct()->GetFName()
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
	/** Stops PIE startup feedback at the editor log without changing project settings. */
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
	TStrongObjectPtr<UOpenMobileAdsBlueprintCompilerExtension> CompilerExtension;
};

IMPLEMENT_MODULE(FOpenMobileAdsEditorModule, OpenMobileAdsEditor)

#undef LOCTEXT_NAMESPACE
