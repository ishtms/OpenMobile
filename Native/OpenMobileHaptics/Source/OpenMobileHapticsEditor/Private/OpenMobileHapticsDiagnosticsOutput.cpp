#include "OpenMobileHapticsDiagnosticsOutput.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileHapticLibrary.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticPlatformAssets.h"
#include "OpenMobileHapticsBackendRegistry.h"
#include "OpenMobileHapticsPreviewReceiverSubsystem.h"
#include "OpenMobileHapticsSubsystem.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectIterator.h"

namespace OpenMobileHapticsDiagnosticsOutputPrivate
{
	constexpr int32 MaximumOutputBytes = 128 * 1024;
	constexpr int32 MaximumStringLength = 256;
	constexpr int32 MaximumChannels = 64;
	constexpr int32 MaximumHandles = 64;
	constexpr int32 MaximumErrors = 32;
	constexpr int32 MaximumIssues = 64;
	constexpr int32 MaximumScannedAssets = 256;
	constexpr int32 MinimumAndroidBuildApi = 31;
	constexpr int32 MinimumIOSVersion = 13;

#if WITH_DEV_AUTOMATION_TESTS
	TFunction<bool(const FString&)> ClipboardWriterForTests;
	TFunction<bool(const FString&, const FString&)> FileWriterForTests;
#endif

	template <typename EnumType>
	FString EnumName(EnumType Value)
	{
		const UEnum* Enum = StaticEnum<EnumType>();
		return Enum
			? Enum->GetNameStringByValue(static_cast<int64>(Value))
			: TEXT("Unknown");
	}

	FString SeverityName(EOpenMobileHapticsDiagnosticSeverity Severity)
	{
		switch (Severity)
		{
		case EOpenMobileHapticsDiagnosticSeverity::Info:
			return TEXT("Info");
		case EOpenMobileHapticsDiagnosticSeverity::Warning:
			return TEXT("Warning");
		case EOpenMobileHapticsDiagnosticSeverity::Error:
			return TEXT("Error");
		case EOpenMobileHapticsDiagnosticSeverity::Fatal:
			return TEXT("Fatal");
		}
		return TEXT("Unknown");
	}

	EOpenMobileHapticsDiagnosticSeverity ErrorSeverity(
		EOpenMobileHapticErrorCode Code
	)
	{
		switch (Code)
		{
		case EOpenMobileHapticErrorCode::NativeEngineFailure:
		case EOpenMobileHapticErrorCode::BackendUnavailable:
		case EOpenMobileHapticErrorCode::Internal:
			return EOpenMobileHapticsDiagnosticSeverity::Error;
		case EOpenMobileHapticErrorCode::None:
			return EOpenMobileHapticsDiagnosticSeverity::Info;
		default:
			return EOpenMobileHapticsDiagnosticSeverity::Warning;
		}
	}

	FOpenMobileHapticsDiagnosticsOutputResult MakeFailure(
		EOpenMobileHapticsDiagnosticsOutputCode Code,
		const TCHAR* Message
	)
	{
		FOpenMobileHapticsDiagnosticsOutputResult Result;
		Result.Code = Code;
		Result.Message = Message;
		return Result;
	}

	void AddSaturated(int32& Target, int32 Value)
	{
		Target = static_cast<int32>(FMath::Min<int64>(
			MAX_int32,
			static_cast<int64>(Target) + FMath::Max(0, Value)
		));
	}

	void AddSaturated(int64& Target, int64 Value)
	{
		Target = Value > 0 && Target > MAX_int64 - Value
			? MAX_int64
			: Target + FMath::Max<int64>(0, Value);
	}

	FName ApplicationStateName(EOpenMobileHapticsApplicationState State)
	{
		switch (State)
		{
		case EOpenMobileHapticsApplicationState::Active:
			return TEXT("Active");
		case EOpenMobileHapticsApplicationState::Inactive:
			return TEXT("Inactive");
		case EOpenMobileHapticsApplicationState::Background:
			return TEXT("Background");
		case EOpenMobileHapticsApplicationState::Terminating:
			return TEXT("Terminating");
		}
		return TEXT("Unknown");
	}

	FString SafeString(
		const FString& Value,
		bool bIncludeFilePaths,
		bool& bTruncated
	)
	{
		FString Result = Value;
		Result.ReplaceInline(TEXT("\r"), TEXT(" "));
		Result.ReplaceInline(TEXT("\n"), TEXT(" "));
		Result.ReplaceInline(TEXT("\t"), TEXT(" "));
		if (!bIncludeFilePaths)
		{
			const FString ProjectDirectory = FPaths::ProjectDir();
			const FString UserDirectory = FPlatformProcess::UserDir();
			if (Result.Contains(TEXT("://"))
				|| Result.Contains(TEXT("?"))
				|| Result.Contains(TEXT("#"))
				|| (!ProjectDirectory.IsEmpty()
					&& Result.Contains(ProjectDirectory))
				|| (!UserDirectory.IsEmpty() && Result.Contains(UserDirectory))
				|| (!Result.IsEmpty() && !FPaths::IsRelative(Result)))
			{
				bTruncated = true;
				return TEXT("<redacted>");
			}
		}
		if (Result.Len() > MaximumStringLength)
		{
			Result.LeftInline(MaximumStringLength);
			bTruncated = true;
		}
		return Result;
	}

	FString NameForExport(
		FName Name,
		const TCHAR* Prefix,
		bool bIncludeProjectNames,
		TMap<FName, FString>& Aliases,
		bool& bTruncated
	)
	{
		if (Name.IsNone())
		{
			return TEXT("None");
		}
		if (bIncludeProjectNames)
		{
			return SafeString(Name.ToString(), false, bTruncated);
		}
		if (const FString* Existing = Aliases.Find(Name))
		{
			return *Existing;
		}
		const FString Alias = FString::Printf(
			TEXT("%s-%d"),
			Prefix,
			Aliases.Num() + 1
		);
		Aliases.Add(Name, Alias);
		return Alias;
	}

