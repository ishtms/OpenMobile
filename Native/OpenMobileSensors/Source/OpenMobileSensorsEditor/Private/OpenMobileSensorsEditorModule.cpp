#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IOpenMobilePermissionProvider.h"
#include "Interfaces/IPluginManager.h"
#include "IMessageLogListing.h"
#include "Logging/MessageLog.h"
#include "MessageLogModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsDevelopmentInputService.h"
#include "OpenMobileSensorsDiagnosticsScreen.h"
#include "OpenMobileSensorsEditorMockBackend.h"
#include "OpenMobileSensorsEditorMockSettings.h"
#include "OpenMobileSensorsPackagingValidation.h"
#include "OpenMobileSensorsSettings.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/Package.h"
#include "Widgets/Notifications/SNotificationList.h"

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
		GetMutableDefault<UOpenMobileSensorsEditorMockSettings>()
			->OnSettingChanged()
			.AddRaw(
				this,
				&FOpenMobileSensorsEditorModule::HandleMockSettingsChanged
			);
		MockBackend = MakeUnique<FOpenMobileSensorsEditorMockBackend>();
		UpdateDevelopmentInput();
		FOpenMobileSensorsDiagnosticsScreen::Register();
		ValidateCurrentProject();
	}

	virtual void ShutdownModule() override
	{
		using namespace OpenMobileSensorsEditorPrivate;
		FOpenMobileSensorsDiagnosticsScreen::Unregister();
		DeactivateMock();
		if (UObjectInitialized())
		{
			GetMutableDefault<UOpenMobileSensorsSettings>()
				->OnSettingChanged()
				.RemoveAll(this);
			GetMutableDefault<UOpenMobileSensorsEditorMockSettings>()
				->OnSettingChanged()
				.RemoveAll(this);
		}
		MockBackend.Reset();
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
		UpdateDevelopmentInput();
		ValidateCurrentProject();
	}

	void HandleMockSettingsChanged(
		UObject* SettingsObject,
		FPropertyChangedEvent& PropertyChangedEvent
	)
	{
		static_cast<void>(SettingsObject);
		if (!MockBackend || !MockBackend->IsActive())
		{
			return;
		}
		const UOpenMobileSensorsEditorMockSettings* Settings =
			GetDefault<UOpenMobileSensorsEditorMockSettings>();
		if (PropertyChangedEvent.GetPropertyName() ==
			GET_MEMBER_NAME_CHECKED(
				UOpenMobileSensorsEditorMockSettings,
				Preset
			))
		{
			if (Settings->Preset == EOpenMobileSensorsMockPreset::Custom)
			{
				MockBackend->ApplyInput(Settings->Input);
			}
			else
			{
				MockBackend->ApplyPreset(Settings->Preset);
			}
			return;
		}
		if (PropertyChangedEvent.GetMemberPropertyName() ==
			GET_MEMBER_NAME_CHECKED(
				UOpenMobileSensorsEditorMockSettings,
				Input
			))
		{
			MockBackend->ApplyInput(Settings->Input);
		}
	}

	void UpdateDevelopmentInput()
	{
		const UOpenMobileSensorsSettings* Settings =
			GetDefault<UOpenMobileSensorsSettings>();
		if (Settings
			&& Settings->DevelopmentInputMode ==
				EOpenMobileSensorsDevelopmentInputMode::Mock)
		{
			ActivateMock();
			return;
		}
		DeactivateMock();
	}

	void ActivateMock()
	{
		if (!MockBackend || bDevelopmentProviderRegistered)
		{
			ShowMockIndicator();
			return;
		}
		MockBackend->Activate();
		bBackendRegistered =
			FOpenMobileSensorsBackendRegistry::RegisterBackend(*MockBackend);
		if (!bBackendRegistered)
		{
			MockBackend->BeginShutdown();
			return;
		}
		bPermissionProviderRegistered =
			FOpenMobilePermissionProviderRegistry::RegisterProvider(*MockBackend);
		if (!bPermissionProviderRegistered)
		{
			FOpenMobileSensorsBackendRegistry::UnregisterBackend(*MockBackend);
			bBackendRegistered = false;
			return;
		}
		bDevelopmentProviderRegistered =
			FOpenMobileSensorsDevelopmentInputService::RegisterProvider(
				*MockBackend
			);
		if (!bDevelopmentProviderRegistered)
		{
			FOpenMobilePermissionProviderRegistry::UnregisterProvider(
				*MockBackend
			);
			FOpenMobileSensorsBackendRegistry::UnregisterBackend(*MockBackend);
			bPermissionProviderRegistered = false;
			bBackendRegistered = false;
			return;
		}

		const UOpenMobileSensorsEditorMockSettings* MockSettings =
			GetDefault<UOpenMobileSensorsEditorMockSettings>();
		if (MockSettings->Preset == EOpenMobileSensorsMockPreset::Custom)
		{
			MockBackend->ApplyInput(MockSettings->Input);
		}
		else
		{
			MockBackend->ApplyPreset(MockSettings->Preset);
		}
		ShowMockIndicator();
	}

	void DeactivateMock()
	{
		HideMockIndicator();
		if (!MockBackend)
		{
			return;
		}
		if (bDevelopmentProviderRegistered)
		{
			FOpenMobileSensorsDevelopmentInputService::UnregisterProvider(
				*MockBackend
			);
			bDevelopmentProviderRegistered = false;
		}
		if (bPermissionProviderRegistered)
		{
			FOpenMobilePermissionProviderRegistry::UnregisterProvider(
				*MockBackend
			);
			bPermissionProviderRegistered = false;
		}
		if (bBackendRegistered)
		{
			FOpenMobileSensorsBackendRegistry::UnregisterBackend(*MockBackend);
			bBackendRegistered = false;
		}
		else if (MockBackend->IsActive())
		{
			MockBackend->BeginShutdown();
		}
	}

	void ShowMockIndicator()
	{
		if (MockNotification.IsValid()
			|| !FSlateApplication::IsInitialized())
		{
			return;
		}
		FNotificationInfo Info(LOCTEXT(
			"MockActive",
			"OpenMobile sensor mocks are active"
		));
		Info.bFireAndForget = false;
		Info.bUseLargeFont = false;
		Info.bUseSuccessFailIcons = false;
		Info.ExpireDuration = 0.0f;
		MockNotification =
			FSlateNotificationManager::Get().AddNotification(Info);
		if (const TSharedPtr<SNotificationItem> Item = MockNotification.Pin())
		{
			Item->SetCompletionState(SNotificationItem::CS_Pending);
		}
	}

	void HideMockIndicator()
	{
		if (const TSharedPtr<SNotificationItem> Item = MockNotification.Pin())
		{
			Item->ExpireAndFadeout();
		}
		MockNotification.Reset();
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

	TUniquePtr<FOpenMobileSensorsEditorMockBackend> MockBackend;
	TWeakPtr<SNotificationItem> MockNotification;
	bool bBackendRegistered = false;
	bool bPermissionProviderRegistered = false;
	bool bDevelopmentProviderRegistered = false;
};

IMPLEMENT_MODULE(FOpenMobileSensorsEditorModule, OpenMobileSensorsEditor)

#undef LOCTEXT_NAMESPACE
