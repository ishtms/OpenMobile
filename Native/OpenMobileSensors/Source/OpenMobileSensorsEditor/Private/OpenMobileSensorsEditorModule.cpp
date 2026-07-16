#include "Interfaces/IPluginManager.h"
#include "IMessageLogListing.h"
#include "Logging/MessageLog.h"
#include "MessageLogModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileSensorsPackagingValidation.h"
#include "OpenMobileSensorsSettings.h"
#include "Runtime/Launch/Resources/Version.h"

#define LOCTEXT_NAMESPACE "OpenMobileSensorsEditor"

namespace OpenMobileSensorsEditorPrivate
{
	const FName PackagingLogName(TEXT("OpenMobileSensorsPackaging"));
	const TCHAR* SensorsSection =
		TEXT("/Script/OpenMobileSensors.OpenMobileSensorsSettings");

	void ApplyPackagingSettings(
		const FConfigFile& Config,
		UOpenMobileSensorsSettings& Settings
	)
	{
		Config.GetBool(
			SensorsSection,
			TEXT("bEnablePermissionSensitiveSensors"),
			Settings.bEnablePermissionSensitiveSensors
		);
		Config.GetBool(
			SensorsSection,
			TEXT("bAllowHighSamplingRate"),
			Settings.bAllowHighSamplingRate
		);
		Config.GetString(
			SensorsSection,
			TEXT("IOSMotionUsageDescription"),
			Settings.IOSMotionUsageDescription
		);
		Config.GetString(
			SensorsSection,
			TEXT("AndroidActivityRecognitionRationale"),
			Settings.AndroidActivityRecognitionRationale
		);
		FString DevelopmentInputMode;
		if (Config.GetString(
			SensorsSection,
			TEXT("DevelopmentInputMode"),
			DevelopmentInputMode
		))
		{
			const int64 Value = StaticEnum<
				EOpenMobileSensorsDevelopmentInputMode
			>()->GetValueByNameString(DevelopmentInputMode);
			if (Value != INDEX_NONE)
			{
				Settings.DevelopmentInputMode =
					static_cast<EOpenMobileSensorsDevelopmentInputMode>(Value);
			}
		}
	}

	FString GetPackagingPath(
		const FString& PluginDirectory,
		EOpenMobileSensorsPackagingTarget Target
	)
	{
		if (Target == EOpenMobileSensorsPackagingTarget::Android)
		{
			return FPaths::Combine(
				PluginDirectory,
				TEXT("Source/OpenMobileSensorsAndroid/Private/Android/OpenMobileSensors_Android_UPL.xml")
			);
		}
		return FPaths::Combine(
			PluginDirectory,
			TEXT("Source/OpenMobileSensorsIOS/Private/IOS/OpenMobileSensors_IOS_UPL.xml")
		);
	}

	void AppendAndroidHostFiles(TArray<FString>& OutDeclarations)
	{
		for (const TCHAR* FileName : {
			TEXT("ManifestRequirementsAdditions.txt"),
			TEXT("ManifestRequirementsOverride.txt")
		})
		{
			FString Contents;
			const FString Path = FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("Build/Android"),
				FileName
			);
			if (FFileHelper::LoadFileToString(Contents, *Path))
			{
				OutDeclarations.Add(MoveTemp(Contents));
			}
		}
	}
}

class FOpenMobileSensorsEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		using namespace OpenMobileSensorsEditorPrivate;
		FMessageLogModule& MessageLogModule =
			FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog"));
		if (!MessageLogModule.IsRegisteredLogListing(PackagingLogName))
		{
			MessageLogModule.RegisterLogListing(
				PackagingLogName,
				LOCTEXT("PackagingLog", "OpenMobile Sensors Packaging")
			);
		}
		GetMutableDefault<UOpenMobileSensorsSettings>()
			->OnSettingChanged()
			.AddRaw(this, &FOpenMobileSensorsEditorModule::HandleSettingsChanged);
		ValidateCurrentProject();
	}

	virtual void ShutdownModule() override
	{
		using namespace OpenMobileSensorsEditorPrivate;
		if (UObjectInitialized())
		{
			GetMutableDefault<UOpenMobileSensorsSettings>()
				->OnSettingChanged()
				.RemoveAll(this);
		}
		if (FModuleManager::Get().IsModuleLoaded(TEXT("MessageLog")))
		{
			FMessageLogModule& MessageLogModule =
				FModuleManager::GetModuleChecked<FMessageLogModule>(TEXT("MessageLog"));
			if (MessageLogModule.IsRegisteredLogListing(PackagingLogName))
			{
				MessageLogModule.UnregisterLogListing(PackagingLogName);
			}
		}
	}

