#include "Editor.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailCustomization.h"
#include "Interfaces/IProjectManager.h"
#include "Logging/MessageLog.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsAdMobSettings.h"
#include "OpenMobileAdsAdMobSettingsValidator.h"
#include "PropertyEditorModule.h"
#include "ProjectDescriptor.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "OpenMobileAdsAdMobEditor"

namespace OpenMobileAdsAdMobEditorPrivate
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
		if (Project->TargetPlatforms.Contains(FName(TEXT("Android"))))
		{
			Platforms.Add(EOpenMobileAdsPlatform::Android);
		}
		if (Project->TargetPlatforms.Contains(FName(TEXT("IOS"))))
		{
			Platforms.Add(EOpenMobileAdsPlatform::IOS);
		}
		return Platforms;
	}

	bool UsesSampleAppId(
		const UOpenMobileAdsAdMobSettings& Settings,
		const TArray<EOpenMobileAdsPlatform>& Platforms
	)
	{
		for (const EOpenMobileAdsPlatform Platform : Platforms)
		{
			if (UOpenMobileAdsAdMobSettings::IsGoogleSampleIdentifier(
				Settings.GetAppId(Platform)
			))
			{
				return true;
			}
		}
		return false;
	}

	int32 CountMissingPlacementIds(
		const UOpenMobileAdsSettings& Settings,
		const TArray<EOpenMobileAdsPlatform>& Platforms,
		int32& OutEnabledPlacements
	)
	{
		OutEnabledPlacements = 0;
		int32 Missing = 0;
		for (const FOpenMobileAdsPlacementSettings& Placement : Settings.Placements)
		{
			for (const EOpenMobileAdsPlatform Platform : Platforms)
			{
				const FOpenMobileAdsResolvedPlacement Resolved =
					Placement.Resolve(Platform);
				if (Resolved.bEnabled)
				{
					++OutEnabledPlacements;
					Missing += Resolved.AdUnitId.IsEmpty() ? 1 : 0;
				}
			}
		}
		return Missing;
	}
}

class FOpenMobileAdsAdMobSettingsCustomization final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FOpenMobileAdsAdMobSettingsCustomization>();
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		TArray<TWeakObjectPtr<UObject>> Objects;
		DetailBuilder.GetObjectsBeingCustomized(Objects);
		Settings = Objects.Num() == 1
			? Cast<UOpenMobileAdsAdMobSettings>(Objects[0].Get())
			: nullptr;

		IDetailCategoryBuilder& ValidationCategory = DetailBuilder.EditCategory(
			TEXT("Validation"),
			LOCTEXT("ValidationCategory", "Validation"),
			ECategoryPriority::Important
		);
		ValidationCategory.AddCustomRow(LOCTEXT("ValidationSearch", "Validation"))
			.WholeRowContent()
			[
				SNew(STextBlock)
					.Text(this, &FOpenMobileAdsAdMobSettingsCustomization::GetValidationText)
					.ColorAndOpacity(
						this,
						&FOpenMobileAdsAdMobSettingsCustomization::GetValidationColor
					)
					.AutoWrapText(true)
			];
	}