	void AddIssue(
		FOpenMobileHapticsDiagnosticSnapshot& Snapshot,
		FName Code,
		EOpenMobileHapticsDiagnosticSeverity Severity,
		const TCHAR* Summary,
		FString Subject = {},
		bool bSubjectIsProjectName = false,
		bool bSubjectIsFilePath = false,
		bool bSubjectIsImportedMetadata = false
	)
	{
		for (const FOpenMobileHapticsDiagnosticIssue& Existing : Snapshot.Issues)
		{
			if (Existing.Code == Code && Existing.Subject == Subject)
			{
				return;
			}
		}
		if (Snapshot.Issues.Num() >= MaximumIssues)
		{
			Snapshot.bTruncated = true;
			return;
		}
		FOpenMobileHapticsDiagnosticIssue& Issue =
			Snapshot.Issues.AddDefaulted_GetRef();
		Issue.Code = Code;
		Issue.Severity = Severity;
		Issue.Summary = Summary;
		Issue.Subject = MoveTemp(Subject);
		Issue.bSubjectIsProjectName = bSubjectIsProjectName;
		Issue.bSubjectIsFilePath = bSubjectIsFilePath;
		Issue.bSubjectIsImportedMetadata = bSubjectIsImportedMetadata;
	}

	int32 ParseVersionNumber(const FString& Value)
	{
		FString Digits;
		bool bStarted = false;
		for (const TCHAR Character : Value)
		{
			if (FChar::IsDigit(Character))
			{
				Digits.AppendChar(Character);
				bStarted = true;
			}
			else if (bStarted)
			{
				break;
			}
		}
		return Digits.IsEmpty() ? 0 : FCString::Atoi(*Digits);
	}

	void CheckSettings(
		const UOpenMobileHapticsSettings& Settings,
		FOpenMobileHapticsDiagnosticSnapshot& Snapshot
	)
	{
		TArray<FString> Errors;
		if (!Settings.Validate(Errors))
		{
			AddIssue(
				Snapshot,
				TEXT("Haptics.Configuration.InvalidSettings"),
				EOpenMobileHapticsDiagnosticSeverity::Error,
				TEXT("One or more Haptics project settings are invalid.")
			);
		}

		for (const FOpenMobileHapticNamedLibrarySettings& Entry :
			Settings.NamedLibraries)
		{
			UOpenMobileHapticLibrary* Library = Entry.Asset.IsNull()
				? nullptr
				: Cast<UOpenMobileHapticLibrary>(Entry.Asset.TryLoad());
			if (!Library)
			{
				AddIssue(
					Snapshot,
					TEXT("Haptics.Library.MissingNamedLibrary"),
					EOpenMobileHapticsDiagnosticSeverity::Error,
					TEXT("A configured named Haptics library could not be loaded."),
					Entry.Name.ToString(),
					true
				);
				continue;
			}
			TMap<FName, FSoftObjectPath> Patterns;
			if (!Library->BuildPatternLookup(Patterns, Errors))
			{
				AddIssue(
					Snapshot,
					TEXT("Haptics.Asset.InvalidLibrary"),
					EOpenMobileHapticsDiagnosticSeverity::Error,
					TEXT("A configured Haptics library contains invalid entries."),
					Entry.Name.ToString(),
					true
				);
			}
		}
	}

