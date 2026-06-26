#include "OpenMobileDeviceBuildValidation.h"

#include "Dom/JsonObject.h"
#include "Internationalization/Regex.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenMobileDeviceSettings.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"

namespace OpenMobileDeviceBuildValidationPrivate
{
	constexpr const TCHAR* SettingsSection =
		TEXT("/Script/OpenMobileDevice.OpenMobileDeviceSettings");

	bool IsAsciiLetter(TCHAR Character)
	{
		return (Character >= TEXT('A') && Character <= TEXT('Z'))
			|| (Character >= TEXT('a') && Character <= TEXT('z'));
	}

	bool IsAsciiDigit(TCHAR Character)
	{
		return Character >= TEXT('0') && Character <= TEXT('9');
	}

	bool IsValidUrlScheme(const FString& Value)
	{
		if (Value.IsEmpty() || Value != Value.TrimStartAndEnd()
			|| !IsAsciiLetter(Value[0]))
		{
			return false;
		}
		for (int32 Index = 1; Index < Value.Len(); ++Index)
		{
			const TCHAR Character = Value[Index];
			if (!IsAsciiLetter(Character) && !IsAsciiDigit(Character)
				&& Character != TEXT('+') && Character != TEXT('-')
				&& Character != TEXT('.'))
			{
				return false;
			}
		}
		const FString Normalized = Value.ToLower();
		return Normalized != TEXT("about") && Normalized != TEXT("blob")
			&& Normalized != TEXT("content") && Normalized != TEXT("data")
			&& Normalized != TEXT("file") && Normalized != TEXT("intent")
			&& Normalized != TEXT("javascript")
			&& Normalized != TEXT("http") && Normalized != TEXT("https");
	}

	bool IsValidDottedIdentifier(
		const FString& Value,
		const int32 MaximumCharacters
	)
	{
		if (Value.IsEmpty() || Value.Len() > MaximumCharacters
			|| Value != Value.TrimStartAndEnd())
		{
			return false;
		}
		bool bAtSegmentStart = true;
		bool bHasSeparator = false;
		for (const TCHAR Character : Value)
		{
			if (Character == TEXT('.'))
			{
				if (bAtSegmentStart)
				{
					return false;
				}
				bAtSegmentStart = true;
				bHasSeparator = true;
				continue;
			}
			if (bAtSegmentStart && !IsAsciiLetter(Character))
			{
				return false;
			}
			if (!IsAsciiLetter(Character) && !IsAsciiDigit(Character)
				&& Character != TEXT('_'))
			{
				return false;
			}
			bAtSegmentStart = false;
		}
		return bHasSeparator && !bAtSegmentStart;
	}

