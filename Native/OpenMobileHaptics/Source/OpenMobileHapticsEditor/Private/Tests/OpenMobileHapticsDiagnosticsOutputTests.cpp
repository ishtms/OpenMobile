#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileHapticsDiagnosticsOutput.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsDiagnosticsOutputTest,
	"OpenMobile.Haptics.Diagnostics.Output",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsDiagnosticsOutputTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);

	const FOpenMobileHapticsDiagnosticSnapshot Captured =
		FOpenMobileHapticsDiagnosticsOutput::Capture();
	TestNotEqual(
		TEXT("Capture records a timestamp"),
		Captured.CapturedAtUtc,
		FDateTime()
	);
	FString CapturedJson;
	FOpenMobileHapticsDiagnosticsOutputResult Result =
		FOpenMobileHapticsDiagnosticsOutput::Serialize(
			Captured,
			{},
			CapturedJson
		);
	TestEqual(
		TEXT("Captured diagnostics serialize"),
		Result.Code,
		EOpenMobileHapticsDiagnosticsOutputCode::Success
	);

	FOpenMobileHapticsDiagnosticSnapshot Snapshot;
	Snapshot.CapturedAtUtc = FDateTime(2026, 8, 23, 12, 0, 0);
	Snapshot.PluginVersion = FString::ChrN(500, TEXT('V'));
	Snapshot.BackendName = TEXT("MockHaptics");
	Snapshot.ApplicationState = TEXT("Background");
	Snapshot.bBackendRecovering = true;
	Snapshot.SubsystemCount = 2;
	Snapshot.ActivePlaybackCount = 3;
	Snapshot.QueuedPlaybackCount = 4;
	Snapshot.PreparedNamedPatternCount = 5;
	Snapshot.FallbackPlaybackCount = 6;
	Snapshot.Settings.bEnabledByDefault = true;
	Snapshot.Settings.bCustomPlaybackEnabled = true;
	Snapshot.Settings.ConfiguredChannelCount = 1;
	Snapshot.Settings.MaximumActiveHandles = 16;
	Snapshot.Capabilities.Availability =
		EOpenMobileHapticAvailability::RichHaptics;
	Snapshot.Capabilities.BasicVibration =
		EOpenMobileHapticSupportState::Supported;
	Snapshot.Capabilities.Detail = TEXT("private device capability detail");

	FOpenMobileHapticChannelDiagnostics& Channel =
		Snapshot.Channels.AddDefaulted_GetRef();
	Channel.Channel = TEXT("PrivateGameplayChannel");
	Channel.ActivePlaybackCount = 2;
	Channel.QueuedPlaybackCount = 1;

	FOpenMobileHapticHandleDiagnostics& Handle =
		Snapshot.ActiveHandles.AddDefaulted_GetRef();
	Handle.Ordinal = 42;
	Handle.State = EOpenMobileHapticPlaybackState::Started;
	Handle.Channel = Channel.Channel;
	Handle.PatternOrEffect = TEXT("ProjectSecretEffect");
	Handle.ResolvedPath = TEXT("NativePrivatePath");

	FOpenMobileHapticsDiagnosticError& Error =
		Snapshot.RecentErrors.AddDefaulted_GetRef();
	Error.TimestampSeconds = 123.0;
	Error.Code = EOpenMobileHapticErrorCode::NativeEngineFailure;
	Error.CommonCode = EOpenMobileErrorCode::NativeFailure;
	Error.Stage = EOpenMobileHapticFailureStage::NativeSubmission;
	Error.PatternOrEffect = Handle.PatternOrEffect;
	Error.Channel = Handle.Channel;
	Error.ResolvedPath = Handle.ResolvedPath;
	Error.Message = TEXT("Failed /Users/private/SecretEffect.ahap");
	Error.NativeDomain = TEXT("com.project.private");
	Error.NativeCode = TEXT("secret-17");

	FOpenMobileHapticsDiagnosticIssue& PrivateIssue =
		Snapshot.Issues.AddDefaulted_GetRef();
	PrivateIssue.Code = TEXT("Haptics.Asset.InvalidPattern");
	PrivateIssue.Severity = EOpenMobileHapticsDiagnosticSeverity::Fatal;
	PrivateIssue.Summary = TEXT("A Haptics pattern asset failed validation.");
	PrivateIssue.Subject = TEXT("/Game/Private/SecretEffect.SecretEffect");
	PrivateIssue.bSubjectIsProjectName = true;
	PrivateIssue.bSubjectIsFilePath = true;
	PrivateIssue.bSubjectIsImportedMetadata = true;
	for (int32 Index = 0; Index < 80; ++Index)
	{
		FOpenMobileHapticsDiagnosticIssue& Issue =
			Snapshot.Issues.AddDefaulted_GetRef();
		Issue.Code = *FString::Printf(TEXT("Haptics.Test.%d"), Index);
		Issue.Severity = EOpenMobileHapticsDiagnosticSeverity::Warning;
		Issue.Summary = FString::ChrN(500, TEXT('S'));
	}

	FString RedactedJson;
	Result = FOpenMobileHapticsDiagnosticsOutput::Serialize(
		Snapshot,
		{},
		RedactedJson
	);
	TestEqual(
		TEXT("Redacted diagnostics serialize"),
		Result.Code,
		EOpenMobileHapticsDiagnosticsOutputCode::Success
	);
	for (const FString& Required : {
		FString(TEXT("channel-1")),
		FString(TEXT("effect-1")),
		FString(TEXT("path-1")),
		FString(TEXT("NativeEngineFailure")),
		FString(TEXT("\"severity\": \"Error\"")),
		FString(TEXT("\"severity\": \"Fatal\"")),
		FString(TEXT("Haptics.Asset.InvalidPattern")),
		FString(TEXT("\"truncated\": true"))
	})
	{
		TestTrue(
			*FString::Printf(TEXT("Redacted report contains %s"), *Required),
			RedactedJson.Contains(Required)
		);
	}
	for (const FString& Forbidden : {
		FString(TEXT("PrivateGameplayChannel")),
		FString(TEXT("ProjectSecretEffect")),
		FString(TEXT("NativePrivatePath")),
		FString(TEXT("SecretEffect.ahap")),
		FString(TEXT("com.project.private")),
		FString(TEXT("secret-17")),
		FString(TEXT("/Game/Private")),
		FString(TEXT("private device capability detail"))
	})
	{
		TestFalse(
			*FString::Printf(TEXT("Redacted report excludes %s"), *Forbidden),
			RedactedJson.Contains(Forbidden)
		);
	}
	TestTrue(
		TEXT("Report stays within the byte limit"),
		FTCHARToUTF8(*RedactedJson).Length()
			<= FOpenMobileHapticsDiagnosticsOutput::GetMaximumOutputBytes()
	);

	FString RepeatedJson;
	Result = FOpenMobileHapticsDiagnosticsOutput::Serialize(
		Snapshot,
		{},
		RepeatedJson
	);
	TestTrue(TEXT("Serialization is deterministic"), RedactedJson == RepeatedJson);

	FOpenMobileHapticsDiagnosticExportOptions FullOptions;
	FullOptions.bIncludeProjectNames = true;
	FullOptions.bIncludeFilePaths = true;
	FullOptions.bIncludeImportedMetadata = true;
	FString FullJson;
	Result = FOpenMobileHapticsDiagnosticsOutput::Serialize(
		Snapshot,
		FullOptions,
		FullJson
	);
	TestEqual(
		TEXT("Explicit diagnostic fields serialize"),
		Result.Code,
		EOpenMobileHapticsDiagnosticsOutputCode::Success
	);
	for (const FString& Revealed : {
		FString(TEXT("PrivateGameplayChannel")),
		FString(TEXT("ProjectSecretEffect")),
		FString(TEXT("NativePrivatePath")),
		FString(TEXT("SecretEffect.ahap")),
		FString(TEXT("com.project.private")),
		FString(TEXT("secret-17")),
		FString(TEXT("/Game/Private"))
	})
	{
		TestTrue(
			*FString::Printf(TEXT("Explicit report includes %s"), *Revealed),
			FullJson.Contains(Revealed)
		);
	}

	FOpenMobileHapticsDiagnosticSnapshot InvalidSnapshot;
	FString InvalidJson;
	Result = FOpenMobileHapticsDiagnosticsOutput::Serialize(
		InvalidSnapshot,
		{},
		InvalidJson
	);
	TestEqual(
		TEXT("Uncaptured snapshots are rejected"),
		Result.Code,
		EOpenMobileHapticsDiagnosticsOutputCode::InvalidSnapshot
	);

	FOpenMobileHapticsDiagnosticsOutput::SetClipboardWriterForTests(
		[](const FString& Text)
		{
			return !Text.IsEmpty();
		}
	);
	Result = FOpenMobileHapticsDiagnosticsOutput::CopyToClipboard(Snapshot);
	TestEqual(
		TEXT("Clipboard output succeeds through the writer"),
		Result.Code,
		EOpenMobileHapticsDiagnosticsOutputCode::Success
	);
	FOpenMobileHapticsDiagnosticsOutput::SetFileWriterForTests(
		[](const FString& Path, const FString& Text)
		{
			return Path.IsEmpty() || Text.IsEmpty();
		}
	);
	Result = FOpenMobileHapticsDiagnosticsOutput::ExportToFile(
		Snapshot,
		TEXT("/tmp/openmobile-haptics-diagnostics.json")
	);
	TestEqual(
		TEXT("File writer failures are typed"),
		Result.Code,
		EOpenMobileHapticsDiagnosticsOutputCode::WriteFailed
	);
	FOpenMobileHapticsDiagnosticsOutput::ResetWritersForTests();
	return true;
}

#endif