	void CheckAndroidPackaging(
		const UOpenMobileHapticsSettings& Settings,
		const FString& PluginDirectory,
		FOpenMobileHapticsDiagnosticSnapshot& Snapshot
	)
	{
		if (!Settings.bEnableAndroidCustomVibration)
		{
			return;
		}
		const FString UPLPath = FPaths::Combine(
			PluginDirectory,
			TEXT("Source/OpenMobileHapticsAndroid/Private/Android/"),
			TEXT("OpenMobileHaptics_Android_UPL.xml")
		);
		FString UPL;
		const bool bHasPermission = FFileHelper::LoadFileToString(UPL, *UPLPath)
			&& UPL.Contains(TEXT("addPermission"))
			&& UPL.Contains(TEXT("android.permission.VIBRATE"))
			&& UPL.Contains(TEXT("bEnableAndroidCustomVibration"));
		bool bProjectRemovesPermission = false;
		const TArray<FString> ProjectManifestFiles = {
			FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("Build/Android/ManifestRequirementsOverride.txt")
			),
			FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("Build/Android/AndroidManifest.xml")
			)
		};
		for (const FString& FilePath : ProjectManifestFiles)
		{
			FString Manifest;
			if (!FFileHelper::LoadFileToString(Manifest, *FilePath))
			{
				continue;
			}
			Manifest.ToLowerInline();
			bProjectRemovesPermission |=
				Manifest.Contains(TEXT("android.permission.vibrate"))
				&& (Manifest.Contains(TEXT("removepermission"))
					|| Manifest.Contains(TEXT("tools:node=\"remove\"")));
		}
		if (!bHasPermission || bProjectRemovesPermission)
		{
			AddIssue(
				Snapshot,
				TEXT("Haptics.Android.MissingVibratePermission"),
				EOpenMobileHapticsDiagnosticSeverity::Error,
				TEXT("Android custom vibration is enabled but the VIBRATE permission is not safely packaged.")
			);
		}

		FString BuildApi;
		if (GConfig && GConfig->GetString(
			TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"),
			TEXT("SDKAPILevelOverride"),
			BuildApi,
			GEngineIni
		))
		{
			const int32 BuildApiNumber = ParseVersionNumber(BuildApi);
			if (BuildApiNumber > 0 && BuildApiNumber < MinimumAndroidBuildApi)
			{
				AddIssue(
					Snapshot,
					TEXT("Haptics.Android.UnsupportedBuildApi"),
					EOpenMobileHapticsDiagnosticSeverity::Error,
					TEXT("The Android build API is too old for the Haptics bridge.")
				);
			}
		}
	}

	void CheckApplePackaging(
		const UOpenMobileHapticsSettings& Settings,
		const FString& PluginDirectory,
		FOpenMobileHapticsDiagnosticSnapshot& Snapshot
	)
	{
		if (!Settings.IOS.bEnableCoreHaptics)
		{
			return;
		}
		const FString BuildRulesPath = FPaths::Combine(
			PluginDirectory,
			TEXT("Source/OpenMobileHapticsIOS/OpenMobileHapticsIOS.Build.cs")
		);
		FString BuildRules;
		const bool bHasCoreFrameworks =
			FFileHelper::LoadFileToString(BuildRules, *BuildRulesPath)
			&& BuildRules.Contains(TEXT("CoreHaptics"))
			&& BuildRules.Contains(TEXT("Foundation"))
			&& BuildRules.Contains(TEXT("UIKit"));
		const bool bHasResourceFrameworks = !Settings.IOS.bPackageAHAPResources
			|| (BuildRules.Contains(TEXT("AVFoundation"))
				&& BuildRules.Contains(TEXT("AudioToolbox")));
		if (!bHasCoreFrameworks || !bHasResourceFrameworks)
		{
			AddIssue(
				Snapshot,
				TEXT("Haptics.Apple.MissingFramework"),
				EOpenMobileHapticsDiagnosticSeverity::Error,
				TEXT("Required Apple Haptics frameworks are missing from the build rules.")
			);
		}

		FString IOSVersion;
		if (GConfig && GConfig->GetString(
			TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"),
			TEXT("MinimumiOSVersion"),
			IOSVersion,
			GEngineIni
		))
		{
			const int32 VersionNumber = ParseVersionNumber(IOSVersion);
			if (VersionNumber > 0 && VersionNumber < MinimumIOSVersion)
			{
				AddIssue(
					Snapshot,
					TEXT("Haptics.Apple.UnsupportedDeploymentTarget"),
					EOpenMobileHapticsDiagnosticSeverity::Error,
					TEXT("The iOS deployment target is too old for Core Haptics.")
				);
			}
		}
	}

	void CheckAssets(
		const UOpenMobileHapticsSettings& Settings,
		FOpenMobileHapticsDiagnosticSnapshot& Snapshot
	)
	{
		FAssetRegistryModule& RegistryModule =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
				TEXT("AssetRegistry")
			);
		TArray<FAssetData> Assets;
		RegistryModule.Get().GetAssetsByClass(
			UOpenMobileHapticPatternAsset::StaticClass()->GetClassPathName(),
			Assets,
			true
		);
		TArray<FAssetData> PlatformAssets;
		RegistryModule.Get().GetAssetsByClass(
			UOpenMobileHapticPlatformPatternAsset::StaticClass()->GetClassPathName(),
			PlatformAssets,
			true
		);
		Assets.Append(PlatformAssets);
		Assets.Sort([](const FAssetData& Left, const FAssetData& Right)
		{
			return Left.PackageName.LexicalLess(Right.PackageName);
		});
		if (Assets.Num() > MaximumScannedAssets)
		{
			Snapshot.bTruncated = true;
		}
		for (int32 Index = 0;
			Index < Assets.Num() && Index < MaximumScannedAssets;
			++Index)
		{
			const FAssetData& AssetData = Assets[Index];
			UObject* Asset = AssetData.GetAsset();
			const FString Subject = AssetData.GetSoftObjectPath().ToString();
			TArray<FString> Errors;
			bool bValid = IsValid(Asset);
			if (const UOpenMobileHapticPatternAsset* Pattern =
				Cast<UOpenMobileHapticPatternAsset>(Asset))
			{
				bValid = Pattern->ValidateForEditor(Errors);
				const FOpenMobileHapticLoopOptions& Loop = Pattern->Loop;
				const bool bUnsafeLoop = Loop.bLoop
					&& (Loop.RepeatCount < 0
						|| Loop.RepeatCount > Settings.MaximumFiniteRepeatCount
						|| !FMath::IsFinite(Loop.RepeatStartTimeSeconds)
						|| Loop.RepeatStartTimeSeconds < 0.0
						|| !FMath::IsFinite(Loop.MaximumDurationSeconds)
						|| Loop.MaximumDurationSeconds <= 0.0
						|| Loop.MaximumDurationSeconds
							> Settings.MaximumContinuousDurationSeconds);
				if (bUnsafeLoop)
				{
					AddIssue(
						Snapshot,
						TEXT("Haptics.Asset.UnsafeLoop"),
						EOpenMobileHapticsDiagnosticSeverity::Error,
						TEXT("A Haptics pattern has loop settings outside the project safety limits."),
						Subject,
						true,
						true
					);
				}
			}
			else if (const UOpenMobileHapticPlatformPatternAsset* Platform =
				Cast<UOpenMobileHapticPlatformPatternAsset>(Asset))
			{
				bValid = Platform->Validate(Errors);
			}
			if (!bValid)
			{
				AddIssue(
					Snapshot,
					TEXT("Haptics.Asset.InvalidPattern"),
					EOpenMobileHapticsDiagnosticSeverity::Error,
					TEXT("A Haptics pattern asset failed validation."),
					Subject,
					true,
					true
				);
			}
			if (const UOpenMobileHapticIOSPatternAsset* IOSPattern =
				Cast<UOpenMobileHapticIOSPatternAsset>(Asset))
			{
				bool bInvalidResource = !bValid;
				for (const FOpenMobileHapticIOSAudioResource& Resource :
					IOSPattern->GetAudioResources())
				{
					bInvalidResource |= Resource.RelativePath.IsEmpty()
						|| Resource.Data.IsEmpty();
				}
				if (bInvalidResource)
				{
					AddIssue(
						Snapshot,
						TEXT("Haptics.Apple.InvalidResource"),
						EOpenMobileHapticsDiagnosticSeverity::Error,
						TEXT("An Apple Haptics asset has invalid AHAP or audio resources."),
						Subject,
						true,
						true,
						true
					);
				}
			}
		}
	}

	void CheckPreviewReceivers(
		FOpenMobileHapticsDiagnosticSnapshot& Snapshot
	)
	{
		for (TObjectIterator<UOpenMobileHapticsPreviewReceiverSubsystem> It;
			It;
			++It)
		{
			const UOpenMobileHapticsPreviewReceiverSubsystem* Receiver = *It;
			if (!IsValid(Receiver)
				|| Receiver->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
			{
				continue;
			}
			const FOpenMobileHapticsPreviewReceiverStatus Status =
				Receiver->GetReceiverStatus();
			FString Error = Status.LastError;
			Error.ToLowerInline();
			const bool bLooksStale = Error.Contains(TEXT("stale"))
				|| Error.Contains(TEXT("expired"))
				|| Error.Contains(TEXT("session"))
				|| Error.Contains(TEXT("capability"));
			if (Status.bEnabled
				&& (!Status.bAvailableInThisBuild || bLooksStale))
			{
				AddIssue(
					Snapshot,
					TEXT("Haptics.Preview.StaleReceiver"),
					EOpenMobileHapticsDiagnosticSeverity::Warning,
					TEXT("A connected-device preview receiver appears stale or unavailable."),
					Status.ReceiverLabel,
					true,
					false,
					true
				);
			}
		}
	}

	void MergeDiagnostics(
		const FOpenMobileHapticsDiagnostics& Source,
		FOpenMobileHapticsDiagnosticSnapshot& Snapshot,
		TMap<FName, FOpenMobileHapticChannelDiagnostics>& Channels
	)
	{
		AddSaturated(Snapshot.ActivePlaybackCount, Source.ActivePlaybackCount);
		AddSaturated(Snapshot.QueuedPlaybackCount, Source.QueuedPlaybackCount);
		AddSaturated(
			Snapshot.PreparedNamedPatternCount,
			Source.PreparedNamedPatternCount
		);
		AddSaturated(Snapshot.FallbackPlaybackCount, Source.FallbackPlaybackCount);
		Snapshot.bTruncated |= Source.bTruncated;
		const FOpenMobileHapticsPerformanceDiagnostics& Performance =
			Source.Performance;
		AddSaturated(
			Snapshot.Performance.DroppedRequestCount,
			Performance.DroppedRequestCount
		);
		Snapshot.Performance.PeakQueuedPlaybackCount = FMath::Max(
			Snapshot.Performance.PeakQueuedPlaybackCount,
			Performance.PeakQueuedPlaybackCount
		);
		Snapshot.Performance.TimelineCacheHitCount = FMath::Max(
			Snapshot.Performance.TimelineCacheHitCount,
			Performance.TimelineCacheHitCount
		);
		Snapshot.Performance.TimelineCacheMissCount = FMath::Max(
			Snapshot.Performance.TimelineCacheMissCount,
			Performance.TimelineCacheMissCount
		);
		Snapshot.Performance.TimelineCacheEvictionCount = FMath::Max(
			Snapshot.Performance.TimelineCacheEvictionCount,
			Performance.TimelineCacheEvictionCount
		);
		Snapshot.Performance.TimelineCacheEntryCount = FMath::Max(
			Snapshot.Performance.TimelineCacheEntryCount,
			Performance.TimelineCacheEntryCount
		);
		Snapshot.Performance.TimelineCacheMemoryBytes = FMath::Max(
			Snapshot.Performance.TimelineCacheMemoryBytes,
			Performance.TimelineCacheMemoryBytes
		);
		Snapshot.Performance.TimelineCacheMaximumEntryCount = FMath::Max(
			Snapshot.Performance.TimelineCacheMaximumEntryCount,
			Performance.TimelineCacheMaximumEntryCount
		);
		Snapshot.Performance.TimelineCacheMaximumMemoryBytes = FMath::Max(
			Snapshot.Performance.TimelineCacheMaximumMemoryBytes,
			Performance.TimelineCacheMaximumMemoryBytes
		);
		AddSaturated(
			Snapshot.Performance.PreparationCount,
			Performance.PreparationCount
		);
		Snapshot.Performance.LastPreparationLatencyMilliseconds = FMath::Max(
			Snapshot.Performance.LastPreparationLatencyMilliseconds,
			Performance.LastPreparationLatencyMilliseconds
		);
		Snapshot.Performance.MaximumPreparationLatencyMilliseconds = FMath::Max(
			Snapshot.Performance.MaximumPreparationLatencyMilliseconds,
			Performance.MaximumPreparationLatencyMilliseconds
		);
		AddSaturated(
			Snapshot.Performance.NativeSubmissionCount,
			Performance.NativeSubmissionCount
		);
		Snapshot.Performance.LastNativeSubmissionLatencyMilliseconds = FMath::Max(
			Snapshot.Performance.LastNativeSubmissionLatencyMilliseconds,
			Performance.LastNativeSubmissionLatencyMilliseconds
		);
		Snapshot.Performance.MaximumNativeSubmissionLatencyMilliseconds = FMath::Max(
			Snapshot.Performance.MaximumNativeSubmissionLatencyMilliseconds,
			Performance.MaximumNativeSubmissionLatencyMilliseconds
		);

		for (const FOpenMobileHapticChannelDiagnostics& SourceChannel :
			Source.Channels)
		{
			FOpenMobileHapticChannelDiagnostics& Channel =
				Channels.FindOrAdd(SourceChannel.Channel);
			Channel.Channel = SourceChannel.Channel;
			AddSaturated(
				Channel.ActivePlaybackCount,
				SourceChannel.ActivePlaybackCount
			);
			AddSaturated(
				Channel.QueuedPlaybackCount,
				SourceChannel.QueuedPlaybackCount
			);
		}
		for (const FOpenMobileHapticHandleDiagnostics& SourceHandle :
			Source.ActiveHandles)
		{
			if (Snapshot.ActiveHandles.Num() >= MaximumHandles)
			{
				Snapshot.bTruncated = true;
				break;
			}
			FOpenMobileHapticHandleDiagnostics Handle = SourceHandle;
			Handle.Ordinal = Snapshot.ActiveHandles.Num() + 1;
			Snapshot.ActiveHandles.Add(MoveTemp(Handle));
		}

		for (const FOpenMobileHapticPlaybackEvent& Event :
			Source.RecentPlaybackEvents)
		{
			if (!Event.Error.IsSet())
			{
				continue;
			}
			FOpenMobileHapticsDiagnosticError& Error =
				Snapshot.RecentErrors.AddDefaulted_GetRef();
			Error.TimestampSeconds = Event.TimestampSeconds;
			Error.Code = Event.Error.Code;
			Error.CommonCode = Event.Error.CommonCode;
			Error.Stage = Event.Error.Stage;
			Error.PatternOrEffect = Event.PatternOrEffect.IsNone()
				? Event.Error.FailedItem
				: Event.PatternOrEffect;
			Error.Channel = Event.Channel.IsNone()
				? Event.Error.Channel
				: Event.Channel;
			Error.ResolvedPath = Event.ResolvedPath;
			Error.Message = Event.Error.Message;
			Error.NativeDomain = Event.Error.NativeDomain;
			Error.NativeCode = Event.Error.NativeCode;
		}
		if (Source.LastError.IsSet()
			&& !Snapshot.RecentErrors.ContainsByPredicate(
				[&Source](const FOpenMobileHapticsDiagnosticError& Error)
				{
					return Error.Code == Source.LastError.Code
						&& Error.Stage == Source.LastError.Stage
						&& Error.Message == Source.LastError.Message;
				}
			))
		{
			FOpenMobileHapticsDiagnosticError& Error =
				Snapshot.RecentErrors.AddDefaulted_GetRef();
			Error.Code = Source.LastError.Code;
			Error.CommonCode = Source.LastError.CommonCode;
			Error.Stage = Source.LastError.Stage;
			Error.PatternOrEffect = Source.LastError.FailedItem;
			Error.Channel = Source.LastError.Channel;
			Error.Message = Source.LastError.Message;
			Error.NativeDomain = Source.LastError.NativeDomain;
			Error.NativeCode = Source.LastError.NativeCode;
		}
	}

	bool WriteClipboard(const FString& Text)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (ClipboardWriterForTests)
		{
			return ClipboardWriterForTests(Text);
		}
#endif
		FPlatformApplicationMisc::ClipboardCopy(*Text);
		return true;
	}

	bool WriteFile(const FString& FilePath, const FString& Text)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (FileWriterForTests)
		{
			return FileWriterForTests(FilePath, Text);
		}