	bool IsPrivacySafeEndpoint(const FString& Value)
	{
		if (Value.IsEmpty() || Value != Value.TrimStartAndEnd()
			|| Value.Len() > 2048
			|| !Value.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase)
			|| Value.Contains(TEXT("?")) || Value.Contains(TEXT("#")))
		{
			return false;
		}
		const int32 AuthorityStart = 8;
		int32 AuthorityEnd = Value.Find(
			TEXT("/"),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			AuthorityStart
		);
		if (AuthorityEnd == INDEX_NONE)
		{
			AuthorityEnd = Value.Len();
		}
		const FString Authority =
			Value.Mid(AuthorityStart, AuthorityEnd - AuthorityStart);
		return !Authority.IsEmpty() && !Authority.Contains(TEXT("@"))
			&& !Authority.Contains(TEXT(" "))
			&& !Authority.Contains(TEXT("\t"));
	}

	void AddIssue(
		FOpenMobileDeviceBuildValidationReport& Report,
		const FName Code,
		const TCHAR* Message
	)
	{
		const bool bBlocks = Report.Target
			!= EOpenMobileDeviceBuildValidationTarget::Development;
		Report.Issues.Add({
			Code,
			bBlocks
				? EOpenMobileDeviceBuildValidationSeverity::Error
				: EOpenMobileDeviceBuildValidationSeverity::Warning,
			Message,
			bBlocks
		});
	}

	bool ReadStringArray(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		TArray<FString>& Out
	)
	{
		return Object.IsValid() && Object->TryGetStringArrayField(Field, Out);
	}

	bool ParsePlatform(
		const TSharedPtr<FJsonObject>& Object,
		FOpenMobileDeviceBuildValidationPlatform& Out
	)
	{
		return Object.IsValid()
			&& Object->TryGetStringField(TEXT("name"), Out.Name)
			&& Object->TryGetBoolField(
				TEXT("configHierarchyResolved"),
				Out.bConfigHierarchyResolved
			)
			&& Object->TryGetBoolField(
				TEXT("usePlatformDefaultLowStorageThreshold"),
				Out.bUsePlatformDefaultLowStorageThreshold
			)
			&& Object->TryGetNumberField(
				TEXT("lowStorageThresholdBytes"),
				Out.LowStorageThresholdBytes
			)
			&& Object->TryGetNumberField(
				TEXT("lowStorageRecoveryHysteresisBytes"),
				Out.LowStorageRecoveryHysteresisBytes
			)
			&& Object->TryGetNumberField(
				TEXT("lowStorageFallbackPollingIntervalSeconds"),
				Out.LowStorageFallbackPollingIntervalSeconds
			)
			&& Object->TryGetNumberField(
				TEXT("fallbackPollingIntervalSeconds"),
				Out.FallbackPollingIntervalSeconds
			)
			&& Object->TryGetNumberField(
				TEXT("endpointReachabilityMaximumConcurrentRequests"),
				Out.EndpointReachabilityMaximumConcurrentRequests
			)
			&& ReadStringArray(
				Object,
				TEXT("reachabilityEndpoints"),
				Out.ReachabilityEndpoints
			)
			&& ReadStringArray(
				Object,
				TEXT("declaredUrlSchemes"),
				Out.DeclaredUrlSchemes
			)
			&& ReadStringArray(
				Object,
				TEXT("declaredAndroidIntentActions"),
				Out.DeclaredAndroidIntentActions
			)
			&& ReadStringArray(
				Object,
				TEXT("declaredAndroidPackages"),
				Out.DeclaredAndroidPackages
			);
	}

	bool ParseInput(
		const FString& Json,
		FOpenMobileDeviceBuildValidationInput& Out
	)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}
		const TSharedPtr<FJsonObject>* Settings = nullptr;
		const TSharedPtr<FJsonObject>* Native = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Platforms = nullptr;
		FString Category;
		FString Section;
		FString ConfigName;
		if (!Root->TryGetObjectField(TEXT("settings"), Settings)
			|| !Root->TryGetObjectField(TEXT("native"), Native)
			|| !Root->TryGetArrayField(TEXT("platforms"), Platforms)
			|| !(*Settings)->TryGetStringField(TEXT("category"), Category)
			|| !(*Settings)->TryGetStringField(TEXT("section"), Section)
			|| !(*Settings)->TryGetStringField(
				TEXT("displayName"),
				Out.SettingsDisplayName
			)
			|| !(*Settings)->TryGetStringField(TEXT("configName"), ConfigName)
			|| !(*Settings)->TryGetBoolField(
				TEXT("allPropertiesStaged"),
				Out.bAllSettingsPropertiesStaged
			)
			|| !(*Native)->TryGetBoolField(
				TEXT("androidPluginReceiptDeclared"),
				Out.Native.bAndroidPluginReceiptDeclared
			)
			|| !ReadStringArray(
				*Native,
				TEXT("androidPermissions"),
				Out.Native.AndroidPermissions
			)
			|| !ReadStringArray(
				*Native,
				TEXT("androidDependencies"),
				Out.Native.AndroidDependencies
			)
			|| !(*Native)->TryGetBoolField(
				TEXT("iosPluginReceiptDeclared"),
				Out.Native.bIOSPluginReceiptDeclared
			)
			|| !ReadStringArray(
				*Native,
				TEXT("iosFrameworks"),
				Out.Native.IOSFrameworks
			)
			|| !(*Native)->TryGetStringField(
				TEXT("iosDiskSpacePrivacyReason"),
				Out.Native.IOSDiskSpacePrivacyReason
			))
		{
			return false;
		}
		Out.SettingsCategory = *Category;
		Out.SettingsSection = *Section;
		Out.SettingsConfigName = *ConfigName;
		for (const TSharedPtr<FJsonValue>& Value : *Platforms)
		{
			FOpenMobileDeviceBuildValidationPlatform Platform;
			if (!Value.IsValid()
				|| !ParsePlatform(Value->AsObject(), Platform))
			{
				return false;
			}
			Out.Platforms.Add(MoveTemp(Platform));
		}
		return true;
	}

	void OverlayConfig(
		const FConfigFile& Config,
		FOpenMobileDeviceBuildValidationPlatform& Platform
	)
	{
		Config.GetBool(
			SettingsSection,
			TEXT("bUsePlatformDefaultLowStorageThreshold"),
			Platform.bUsePlatformDefaultLowStorageThreshold
		);
		Config.GetInt64(
			SettingsSection,
			TEXT("LowStorageThresholdBytes"),
			Platform.LowStorageThresholdBytes
		);
		Config.GetInt64(
			SettingsSection,
			TEXT("LowStorageRecoveryHysteresisBytes"),
			Platform.LowStorageRecoveryHysteresisBytes
		);
		Config.GetFloat(
			SettingsSection,
			TEXT("LowStorageFallbackPollingIntervalSeconds"),
			Platform.LowStorageFallbackPollingIntervalSeconds
		);
		Config.GetFloat(
			SettingsSection,
			TEXT("FallbackPollingIntervalSeconds"),
			Platform.FallbackPollingIntervalSeconds
		);
		Config.GetInt(
			SettingsSection,
			TEXT("EndpointReachabilityMaximumConcurrentRequests"),
			Platform.EndpointReachabilityMaximumConcurrentRequests
		);
		Config.GetArray(
			SettingsSection,
			TEXT("DeclaredUrlSchemes"),
			Platform.DeclaredUrlSchemes
		);
		Config.GetArray(
			SettingsSection,
			TEXT("DeclaredAndroidIntentActions"),
			Platform.DeclaredAndroidIntentActions
		);
		Config.GetArray(
			SettingsSection,
			TEXT("DeclaredAndroidPackages"),
			Platform.DeclaredAndroidPackages
		);
	}

	void AddQuotedTokenIfPresent(
		const FString& Source,
		const TCHAR* Token,
		TArray<FString>& Out
	)
	{
		if (Source.Contains(FString::Printf(TEXT("\"%s\""), Token)))
		{
			Out.Add(Token);
		}
	}
}