private:
	void HandleSettingsChanged(
		UObject* SettingsObject,
		FPropertyChangedEvent& PropertyChangedEvent
	)
	{
		static_cast<void>(SettingsObject);
		static_cast<void>(PropertyChangedEvent);
		ValidateCurrentProject();
	}

	void ValidateCurrentProject() const
	{
		using namespace OpenMobileSensorsEditorPrivate;
		FMessageLogModule& MessageLogModule =
			FModuleManager::LoadModuleChecked<FMessageLogModule>(TEXT("MessageLog"));
		MessageLogModule.GetLogListing(PackagingLogName)->ClearMessages();
		FMessageLog PackagingLog(PackagingLogName);

		const UOpenMobileSensorsSettings* CurrentSettings =
			GetDefault<UOpenMobileSensorsSettings>();
		TArray<FString> SettingsErrors;
		CurrentSettings->Validate(SettingsErrors, false);
		TSet<FString> UniqueMessages;
		for (FString& Error : SettingsErrors)
		{
			UniqueMessages.Add(MoveTemp(Error));
		}

		const TSharedPtr<IPlugin> Plugin =
			IPluginManager::Get().FindPlugin(TEXT("OpenMobileSensors"));
		const FString PluginDirectory = Plugin.IsValid()
			? Plugin->GetBaseDir()
			: FString();
		for (const TPair<EOpenMobileSensorsPackagingTarget, FString>& Entry : {
			TPair<EOpenMobileSensorsPackagingTarget, FString>(
				EOpenMobileSensorsPackagingTarget::Android,
				TEXT("Android")
			),
			TPair<EOpenMobileSensorsPackagingTarget, FString>(
				EOpenMobileSensorsPackagingTarget::IOS,
				TEXT("IOS")
			)
		})
		{
			FConfigFile PlatformConfig;
			FConfigCacheIni::LoadLocalIniFile(
				PlatformConfig,
				TEXT("Engine"),
				true,
				*Entry.Value,
				true
			);
			UOpenMobileSensorsSettings* PlatformSettings =
				DuplicateObject<UOpenMobileSensorsSettings>(
					CurrentSettings,
					GetTransientPackage()
				);
			ApplyPackagingSettings(PlatformConfig, *PlatformSettings);

			FOpenMobileSensorsPackagingContext Context;
			Context.Target = Entry.Key;
			Context.bSensorsPlatformPackagingEnabled =
				!PluginDirectory.IsEmpty()
				&& FPaths::FileExists(GetPackagingPath(
					PluginDirectory,
					Entry.Key
				));
			if (Entry.Key == EOpenMobileSensorsPackagingTarget::Android)
			{
				PlatformConfig.GetArray(
					TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"),
					TEXT("ExtraPermissions"),
					Context.AndroidHostDeclarations
				);
				AppendAndroidHostFiles(Context.AndroidHostDeclarations);
			}
			else
			{
			#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 8
				Context.bIOSHostPlistFallbackRequired = true;
			#endif
				PlatformConfig.GetString(
					TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"),
					TEXT("AdditionalPlistData"),
					Context.IOSHostAdditionalPlistData
				);
			}

			TArray<FOpenMobileSensorsPackagingIssue> Issues;
			FOpenMobileSensorsPackagingValidator::Validate(
				*PlatformSettings,
				Context,
				Issues
			);
			for (FOpenMobileSensorsPackagingIssue& Issue : Issues)
			{
				UniqueMessages.Add(MoveTemp(Issue.Message));
			}
		}

		for (const FString& Message : UniqueMessages)
		{
			PackagingLog.Error(FText::FromString(Message));
		}
		if (CurrentSettings->DevelopmentInputMode !=
			EOpenMobileSensorsDevelopmentInputMode::Disabled)
		{
			PackagingLog.Warning(LOCTEXT(
				"ShippingDevelopmentInput",
				"Shipping packaging will fail until Development Input Mode is Disabled."
			));
		}
		if (!UniqueMessages.IsEmpty())
		{
			PackagingLog.Notify(
				LOCTEXT("PackagingErrors", "OpenMobile Sensors packaging settings need attention."),
				EMessageSeverity::Error
			);
		}
	}
};

IMPLEMENT_MODULE(FOpenMobileSensorsEditorModule, OpenMobileSensorsEditor)

#undef LOCTEXT_NAMESPACE