#endif
		return FFileHelper::SaveStringToFile(
			Text,
			*FilePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM
		);
	}
}

FOpenMobileHapticsDiagnosticSnapshot
FOpenMobileHapticsDiagnosticsOutput::Capture()
{
	using namespace OpenMobileHapticsDiagnosticsOutputPrivate;
	check(IsInGameThread());
	FOpenMobileHapticsDiagnosticSnapshot Snapshot;
	Snapshot.CapturedAtUtc = FDateTime::UtcNow();
	const TSharedPtr<IPlugin> Plugin =
		IPluginManager::Get().FindPlugin(TEXT("OpenMobileHaptics"));
	const FString PluginDirectory = Plugin ? Plugin->GetBaseDir() : FString();
	if (Plugin)
	{
		Snapshot.PluginVersion = Plugin->GetDescriptor().VersionName;
	}

	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	if (Settings)
	{
		Snapshot.Settings.bEnabledByDefault = Settings->bEnabledByDefault;
		Snapshot.Settings.bCustomPlaybackEnabled = Settings->bEnableCustomPlayback;
		Snapshot.Settings.bAndroidCustomVibrationEnabled =
			Settings->bEnableAndroidCustomVibration;
		Snapshot.Settings.bIOSCoreHapticsEnabled =
			Settings->IOS.bEnableCoreHaptics;
		Snapshot.Settings.bIOSAHAPResourcesEnabled =
			Settings->IOS.bPackageAHAPResources;
		Snapshot.Settings.BackgroundPolicy = Settings->BackgroundPolicy;
		Snapshot.Settings.ConfiguredChannelCount = Settings->Channels.Num();
		Snapshot.Settings.ConfiguredEffectCount = Settings->EffectOverrides.Num();
		Snapshot.Settings.ConfiguredNamedLibraryCount =
			Settings->NamedLibraries.Num();
		Snapshot.Settings.MaximumActiveHandles = Settings->MaximumActiveHandles;
		Snapshot.Settings.MaximumQueuedHandles = Settings->MaximumQueuedHandles;
		Snapshot.Settings.MaximumQueueDepthPerChannel =
			Settings->MaximumQueueDepthPerChannel;
		Snapshot.Settings.MaximumPreparedPatterns =
			Settings->MaximumPreparedPatterns;
		Snapshot.Settings.MaximumPreparedPatternMemoryBytes =
			static_cast<int64>(Settings->MaximumPreparedPatternMemoryKilobytes)
			* 1024;
		Snapshot.Settings.MaximumDiagnosticEvents =
			Settings->MaximumDiagnosticEvents;
		Snapshot.Settings.MaximumFiniteRepeatCount =
			Settings->MaximumFiniteRepeatCount;
		Snapshot.Settings.MaximumContinuousDurationSeconds =
			Settings->MaximumContinuousDurationSeconds;
		CheckSettings(*Settings, Snapshot);
		CheckAndroidPackaging(*Settings, PluginDirectory, Snapshot);
		CheckApplePackaging(*Settings, PluginDirectory, Snapshot);
		CheckAssets(*Settings, Snapshot);
	}

	Snapshot.Capabilities =
		FOpenMobileHapticsBackendRegistry::GetCapabilitySnapshot();
	Snapshot.BackendName = Snapshot.Capabilities.BackendName;
	Snapshot.ApplicationState = ApplicationStateName(
		FOpenMobileHapticsBackendRegistry::GetApplicationState()
	);
	Snapshot.bBackendRecovering =
		FOpenMobileHapticsBackendRegistry::IsRecovering();
	Snapshot.bBackendShuttingDown =
		FOpenMobileHapticsBackendRegistry::IsShuttingDown();

	TMap<FName, FOpenMobileHapticChannelDiagnostics> Channels;
	for (TObjectIterator<UOpenMobileHapticsSubsystem> It; It; ++It)
	{
		UOpenMobileHapticsSubsystem* Subsystem = *It;
		if (!IsValid(Subsystem)
			|| Subsystem->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			continue;
		}
		const FOpenMobileHapticsDiagnostics Diagnostics =
			Subsystem->GetDiagnosticsNative();
		++Snapshot.SubsystemCount;
		MergeDiagnostics(Diagnostics, Snapshot, Channels);
	}

	TArray<FName> ChannelNames;
	Channels.GenerateKeyArray(ChannelNames);
	ChannelNames.Sort(FNameLexicalLess());
	for (const FName ChannelName : ChannelNames)
	{
		if (Snapshot.Channels.Num() >= MaximumChannels)
		{
			Snapshot.bTruncated = true;
			break;
		}
		Snapshot.Channels.Add(Channels.FindChecked(ChannelName));
	}

	Snapshot.RecentErrors.Sort(
		[](const FOpenMobileHapticsDiagnosticError& Left,
			const FOpenMobileHapticsDiagnosticError& Right)
		{
			if (Left.TimestampSeconds != Right.TimestampSeconds)
			{
				return Left.TimestampSeconds < Right.TimestampSeconds;
			}
			if (Left.Code != Right.Code)
			{
				return static_cast<uint8>(Left.Code)
					< static_cast<uint8>(Right.Code);
			}
			return static_cast<uint8>(Left.Stage)
				< static_cast<uint8>(Right.Stage);
		}
	);
	if (Snapshot.RecentErrors.Num() > MaximumErrors)
	{
		Snapshot.RecentErrors.RemoveAt(
			0,
			Snapshot.RecentErrors.Num() - MaximumErrors
		);
		Snapshot.bTruncated = true;
	}
	CheckPreviewReceivers(Snapshot);
	Snapshot.Issues.Sort(
		[](const FOpenMobileHapticsDiagnosticIssue& Left,
			const FOpenMobileHapticsDiagnosticIssue& Right)
		{
			if (Left.Code != Right.Code)
			{
				return Left.Code.LexicalLess(Right.Code);
			}
			return Left.Subject < Right.Subject;
		}
	);
	return Snapshot;
}