bool FOpenMobileDeviceBuildValidationReport::HasBlockingIssues() const
{
	return Issues.ContainsByPredicate(
		[](const FOpenMobileDeviceBuildValidationIssue& Issue)
		{
			return Issue.bBlocksBuild;
		}
	);
}

FName IOpenMobileDeviceBuildValidationContributor::GetModularFeatureName()
{
	return TEXT("OpenMobile.StoreDoctor.ValidationContributor");
}

FName FOpenMobileDeviceBuildValidation::GetContributorName() const
{
	return TEXT("OpenMobileDevice");
}

bool FOpenMobileDeviceBuildValidation::LoadPlatformSettingsFromConfigHierarchy(
	const FString& EngineConfigDirectory,
	const FString& SourceConfigDirectory,
	const TCHAR* PlatformName,
	FOpenMobileDeviceBuildValidationPlatform& OutPlatform
)
{
	OutPlatform = {};
	OutPlatform.Name = PlatformName;
	FString EngineConfigRoot = EngineConfigDirectory;
	FString SourceConfigRoot = SourceConfigDirectory;
	FPaths::NormalizeDirectoryName(EngineConfigRoot);
	FPaths::NormalizeDirectoryName(SourceConfigRoot);
	EngineConfigRoot += TEXT("/");
	SourceConfigRoot += TEXT("/");
	FConfigFile Config;
	OutPlatform.bConfigHierarchyResolved = FConfigCacheIni::LoadExternalIniFile(
		Config,
		TEXT("Game"),
		*EngineConfigRoot,
		*SourceConfigRoot,
		true,
		PlatformName,
		true,
		false
	);
	if (OutPlatform.bConfigHierarchyResolved)
	{
		OpenMobileDeviceBuildValidationPrivate::OverlayConfig(Config, OutPlatform);
	}
	return OutPlatform.bConfigHierarchyResolved;
}

