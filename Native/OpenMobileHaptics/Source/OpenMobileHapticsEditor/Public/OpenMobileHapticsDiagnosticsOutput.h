#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsSettings.h"
#include "OpenMobileHapticsTypes.h"

enum class EOpenMobileHapticsDiagnosticSeverity : uint8
{
	Info,
	Warning,
	Error,
	Fatal
};

enum class EOpenMobileHapticsDiagnosticsOutputCode : uint8
{
	Success,
	InvalidSnapshot,
	LimitExceeded,
	WriteFailed
};

struct OPENMOBILEHAPTICSEDITOR_API
FOpenMobileHapticsDiagnosticExportOptions
{
	bool bIncludeProjectNames = false;
	bool bIncludeFilePaths = false;
	bool bIncludeImportedMetadata = false;
};

struct OPENMOBILEHAPTICSEDITOR_API
FOpenMobileHapticsDiagnosticSettingsSnapshot
{
	bool bEnabledByDefault = false;
	bool bCustomPlaybackEnabled = false;
	bool bAndroidCustomVibrationEnabled = false;
	bool bIOSCoreHapticsEnabled = false;
	bool bIOSAHAPResourcesEnabled = false;
	EOpenMobileHapticBackgroundPolicy BackgroundPolicy =
		EOpenMobileHapticBackgroundPolicy::StopAll;
	int32 ConfiguredChannelCount = 0;
	int32 ConfiguredEffectCount = 0;
	int32 ConfiguredNamedLibraryCount = 0;
	int32 MaximumActiveHandles = 0;
	int32 MaximumQueuedHandles = 0;
	int32 MaximumQueueDepthPerChannel = 0;
	int32 MaximumPreparedPatterns = 0;
	int64 MaximumPreparedPatternMemoryBytes = 0;
	int32 MaximumDiagnosticEvents = 0;
	int32 MaximumFiniteRepeatCount = 0;
	double MaximumContinuousDurationSeconds = 0.0;
};

struct OPENMOBILEHAPTICSEDITOR_API FOpenMobileHapticsDiagnosticError
{
	double TimestampSeconds = 0.0;
	EOpenMobileHapticErrorCode Code = EOpenMobileHapticErrorCode::None;
	EOpenMobileErrorCode CommonCode = EOpenMobileErrorCode::None;
	EOpenMobileHapticFailureStage Stage =
		EOpenMobileHapticFailureStage::None;
	FName PatternOrEffect;
	FName Channel;
	FName ResolvedPath;
	FString Message;
	FString NativeDomain;
	FString NativeCode;
};

struct OPENMOBILEHAPTICSEDITOR_API FOpenMobileHapticsDiagnosticIssue
{
	FName Code;
	EOpenMobileHapticsDiagnosticSeverity Severity =
		EOpenMobileHapticsDiagnosticSeverity::Warning;
	FString Summary;
	FString Subject;
	bool bSubjectIsProjectName = false;
	bool bSubjectIsFilePath = false;
	bool bSubjectIsImportedMetadata = false;
};

struct OPENMOBILEHAPTICSEDITOR_API FOpenMobileHapticsDiagnosticSnapshot
{
	FDateTime CapturedAtUtc;
	FString PluginVersion;
	FOpenMobileHapticsDiagnosticSettingsSnapshot Settings;
	FOpenMobileHapticCapabilities Capabilities;
	FName BackendName;
	FName ApplicationState = TEXT("Active");
	bool bBackendRecovering = false;
	bool bBackendShuttingDown = false;
	int32 SubsystemCount = 0;
	int32 ActivePlaybackCount = 0;
	int32 QueuedPlaybackCount = 0;
	int32 PreparedNamedPatternCount = 0;
	int64 FallbackPlaybackCount = 0;
	FOpenMobileHapticsPerformanceDiagnostics Performance;
	TArray<FOpenMobileHapticChannelDiagnostics> Channels;
	TArray<FOpenMobileHapticHandleDiagnostics> ActiveHandles;
	TArray<FOpenMobileHapticsDiagnosticError> RecentErrors;
	TArray<FOpenMobileHapticsDiagnosticIssue> Issues;
	bool bTruncated = false;
};

struct OPENMOBILEHAPTICSEDITOR_API
FOpenMobileHapticsDiagnosticsOutputResult
{
	EOpenMobileHapticsDiagnosticsOutputCode Code =
		EOpenMobileHapticsDiagnosticsOutputCode::Success;
	FString Message;

	/** Lets callers branch on the stable result code, message text is only there for a person to read. */
	bool IsSuccess() const
	{
		return Code == EOpenMobileHapticsDiagnosticsOutputCode::Success;
	}
};

class OPENMOBILEHAPTICSEDITOR_API FOpenMobileHapticsDiagnosticsOutput final
{
public:
	/** Takes one bounded snapshot across settings, backends, and live Game Instances so the exported values belong to the same capture. */
	static FOpenMobileHapticsDiagnosticSnapshot Capture();
	/** Applies the opt-in disclosure flags while producing deterministic JSON, paths and project names stay hidden by default. */
	static FOpenMobileHapticsDiagnosticsOutputResult Serialize(
		const FOpenMobileHapticsDiagnosticSnapshot& Snapshot,
		const FOpenMobileHapticsDiagnosticExportOptions& Options,
		FString& OutJson
	);
	/** Runs the same redaction and size checks before text reaches the system clipboard. */
	static FOpenMobileHapticsDiagnosticsOutputResult CopyToClipboard(
		const FOpenMobileHapticsDiagnosticSnapshot& Snapshot,
		const FOpenMobileHapticsDiagnosticExportOptions& Options = {}
	);
	/** Writes only serialized diagnostics to the path the user picked, it won't choose or create an export location for you. */
	static FOpenMobileHapticsDiagnosticsOutputResult ExportToFile(
		const FOpenMobileHapticsDiagnosticSnapshot& Snapshot,
		const FString& FilePath,
		const FOpenMobileHapticsDiagnosticExportOptions& Options = {}
	);
	/** Gives editor UI the exact byte cap used by serialization so it can explain an oversized export properly. */
	static int32 GetMaximumOutputBytes();

#if WITH_DEV_AUTOMATION_TESTS
	/** Replaces clipboard access during tests, nobody wants a test quietly overwriting real clipboard stuff. */
	static void SetClipboardWriterForTests(
		TFunction<bool(const FString&)>&& Writer
	);
	/** Replaces disk output during tests so failure paths don't depend on a developer's filesystem. */
	static void SetFileWriterForTests(
		TFunction<bool(const FString&, const FString&)>&& Writer
	);
	/** Restores the real writers after a test override, otherwise the next test inherits somebody else's callback. */
	static void ResetWritersForTests();
#endif
};