FOpenMobileHapticsDiagnosticsOutputResult
FOpenMobileHapticsDiagnosticsOutput::Serialize(
	const FOpenMobileHapticsDiagnosticSnapshot& Snapshot,
	const FOpenMobileHapticsDiagnosticExportOptions& Options,
	FString& OutJson
)
{
	using namespace OpenMobileHapticsDiagnosticsOutputPrivate;
	OutJson.Reset();
	if (Snapshot.CapturedAtUtc == FDateTime())
	{
		return MakeFailure(
			EOpenMobileHapticsDiagnosticsOutputCode::InvalidSnapshot,
			TEXT("Capture Haptics diagnostics before exporting them.")
		);
	}

	bool bTruncated = Snapshot.bTruncated;
	TMap<FName, FString> ChannelAliases;
	TMap<FName, FString> EffectAliases;
	TMap<FName, FString> PathAliases;
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), 1);
	Root->SetStringField(TEXT("capturedAtUtc"), Snapshot.CapturedAtUtc.ToIso8601());
	Root->SetStringField(
		TEXT("pluginVersion"),
		SafeString(Snapshot.PluginVersion, false, bTruncated)
	);
	Root->SetStringField(
		TEXT("backend"),
		SafeString(Snapshot.BackendName.ToString(), false, bTruncated)
	);
	Root->SetStringField(
		TEXT("applicationState"),
		SafeString(Snapshot.ApplicationState.ToString(), false, bTruncated)
	);
	Root->SetBoolField(TEXT("backendRecovering"), Snapshot.bBackendRecovering);
	Root->SetBoolField(TEXT("backendShuttingDown"), Snapshot.bBackendShuttingDown);

	const TSharedRef<FJsonObject> Settings = MakeShared<FJsonObject>();
	Settings->SetBoolField(TEXT("enabledByDefault"), Snapshot.Settings.bEnabledByDefault);
	Settings->SetBoolField(TEXT("customPlayback"), Snapshot.Settings.bCustomPlaybackEnabled);
	Settings->SetBoolField(
		TEXT("androidCustomVibration"),
		Snapshot.Settings.bAndroidCustomVibrationEnabled
	);
	Settings->SetBoolField(TEXT("iosCoreHaptics"), Snapshot.Settings.bIOSCoreHapticsEnabled);
	Settings->SetBoolField(
		TEXT("iosAhapResources"),
		Snapshot.Settings.bIOSAHAPResourcesEnabled
	);
	Settings->SetStringField(TEXT("backgroundPolicy"), EnumName(Snapshot.Settings.BackgroundPolicy));
	Settings->SetNumberField(TEXT("configuredChannels"), Snapshot.Settings.ConfiguredChannelCount);
	Settings->SetNumberField(TEXT("configuredEffects"), Snapshot.Settings.ConfiguredEffectCount);
	Settings->SetNumberField(
		TEXT("configuredNamedLibraries"),
		Snapshot.Settings.ConfiguredNamedLibraryCount
	);
	Settings->SetNumberField(TEXT("maximumActiveHandles"), Snapshot.Settings.MaximumActiveHandles);
	Settings->SetNumberField(TEXT("maximumQueuedHandles"), Snapshot.Settings.MaximumQueuedHandles);
	Settings->SetNumberField(
		TEXT("maximumQueueDepthPerChannel"),
		Snapshot.Settings.MaximumQueueDepthPerChannel
	);
	Settings->SetNumberField(
		TEXT("maximumPreparedPatterns"),
		Snapshot.Settings.MaximumPreparedPatterns
	);
	Settings->SetStringField(
		TEXT("maximumPreparedPatternMemoryBytes"),
		FString::Printf(TEXT("%lld"), Snapshot.Settings.MaximumPreparedPatternMemoryBytes)
	);
	Settings->SetNumberField(
		TEXT("maximumDiagnosticEvents"),
		Snapshot.Settings.MaximumDiagnosticEvents
	);
	Settings->SetNumberField(
		TEXT("maximumFiniteRepeatCount"),
		Snapshot.Settings.MaximumFiniteRepeatCount
	);
	Settings->SetNumberField(
		TEXT("maximumContinuousDurationSeconds"),
		Snapshot.Settings.MaximumContinuousDurationSeconds
	);
	Root->SetObjectField(TEXT("settings"), Settings);

	const FOpenMobileHapticCapabilities& Capabilities = Snapshot.Capabilities;
	const TSharedRef<FJsonObject> CapabilityObject = MakeShared<FJsonObject>();
	CapabilityObject->SetStringField(TEXT("availability"), EnumName(Capabilities.Availability));
	CapabilityObject->SetStringField(TEXT("basicVibration"), EnumName(Capabilities.BasicVibration));
	CapabilityObject->SetStringField(TEXT("semanticFeedback"), EnumName(Capabilities.SemanticFeedback));
	CapabilityObject->SetStringField(TEXT("richHaptics"), EnumName(Capabilities.RichHaptics));
	CapabilityObject->SetStringField(TEXT("amplitudeControl"), EnumName(Capabilities.AmplitudeControl));
	CapabilityObject->SetStringField(TEXT("semanticEffects"), EnumName(Capabilities.SemanticEffects));
	CapabilityObject->SetStringField(TEXT("predefinedEffects"), EnumName(Capabilities.PredefinedEffects));
	CapabilityObject->SetStringField(TEXT("waveformTiming"), EnumName(Capabilities.WaveformTiming));
	CapabilityObject->SetStringField(TEXT("looping"), EnumName(Capabilities.Looping));
	CapabilityObject->SetStringField(TEXT("primitives"), EnumName(Capabilities.Primitives));
	CapabilityObject->SetStringField(TEXT("envelopes"), EnumName(Capabilities.Envelopes));
	CapabilityObject->SetStringField(TEXT("frequencyControl"), EnumName(Capabilities.FrequencyControl));
	CapabilityObject->SetStringField(TEXT("transientEvents"), EnumName(Capabilities.TransientEvents));
	CapabilityObject->SetStringField(TEXT("continuousEvents"), EnumName(Capabilities.ContinuousEvents));
	CapabilityObject->SetStringField(TEXT("dynamicParameters"), EnumName(Capabilities.DynamicParameters));
	CapabilityObject->SetStringField(TEXT("audioEvents"), EnumName(Capabilities.AudioEvents));
	CapabilityObject->SetStringField(TEXT("ahap"), EnumName(Capabilities.AHAP));
	CapabilityObject->SetStringField(TEXT("scheduling"), EnumName(Capabilities.Scheduling));
	CapabilityObject->SetStringField(TEXT("mixing"), EnumName(Capabilities.Mixing));
	CapabilityObject->SetStringField(TEXT("backgroundAlerts"), EnumName(Capabilities.BackgroundAlerts));
	CapabilityObject->SetStringField(TEXT("pause"), EnumName(Capabilities.Pause));
	CapabilityObject->SetStringField(TEXT("resume"), EnumName(Capabilities.Resume));
	CapabilityObject->SetStringField(TEXT("seek"), EnumName(Capabilities.Seek));
	CapabilityObject->SetNumberField(TEXT("primitiveCount"), Capabilities.PrimitiveSupport.Num());
	CapabilityObject->SetNumberField(TEXT("presetCount"), Capabilities.PresetSupport.Num());
	Root->SetObjectField(TEXT("capabilities"), CapabilityObject);

	const TSharedRef<FJsonObject> Runtime = MakeShared<FJsonObject>();
	Runtime->SetNumberField(TEXT("subsystems"), Snapshot.SubsystemCount);
	Runtime->SetNumberField(TEXT("activePlaybacks"), Snapshot.ActivePlaybackCount);
	Runtime->SetNumberField(TEXT("queuedPlaybacks"), Snapshot.QueuedPlaybackCount);
	Runtime->SetNumberField(
		TEXT("preparedNamedPatterns"),
		Snapshot.PreparedNamedPatternCount
	);
	Runtime->SetStringField(
		TEXT("fallbackPlaybacks"),
		FString::Printf(TEXT("%lld"), Snapshot.FallbackPlaybackCount)
	);
	Root->SetObjectField(TEXT("runtime"), Runtime);

	const FOpenMobileHapticsPerformanceDiagnostics& Performance =
		Snapshot.Performance;
	const TSharedRef<FJsonObject> PerformanceObject = MakeShared<FJsonObject>();
	PerformanceObject->SetStringField(TEXT("droppedRequests"), FString::Printf(TEXT("%lld"), Performance.DroppedRequestCount));
	PerformanceObject->SetNumberField(TEXT("peakQueuedPlaybacks"), Performance.PeakQueuedPlaybackCount);
	PerformanceObject->SetStringField(TEXT("timelineCacheHits"), FString::Printf(TEXT("%lld"), Performance.TimelineCacheHitCount));
	PerformanceObject->SetStringField(TEXT("timelineCacheMisses"), FString::Printf(TEXT("%lld"), Performance.TimelineCacheMissCount));
	PerformanceObject->SetStringField(TEXT("timelineCacheEvictions"), FString::Printf(TEXT("%lld"), Performance.TimelineCacheEvictionCount));
	PerformanceObject->SetNumberField(TEXT("timelineCacheEntries"), Performance.TimelineCacheEntryCount);
	PerformanceObject->SetStringField(TEXT("timelineCacheMemoryBytes"), FString::Printf(TEXT("%lld"), Performance.TimelineCacheMemoryBytes));
	PerformanceObject->SetNumberField(TEXT("timelineCacheMaximumEntries"), Performance.TimelineCacheMaximumEntryCount);
	PerformanceObject->SetStringField(TEXT("timelineCacheMaximumMemoryBytes"), FString::Printf(TEXT("%lld"), Performance.TimelineCacheMaximumMemoryBytes));
	PerformanceObject->SetStringField(TEXT("preparations"), FString::Printf(TEXT("%lld"), Performance.PreparationCount));
	PerformanceObject->SetNumberField(TEXT("lastPreparationLatencyMs"), Performance.LastPreparationLatencyMilliseconds);
	PerformanceObject->SetNumberField(TEXT("maximumPreparationLatencyMs"), Performance.MaximumPreparationLatencyMilliseconds);
	PerformanceObject->SetStringField(TEXT("nativeSubmissions"), FString::Printf(TEXT("%lld"), Performance.NativeSubmissionCount));
	PerformanceObject->SetNumberField(TEXT("lastNativeSubmissionLatencyMs"), Performance.LastNativeSubmissionLatencyMilliseconds);
	PerformanceObject->SetNumberField(TEXT("maximumNativeSubmissionLatencyMs"), Performance.MaximumNativeSubmissionLatencyMilliseconds);
	Root->SetObjectField(TEXT("performance"), PerformanceObject);

	TArray<TSharedPtr<FJsonValue>> Channels;
	for (int32 Index = 0;
		Index < Snapshot.Channels.Num() && Index < MaximumChannels;
		++Index)
	{
		const FOpenMobileHapticChannelDiagnostics& Source = Snapshot.Channels[Index];
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(
			TEXT("channel"),
			NameForExport(
				Source.Channel,
				TEXT("channel"),
				Options.bIncludeProjectNames,
				ChannelAliases,
				bTruncated
			)
		);
		Entry->SetNumberField(TEXT("active"), Source.ActivePlaybackCount);
		Entry->SetNumberField(TEXT("queued"), Source.QueuedPlaybackCount);
		Channels.Add(MakeShared<FJsonValueObject>(Entry));
	}
	bTruncated |= Snapshot.Channels.Num() > MaximumChannels;
	Root->SetArrayField(TEXT("channels"), Channels);

	TArray<TSharedPtr<FJsonValue>> Handles;
	for (int32 Index = 0;
		Index < Snapshot.ActiveHandles.Num() && Index < MaximumHandles;
		++Index)
	{
		const FOpenMobileHapticHandleDiagnostics& Source =
			Snapshot.ActiveHandles[Index];
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("ordinal"), Index + 1);
		Entry->SetStringField(TEXT("state"), EnumName(Source.State));
		Entry->SetBoolField(TEXT("queued"), Source.bQueued);
		Entry->SetStringField(
			TEXT("channel"),
			NameForExport(Source.Channel, TEXT("channel"), Options.bIncludeProjectNames, ChannelAliases, bTruncated)
		);
		Entry->SetStringField(
			TEXT("patternOrEffect"),
			NameForExport(Source.PatternOrEffect, TEXT("effect"), Options.bIncludeProjectNames, EffectAliases, bTruncated)
		);
		Entry->SetStringField(
			TEXT("resolvedPath"),
			NameForExport(Source.ResolvedPath, TEXT("path"), Options.bIncludeProjectNames, PathAliases, bTruncated)
		);
		Handles.Add(MakeShared<FJsonValueObject>(Entry));
	}
	bTruncated |= Snapshot.ActiveHandles.Num() > MaximumHandles;
	Root->SetArrayField(TEXT("activeHandles"), Handles);

	TArray<TSharedPtr<FJsonValue>> Errors;
	const int32 FirstError = FMath::Max(0, Snapshot.RecentErrors.Num() - MaximumErrors);
	for (int32 Index = FirstError; Index < Snapshot.RecentErrors.Num(); ++Index)
	{
		const FOpenMobileHapticsDiagnosticError& Source = Snapshot.RecentErrors[Index];
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetNumberField(TEXT("timestampSeconds"), Source.TimestampSeconds);
		Entry->SetStringField(TEXT("severity"), SeverityName(ErrorSeverity(Source.Code)));
		Entry->SetStringField(TEXT("code"), EnumName(Source.Code));
		Entry->SetStringField(TEXT("commonCode"), EnumName(Source.CommonCode));
		Entry->SetStringField(TEXT("stage"), EnumName(Source.Stage));
		Entry->SetStringField(
			TEXT("patternOrEffect"),
			NameForExport(Source.PatternOrEffect, TEXT("effect"), Options.bIncludeProjectNames, EffectAliases, bTruncated)
		);
		Entry->SetStringField(
			TEXT("channel"),
			NameForExport(Source.Channel, TEXT("channel"), Options.bIncludeProjectNames, ChannelAliases, bTruncated)
		);
		Entry->SetStringField(
			TEXT("resolvedPath"),
			NameForExport(Source.ResolvedPath, TEXT("path"), Options.bIncludeProjectNames, PathAliases, bTruncated)
		);
		if (Options.bIncludeImportedMetadata)
		{
			Entry->SetStringField(TEXT("message"), SafeString(Source.Message, Options.bIncludeFilePaths, bTruncated));
			Entry->SetStringField(TEXT("nativeDomain"), SafeString(Source.NativeDomain, Options.bIncludeFilePaths, bTruncated));
			Entry->SetStringField(TEXT("nativeCode"), SafeString(Source.NativeCode, Options.bIncludeFilePaths, bTruncated));
		}
		else
		{
			Entry->SetStringField(TEXT("message"), TEXT("<redacted>"));
			Entry->SetStringField(TEXT("nativeDomain"), TEXT("<redacted>"));
			Entry->SetStringField(TEXT("nativeCode"), TEXT("<redacted>"));
		}
		Errors.Add(MakeShared<FJsonValueObject>(Entry));
	}
	bTruncated |= Snapshot.RecentErrors.Num() > MaximumErrors;
	Root->SetArrayField(TEXT("recentErrors"), Errors);

	TArray<TSharedPtr<FJsonValue>> Issues;
	for (int32 Index = 0;
		Index < Snapshot.Issues.Num() && Index < MaximumIssues;
		++Index)
	{
		const FOpenMobileHapticsDiagnosticIssue& Source = Snapshot.Issues[Index];
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("code"), Source.Code.ToString());
		Entry->SetStringField(TEXT("severity"), SeverityName(Source.Severity));
		Entry->SetStringField(TEXT("summary"), SafeString(Source.Summary, false, bTruncated));
		const bool bRevealSubject =
			(!Source.bSubjectIsProjectName || Options.bIncludeProjectNames)
			&& (!Source.bSubjectIsFilePath || Options.bIncludeFilePaths)
			&& (!Source.bSubjectIsImportedMetadata || Options.bIncludeImportedMetadata);
		Entry->SetStringField(
			TEXT("subject"),
			bRevealSubject
				? SafeString(Source.Subject, Options.bIncludeFilePaths, bTruncated)
				: TEXT("<redacted>")
		);
		Issues.Add(MakeShared<FJsonValueObject>(Entry));
	}
	bTruncated |= Snapshot.Issues.Num() > MaximumIssues;
	Root->SetArrayField(TEXT("issues"), Issues);
	Root->SetBoolField(TEXT("truncated"), bTruncated);

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(Root, Writer)
		|| FTCHARToUTF8(*OutJson).Length() > MaximumOutputBytes)
	{
		OutJson.Reset();
		return MakeFailure(
			EOpenMobileHapticsDiagnosticsOutputCode::LimitExceeded,
			TEXT("The Haptics diagnostics output exceeds its size limit.")
		);
	}
	return {};
}