FOpenMobileDeviceBuildValidationInput
FOpenMobileDeviceBuildValidation::CaptureProjectInput() const
{
	using namespace OpenMobileDeviceBuildValidationPrivate;
	FOpenMobileDeviceBuildValidationInput Input;
	const UOpenMobileDeviceSettings* Settings =
		GetDefault<UOpenMobileDeviceSettings>();
	Input.SettingsCategory = Settings->GetCategoryName();
	Input.SettingsSection = Settings->GetSectionName();
#if WITH_METADATA
	Input.SettingsDisplayName =
		Settings->GetClass()->GetMetaData(TEXT("DisplayName"));
#endif
	Input.SettingsConfigName = Settings->GetClass()->ClassConfigName;
	Input.bAllSettingsPropertiesStaged = true;
	for (TFieldIterator<FProperty> Property(Settings->GetClass()); Property; ++Property)
	{
		if (Property->GetOwnerClass() == Settings->GetClass()
			&& !Property->HasAnyPropertyFlags(CPF_Config))
		{
			Input.bAllSettingsPropertiesStaged = false;
			break;
		}
	}
	for (const TCHAR* PlatformName : {TEXT("Android"), TEXT("IOS")})
	{
		FOpenMobileDeviceBuildValidationPlatform Platform;
		LoadPlatformSettingsFromConfigHierarchy(
			FPaths::EngineConfigDir(),
			FPaths::ProjectConfigDir(),
			PlatformName,
			Platform
		);
		Input.Platforms.Add(MoveTemp(Platform));
	}

	const TSharedPtr<IPlugin> Plugin =
		IPluginManager::Get().FindPlugin(TEXT("OpenMobileDevice"));
	if (!Plugin.IsValid())
	{
		return Input;
	}
	const FString BaseDir = Plugin->GetBaseDir();
	const FString AndroidBuildPath = FPaths::Combine(
		BaseDir,
		TEXT("Source/OpenMobileDeviceAndroid/OpenMobileDeviceAndroid.Build.cs")
	);
	const FString AndroidUplPath = FPaths::Combine(
		BaseDir,
		TEXT("Source/OpenMobileDeviceAndroid/Private/Android/OpenMobileDevice_Android_UPL.xml")
	);
	FString AndroidBuild;
	FString AndroidUpl;
	FFileHelper::LoadFileToString(AndroidBuild, *AndroidBuildPath);
	FFileHelper::LoadFileToString(AndroidUpl, *AndroidUplPath);
	Input.Native.bAndroidPluginReceiptDeclared =
		AndroidBuild.Contains(TEXT("AdditionalPropertiesForReceipt"))
		&& AndroidBuild.Contains(TEXT("OpenMobileDevice_Android_UPL.xml"))
		&& FPaths::FileExists(AndroidUplPath);
	FRegexMatcher PermissionMatcher(
		FRegexPattern(TEXT("addPermission[^>]*android:name=\"([^\"]+)\"")),
		AndroidUpl
	);
	while (PermissionMatcher.FindNext())
	{
		Input.Native.AndroidPermissions.Add(
			PermissionMatcher.GetCaptureGroup(1)
		);
	}
	if (AndroidUpl.Contains(TEXT("androidx.window:window-java:1.5.1")))
	{
		Input.Native.AndroidDependencies.Add(
			TEXT("androidx.window:window-java:1.5.1")
		);
	}

	const FString IOSBuildPath = FPaths::Combine(
		BaseDir,
		TEXT("Source/OpenMobileDeviceIOS/OpenMobileDeviceIOS.Build.cs")
	);
	const FString IOSUplPath = FPaths::Combine(
		BaseDir,
		TEXT("Source/OpenMobileDeviceIOS/Private/IOS/OpenMobileDevice_IOS_UPL.xml")
	);
	FString IOSBuild;
	FFileHelper::LoadFileToString(IOSBuild, *IOSBuildPath);
	Input.Native.bIOSPluginReceiptDeclared =
		IOSBuild.Contains(TEXT("AdditionalPropertiesForReceipt"))
		&& IOSBuild.Contains(TEXT("OpenMobileDevice_IOS_UPL.xml"))
		&& FPaths::FileExists(IOSUplPath);
	for (const TCHAR* Framework : {
		TEXT("AVFoundation"),
		TEXT("Foundation"),
		TEXT("SystemConfiguration"),
		TEXT("UIKit")
	})
	{
		AddQuotedTokenIfPresent(IOSBuild, Framework, Input.Native.IOSFrameworks);
	}
	FString PrivacyManifest;
	FFileHelper::LoadFileToString(
		PrivacyManifest,
		*FPaths::Combine(
			FPaths::EngineDir(),
			TEXT("Build/IOS/Resources/UEMetadata/PrivacyInfo.xcprivacy")
		)
	);
	if (PrivacyManifest.Contains(TEXT("NSPrivacyAccessedAPICategoryDiskSpace"))
		&& PrivacyManifest.Contains(TEXT("E174.1")))
	{
		Input.Native.IOSDiskSpacePrivacyReason = TEXT("E174.1");
	}
	return Input;
}

