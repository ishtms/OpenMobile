#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceDiagnostics.h"

enum class EOpenMobileDeviceDiagnosticsOutputCode : uint8
{
	Succeeded,
	InvalidSnapshot,
	SerializationFailed,
	TooLarge,
	ClipboardWriteFailed,
	FileWriteFailed
};

struct OPENMOBILEDEVICEEDITOR_API FOpenMobileDeviceDiagnosticsOutputResult
{
	EOpenMobileDeviceDiagnosticsOutputCode Code =
		EOpenMobileDeviceDiagnosticsOutputCode::Succeeded;
	FString Message;

	bool IsSuccess() const
	{
		return Code == EOpenMobileDeviceDiagnosticsOutputCode::Succeeded;
	}
};

class OPENMOBILEDEVICEEDITOR_API FOpenMobileDeviceDiagnosticsOutput final
{
public:
	static FOpenMobileDeviceDiagnosticsSnapshot Capture();
	static FOpenMobileDeviceDiagnosticsOutputResult Serialize(
		const FOpenMobileDeviceDiagnosticsSnapshot& Snapshot,
		FString& OutJson
	);
	static FOpenMobileDeviceDiagnosticsOutputResult CopyToClipboard(
		const FOpenMobileDeviceDiagnosticsSnapshot& Snapshot
	);
	static FOpenMobileDeviceDiagnosticsOutputResult ExportToFile(
		const FOpenMobileDeviceDiagnosticsSnapshot& Snapshot,
		const FString& FilePath
	);
	static bool IsStale(const FOpenMobileDeviceDiagnosticsSnapshot& Snapshot);
	static int32 GetMaximumExportBytes();

#if WITH_DEV_AUTOMATION_TESTS
	static void SetClipboardWriterForTests(
		TFunction<bool(const FString&)> Writer
	);
	static void SetFileWriterForTests(
		TFunction<bool(const FString&, const FString&)> Writer
	);
	static void ResetWritersForTests();
#endif
};