FOpenMobileHapticsDiagnosticsOutputResult
FOpenMobileHapticsDiagnosticsOutput::CopyToClipboard(
	const FOpenMobileHapticsDiagnosticSnapshot& Snapshot,
	const FOpenMobileHapticsDiagnosticExportOptions& Options
)
{
	using namespace OpenMobileHapticsDiagnosticsOutputPrivate;
	FString Json;
	FOpenMobileHapticsDiagnosticsOutputResult Result =
		Serialize(Snapshot, Options, Json);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	return WriteClipboard(Json)
		? FOpenMobileHapticsDiagnosticsOutputResult()
		: MakeFailure(
			EOpenMobileHapticsDiagnosticsOutputCode::WriteFailed,
			TEXT("Could not copy Haptics diagnostics.")
		);
}

FOpenMobileHapticsDiagnosticsOutputResult
FOpenMobileHapticsDiagnosticsOutput::ExportToFile(
	const FOpenMobileHapticsDiagnosticSnapshot& Snapshot,
	const FString& FilePath,
	const FOpenMobileHapticsDiagnosticExportOptions& Options
)
{
	using namespace OpenMobileHapticsDiagnosticsOutputPrivate;
	if (FilePath.IsEmpty())
	{
		return MakeFailure(
			EOpenMobileHapticsDiagnosticsOutputCode::WriteFailed,
			TEXT("Choose a Haptics diagnostics export path.")
		);
	}
	FString Json;
	FOpenMobileHapticsDiagnosticsOutputResult Result =
		Serialize(Snapshot, Options, Json);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	return WriteFile(FilePath, Json)
		? FOpenMobileHapticsDiagnosticsOutputResult()
		: MakeFailure(
			EOpenMobileHapticsDiagnosticsOutputCode::WriteFailed,
			TEXT("Could not write Haptics diagnostics.")
		);
}

int32 FOpenMobileHapticsDiagnosticsOutput::GetMaximumOutputBytes()
{
	return OpenMobileHapticsDiagnosticsOutputPrivate::MaximumOutputBytes;
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileHapticsDiagnosticsOutput::SetClipboardWriterForTests(
	TFunction<bool(const FString&)>&& Writer
)
{
	OpenMobileHapticsDiagnosticsOutputPrivate::ClipboardWriterForTests =
		MoveTemp(Writer);
}

void FOpenMobileHapticsDiagnosticsOutput::SetFileWriterForTests(
	TFunction<bool(const FString&, const FString&)>&& Writer
)
{
	OpenMobileHapticsDiagnosticsOutputPrivate::FileWriterForTests =
		MoveTemp(Writer);
}

void FOpenMobileHapticsDiagnosticsOutput::ResetWritersForTests()
{
	OpenMobileHapticsDiagnosticsOutputPrivate::ClipboardWriterForTests = {};
	OpenMobileHapticsDiagnosticsOutputPrivate::FileWriterForTests = {};
}
#endif