private:
	FText GetValidationText() const
	{
		if (!Settings.IsValid())
		{
			return LOCTEXT("ValidationUnavailable", "AdMob validation is unavailable.");
		}
		const UOpenMobileAdsSettings* AdsSettings =
			GetDefault<UOpenMobileAdsSettings>();
		const TArray<EOpenMobileAdsPlatform> Platforms =
			OpenMobileAdsAdMobEditorPrivate::GetProjectMobilePlatforms();
		TArray<FString> Errors =
			FOpenMobileAdsAdMobSettingsValidator::ValidateForPlatforms(
				*Settings,
				Platforms
			);
		if (!AdsSettings->IsDevelopmentTestModeEnabled()
			&& OpenMobileAdsAdMobEditorPrivate::UsesSampleAppId(
				*Settings,
				Platforms
			))
		{
			Errors.Add(TEXT("Development/Test Mode is off, but a current-target app ID is a Google sample."));
		}
		int32 EnabledPlacementCount = 0;
		const int32 MissingPlacementCount =
			OpenMobileAdsAdMobEditorPrivate::CountMissingPlacementIds(
				*AdsSettings,
				Platforms,
				EnabledPlacementCount
			);
		FString Message;
		for (const FString& Error : Errors)
		{
			if (!Message.IsEmpty())
			{
				Message.AppendChar(TEXT('\n'));
			}
			Message.Append(Error);
		}
		if (!Message.IsEmpty())
		{
			Message.AppendChar(TEXT('\n'));
		}
		if (AdsSettings->IsDevelopmentTestModeEnabled()
			&& AdsSettings->bUseOfficialTestAdUnitIds)
		{
			Message.Append(TEXT("Development/Test Mode source: official Google test ID for each requested format."));
		}
		else
		{
			Message.Append(FString::Printf(
				TEXT("Production source: named OpenMobile Ads placements provide %d current-target ad-unit IDs; %d are missing."),
				EnabledPlacementCount - MissingPlacementCount,
				MissingPlacementCount
			));
		}
		if (AdsSettings->bEnableTrackingAuthorization
			&& Platforms.Contains(EOpenMobileAdsPlatform::IOS))
		{
			Message.Append(TEXT("\nUE 5.8 direct Xcode builds must mirror the tracking usage description in iOS Additional Plist Data."));
		}
		return FText::FromString(MoveTemp(Message));
	}

	FSlateColor GetValidationColor() const
	{
		if (!Settings.IsValid())
		{
			return FLinearColor(0.9f, 0.2f, 0.2f);
		}
		const TArray<EOpenMobileAdsPlatform> Platforms =
			OpenMobileAdsAdMobEditorPrivate::GetProjectMobilePlatforms();
		const UOpenMobileAdsSettings* AdsSettings =
			GetDefault<UOpenMobileAdsSettings>();
		int32 EnabledPlacementCount = 0;
		const bool bMissingProductionId =
			!AdsSettings->IsDevelopmentTestModeEnabled()
			&& OpenMobileAdsAdMobEditorPrivate::CountMissingPlacementIds(
				*AdsSettings,
				Platforms,
				EnabledPlacementCount
			) > 0;
		if (!FOpenMobileAdsAdMobSettingsValidator::ValidateForPlatforms(
				*Settings,
				Platforms
			).IsEmpty()
			|| bMissingProductionId
			|| (!AdsSettings->IsDevelopmentTestModeEnabled()
				&& OpenMobileAdsAdMobEditorPrivate::UsesSampleAppId(
					*Settings,
					Platforms
				)))
		{
			return FLinearColor(0.9f, 0.2f, 0.2f);
		}
		if (AdsSettings->bEnableTrackingAuthorization
			&& Platforms.Contains(EOpenMobileAdsPlatform::IOS))
		{
			return FLinearColor(1.0f, 0.65f, 0.1f);
		}
		return FLinearColor(0.2f, 0.75f, 0.35f);
	}

	TWeakObjectPtr<UOpenMobileAdsAdMobSettings> Settings;
};

/** Reports AdMob-specific settings failures through the shared Ads Message Log before PIE. */
class FOpenMobileAdsAdMobEditorModule final : public IModuleInterface
{
public:
	/** Loads shared Ads editor support before binding the AdMob pre-PIE validator. */
	virtual void StartupModule() override
	{
		FModuleManager::Get().LoadModuleChecked(TEXT("OpenMobileAdsEditor"));
		FPropertyEditorModule& PropertyEditorModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		PropertyEditorModule.RegisterCustomClassLayout(
			UOpenMobileAdsAdMobSettings::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(
				&FOpenMobileAdsAdMobSettingsCustomization::MakeInstance
			)
		);
		PreBeginPIEHandle = FEditorDelegates::PreBeginPIE.AddRaw(
			this,
			&FOpenMobileAdsAdMobEditorModule::HandlePreBeginPIE
		);
	}

	/** Removes the pre-PIE hook before this provider editor module unloads. */
	virtual void ShutdownModule() override
	{
		FEditorDelegates::PreBeginPIE.Remove(PreBeginPIEHandle);
		if (FPropertyEditorModule* PropertyEditorModule =
			FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
		{
			PropertyEditorModule->UnregisterCustomClassLayout(
				UOpenMobileAdsAdMobSettings::StaticClass()->GetFName()
			);
		}
	}

private:
	/** Writes provider identifier problems to the same log used by core Ads validation. */
	void HandlePreBeginPIE(bool) const
	{
		const UOpenMobileAdsAdMobSettings* Settings =
			GetDefault<UOpenMobileAdsAdMobSettings>();
		const TArray<EOpenMobileAdsPlatform> Platforms =
			OpenMobileAdsAdMobEditorPrivate::GetProjectMobilePlatforms();
		TArray<FString> Errors =
			FOpenMobileAdsAdMobSettingsValidator::ValidateForPlatforms(
				*Settings,
				Platforms
			);
		const UOpenMobileAdsSettings* AdsSettings =
			GetDefault<UOpenMobileAdsSettings>();
		if (!AdsSettings->IsDevelopmentTestModeEnabled()
			&& OpenMobileAdsAdMobEditorPrivate::UsesSampleAppId(
				*Settings,
				Platforms
			))
		{
			Errors.Add(TEXT("Development/Test Mode is off, but a current-target app ID is a Google sample."));
		}
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
