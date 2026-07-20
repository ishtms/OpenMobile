#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorDiagnostics.h"

enum class EOpenMobileSensorsDiagnosticsOutputCode : uint8
{
	Success,
	InvalidSnapshot,
	LimitExceeded,
	WriteFailed
};

struct OPENMOBILESENSORSEDITOR_API FOpenMobileSensorsDiagnosticsOutputResult
{
	EOpenMobileSensorsDiagnosticsOutputCode Code =
		EOpenMobileSensorsDiagnosticsOutputCode::Success;
	FString Message;

	bool IsSuccess() const
	{
		return Code == EOpenMobileSensorsDiagnosticsOutputCode::Success;
	}
};

class OPENMOBILESENSORSEDITOR_API FOpenMobileSensorsDiagnosticsOutput final
{
public:
	static FOpenMobileSensorDiagnosticsSnapshot Capture();
	static FOpenMobileSensorsDiagnosticsOutputResult Serialize(
		const FOpenMobileSensorDiagnosticsSnapshot& Snapshot,
		FString& OutJson
	);
	static FOpenMobileSensorsDiagnosticsOutputResult CopyToClipboard(
		const FOpenMobileSensorDiagnosticsSnapshot& Snapshot
	);
	static FOpenMobileSensorsDiagnosticsOutputResult ExportToFile(
		const FOpenMobileSensorDiagnosticsSnapshot& Snapshot,
		const FString& FilePath
	);

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