FOpenMobileDeviceBuildValidationReport
FOpenMobileDeviceBuildValidation::Validate(
	const FOpenMobileDeviceBuildValidationInput& Input,
	EOpenMobileDeviceBuildValidationTarget Target
) const
{
	using namespace OpenMobileDeviceBuildValidationPrivate;
	FOpenMobileDeviceBuildValidationReport Report;
	Report.Target = Target;
	if (Input.SettingsCategory != TEXT("OpenMobile")
		|| Input.SettingsSection != TEXT("OpenMobile Device")
		|| Input.SettingsDisplayName != TEXT("OpenMobile Device")
		|| Input.SettingsConfigName != TEXT("Game")
		|| !Input.bAllSettingsPropertiesStaged)
	{
		AddIssue(
			Report,
			TEXT("Device.BuildValidation.InvalidSettingsContract"),
			TEXT("Device settings metadata or config staging is invalid.")
		);
	}
	TSet<FString> SeenPlatforms;
	for (const FOpenMobileDeviceBuildValidationPlatform& Platform : Input.Platforms)
	{
		const FString NormalizedPlatform = Platform.Name.ToLower();
		if (SeenPlatforms.Contains(NormalizedPlatform))
		{
			AddIssue(
				Report,
				TEXT("Device.BuildValidation.ConflictingPlatformConfiguration"),
				TEXT("A platform has more than one resolved Device configuration.")
			);
		}
		SeenPlatforms.Add(NormalizedPlatform);
		if (!Platform.bConfigHierarchyResolved)
		{
			AddIssue(
				Report,
				TEXT("Device.BuildValidation.PlatformConfigurationMissing"),
				TEXT("A platform Game config hierarchy could not be resolved.")
			);
		}
		if ((!Platform.bUsePlatformDefaultLowStorageThreshold
				&& Platform.LowStorageThresholdBytes < 0)
			|| Platform.LowStorageRecoveryHysteresisBytes < 0)
		{
			AddIssue(
				Report,
				TEXT("Device.BuildValidation.InvalidStorageThreshold"),
				TEXT("A storage threshold is outside its supported range.")
			);
		}
		if (!FMath::IsFinite(Platform.LowStorageFallbackPollingIntervalSeconds)
			|| Platform.LowStorageFallbackPollingIntervalSeconds < 5.0f
			|| Platform.LowStorageFallbackPollingIntervalSeconds > 60.0f
			|| !FMath::IsFinite(Platform.FallbackPollingIntervalSeconds)
			|| Platform.FallbackPollingIntervalSeconds < 0.1f
			|| Platform.FallbackPollingIntervalSeconds > 60.0f)
		{
			AddIssue(
				Report,
				TEXT("Device.BuildValidation.InvalidPollingInterval"),
				TEXT("A polling interval is outside its supported range.")
			);
		}
		if (Platform.EndpointReachabilityMaximumConcurrentRequests < 1
			|| Platform.EndpointReachabilityMaximumConcurrentRequests > 16)
		{
			AddIssue(
				Report,
				TEXT("Device.BuildValidation.InvalidReachabilityConcurrency"),
				TEXT("Endpoint request concurrency must be from 1 through 16.")
			);
		}
		for (const FString& Endpoint : Platform.ReachabilityEndpoints)
		{
			if (!IsPrivacySafeEndpoint(Endpoint))
			{
				AddIssue(
					Report,
					TEXT("Device.BuildValidation.PrivacyUnsafeEndpoint"),
					TEXT("A reachability endpoint is invalid or contains private components.")
				);
			}
		}
		TSet<FString> SeenSchemes;
		for (const FString& Scheme : Platform.DeclaredUrlSchemes)
		{
			const FString Normalized = Scheme.ToLower();
			if (!IsValidUrlScheme(Scheme))
			{
				AddIssue(
					Report,
					TEXT("Device.BuildValidation.InvalidUrlScheme"),
					TEXT("A declared URL scheme is invalid or unsafe.")
				);
			}
			else if (SeenSchemes.Contains(Normalized))
			{
				AddIssue(
					Report,
					TEXT("Device.BuildValidation.ConflictingUrlScheme"),
					TEXT("A declared URL scheme is duplicated.")
				);
			}
			SeenSchemes.Add(Normalized);
		}
		for (const FString& Action : Platform.DeclaredAndroidIntentActions)
		{
			if (!IsValidDottedIdentifier(Action, 255))
			{
				AddIssue(
					Report,
					TEXT("Device.BuildValidation.InvalidIntentAction"),
					TEXT("A declared Android intent action is invalid.")
				);
			}
		}
		for (const FString& Package : Platform.DeclaredAndroidPackages)
		{
			if (!IsValidDottedIdentifier(Package, 223))
			{
				AddIssue(
					Report,
					TEXT("Device.BuildValidation.InvalidPackageName"),
					TEXT("A declared Android package name is invalid.")
				);
			}
		}
	}
	if (!SeenPlatforms.Contains(TEXT("android"))
		|| !SeenPlatforms.Contains(TEXT("ios")))
	{
		AddIssue(
			Report,
			TEXT("Device.BuildValidation.PlatformConfigurationMissing"),
			TEXT("Android and iOS Device configurations are required.")
		);
	}
	if (!Input.Native.bAndroidPluginReceiptDeclared
		|| !Input.Native.bIOSPluginReceiptDeclared)
	{
		AddIssue(
			Report,
			TEXT("Device.BuildValidation.NativePluginReceiptMissing"),
			TEXT("A Device platform plugin receipt is missing.")
		);
	}
	if (!Input.Native.AndroidPermissions.Contains(
		TEXT("android.permission.ACCESS_NETWORK_STATE")))
	{
		AddIssue(
			Report,
			TEXT("Device.BuildValidation.PermissionMissing"),
			TEXT("The required Android network-state permission is missing.")
		);
	}
	for (const FString& Permission : Input.Native.AndroidPermissions)
	{
		if (Permission != TEXT("android.permission.ACCESS_NETWORK_STATE"))
		{
			AddIssue(
				Report,
				TEXT("Device.BuildValidation.PrivacyUnsafePermission"),
				TEXT("The Device plugin declares an unexpected Android permission.")
			);
		}
	}
	if (!Input.Native.AndroidDependencies.Contains(
		TEXT("androidx.window:window-java:1.5.1")))
	{
		AddIssue(
			Report,
			TEXT("Device.BuildValidation.NativeDependencyMissing"),
			TEXT("The required Android window dependency is missing.")
		);
	}
	for (const TCHAR* Framework : {
		TEXT("AVFoundation"),
		TEXT("Foundation"),
		TEXT("SystemConfiguration"),
		TEXT("UIKit")
	})
	{
		if (!Input.Native.IOSFrameworks.Contains(Framework))
		{
			AddIssue(
				Report,
				TEXT("Device.BuildValidation.NativeDependencyMissing"),
				TEXT("A required iOS framework is missing.")
			);
		}
	}
	if (Input.Native.IOSDiskSpacePrivacyReason != TEXT("E174.1"))
	{
		AddIssue(
			Report,
			TEXT("Device.BuildValidation.PrivacyReasonMissing"),
			TEXT("The iOS disk-space privacy reason is missing or invalid.")
		);
	}
	return Report;
}

FOpenMobileDeviceBuildValidationReport
FOpenMobileDeviceBuildValidation::ValidateFixture(
	const FString& Path,
	EOpenMobileDeviceBuildValidationTarget Target
)
{
	using namespace OpenMobileDeviceBuildValidationPrivate;
	FOpenMobileDeviceBuildValidationReport Report;
	Report.Target = Target;
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		AddIssue(
			Report,
			TEXT("Device.BuildValidation.ConfigurationMissing"),
			TEXT("The Device validation configuration could not be loaded.")
		);
		return Report;
	}
	FOpenMobileDeviceBuildValidationInput Input;
	if (!ParseInput(Json, Input))
	{
		AddIssue(
			Report,
			TEXT("Device.BuildValidation.ConfigurationMalformed"),
			TEXT("The Device validation configuration is malformed.")
		);
		return Report;
	}
	return FOpenMobileDeviceBuildValidation().Validate(Input, Target);
}
