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

	bool IsSuccess() const
	{
		return Code == EOpenMobileHapticsDiagnosticsOutputCode::Success;
	}
};

class OPENMOBILEHAPTICSEDITOR_API FOpenMobileHapticsDiagnosticsOutput final
{
public:
	static FOpenMobileHapticsDiagnosticSnapshot Capture();
	static FOpenMobileHapticsDiagnosticsOutputResult Serialize(
		const FOpenMobileHapticsDiagnosticSnapshot& Snapshot,
		const FOpenMobileHapticsDiagnosticExportOptions& Options,
		FString& OutJson
	);
	static FOpenMobileHapticsDiagnosticsOutputResult CopyToClipboard(
		const FOpenMobileHapticsDiagnosticSnapshot& Snapshot,
		const FOpenMobileHapticsDiagnosticExportOptions& Options = {}
	);
	static FOpenMobileHapticsDiagnosticsOutputResult ExportToFile(
		const FOpenMobileHapticsDiagnosticSnapshot& Snapshot,
		const FString& FilePath,
		const FOpenMobileHapticsDiagnosticExportOptions& Options = {}
	);
	static int32 GetMaximumOutputBytes();

#if WITH_DEV_AUTOMATION_TESTS
	static void SetClipboardWriterForTests(
		TFunction<bool(const FString&)>&& Writer
	);
	static void SetFileWriterForTests(
		TFunction<bool(const FString&, const FString&)>&& Writer
	);
	static void ResetWritersForTests();
#endif
};
