#include "OpenMobileSensorsDiagnosticsOutput.h"

#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenMobileSensorsDiagnosticsService.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace OpenMobileSensorsDiagnosticsOutputPrivate
{
	constexpr int32 MaximumOutputBytes = 128 * 1024;
	constexpr int32 MaximumStringLength = 256;
	constexpr int32 MaximumCapabilities = 128;
	constexpr int32 MaximumMetadata = 128;
	constexpr int32 MaximumStreams = 128;
	constexpr int32 MaximumPhysicalStreams = 64;
	constexpr int32 MaximumErrors = 32;

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

	FString SafeString(const FString& Value, bool& bTruncated)
	{
		FString Result = Value;
		Result.ReplaceInline(TEXT("\r"), TEXT(" "));
		Result.ReplaceInline(TEXT("\n"), TEXT(" "));
		Result.ReplaceInline(TEXT("\t"), TEXT(" "));
		const FString ProjectDirectory = FPaths::ProjectDir();
		const FString UserDirectory = FPlatformProcess::UserDir();
		if (Result.Contains(TEXT("://"))
			|| (!ProjectDirectory.IsEmpty()
				&& Result.Contains(ProjectDirectory))
			|| (!UserDirectory.IsEmpty() && Result.Contains(UserDirectory))
			|| (!Result.IsEmpty() && !FPaths::IsRelative(Result)))
		{
			bTruncated = true;
			return TEXT("<redacted>");
		}
		if (Result.Len() > MaximumStringLength)
		{
			Result.LeftInline(MaximumStringLength);
			bTruncated = true;
		}
		return Result;
	}

	FString SensorName(const FOpenMobileSensorIdentifier& Sensor)
	{
		const FName StableName = FOpenMobileSensorTypes::GetStableName(
			Sensor.Type
		);
		return Sensor.InstanceId.IsNone()
			? StableName.ToString()
			: FString::Printf(
				TEXT("%s:%s"),
				*StableName.ToString(),
				*Sensor.InstanceId.ToString()
			);
	}

	FOpenMobileSensorsDiagnosticsOutputResult MakeFailure(
		EOpenMobileSensorsDiagnosticsOutputCode Code,
		const TCHAR* Message
	)
	{
		FOpenMobileSensorsDiagnosticsOutputResult Result;
		Result.Code = Code;
		Result.Message = Message;
		return Result;
	}

	void SetOptionalNumber(
		const TSharedRef<FJsonObject>& Object,
		const TCHAR* Field,
		const FOpenMobileSensorOptionalNumber& Value
	)
	{
		if (Value.bAvailable && FMath::IsFinite(Value.Value))
		{
			Object->SetNumberField(Field, Value.Value);
		}
		else
		{
			Object->SetField(Field, MakeShared<FJsonValueNull>());
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

FOpenMobileSensorDiagnosticsSnapshot
FOpenMobileSensorsDiagnosticsOutput::Capture()
{
	return FOpenMobileSensorsDiagnosticsService::Capture();
}

FOpenMobileSensorsDiagnosticsOutputResult
FOpenMobileSensorsDiagnosticsOutput::Serialize(
	const FOpenMobileSensorDiagnosticsSnapshot& Snapshot,
	FString& OutJson
)
{
	using namespace OpenMobileSensorsDiagnosticsOutputPrivate;
	OutJson.Reset();
	if (Snapshot.CapturedAtUtc.IsEmpty())
	{
		return MakeFailure(
			EOpenMobileSensorsDiagnosticsOutputCode::InvalidSnapshot,
			TEXT("Capture diagnostics before exporting them.")
		);
	}

	bool bTruncated = false;
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), 1);
	Root->SetStringField(TEXT("capturedAtUtc"), Snapshot.CapturedAtUtc);
	Root->SetStringField(
		TEXT("backend"),
		SafeString(Snapshot.BackendName.ToString(), bTruncated)
	);
	Root->SetStringField(
		TEXT("backendState"),
		EnumName(Snapshot.BackendAvailability.State)
	);
	Root->SetStringField(
		TEXT("backendGeneration"),
		FString::Printf(TEXT("%lld"), Snapshot.BackendGeneration)
	);
	Root->SetNumberField(
		TEXT("activeRecordings"),
		Snapshot.ActiveRecordingCount
	);
	Root->SetNumberField(TEXT("activeReplays"), Snapshot.ActiveReplayCount);

	TArray<TSharedPtr<FJsonValue>> Capabilities;
	for (int32 Index = 0;
		Index < Snapshot.Capabilities.Num() && Index < MaximumCapabilities;
		++Index)
	{
		const FOpenMobileSensorCapability& Capability =
			Snapshot.Capabilities[Index];
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("sensor"), SensorName(Capability.Sensor));
		Item->SetStringField(
			TEXT("state"),
			EnumName(Capability.Availability.State)
		);
		Item->SetStringField(TEXT("source"), EnumName(Capability.Source));
		Item->SetStringField(
			TEXT("restriction"),
			EnumName(Capability.ActiveRestriction)
		);
		Item->SetNumberField(TEXT("minimumHz"), Capability.MinimumFrequencyHz);
		Item->SetNumberField(TEXT("maximumHz"), Capability.MaximumFrequencyHz);
		Item->SetBoolField(
			TEXT("nativeBatching"),
			Capability.bSupportsNativeBatching
		);
		Capabilities.Add(MakeShared<FJsonValueObject>(Item));
	}
	bTruncated |= Snapshot.Capabilities.Num() > MaximumCapabilities;
	Root->SetArrayField(TEXT("capabilities"), Capabilities);

	TArray<TSharedPtr<FJsonValue>> Metadata;
	for (int32 Index = 0;
		Index < Snapshot.Metadata.Num() && Index < MaximumMetadata;
		++Index)
	{
		const FOpenMobileSensorMetadata& Source = Snapshot.Metadata[Index];
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("sensor"), SensorName(Source.Sensor));
		Item->SetBoolField(TEXT("preferred"), Source.bPreferred);
		SetOptionalNumber(Item, TEXT("maximumRange"), Source.MaximumRange);
		SetOptionalNumber(Item, TEXT("resolution"), Source.Resolution);
		SetOptionalNumber(
			Item,
			TEXT("estimatedPowerMilliwatts"),
			Source.EstimatedPowerMilliwatts
		);
		SetOptionalNumber(
			Item,
			TEXT("minimumIntervalSeconds"),
			Source.MinimumIntervalSeconds
		);
		Item->SetStringField(
			TEXT("reportingMode"),
			Source.bReportingModeAvailable
				? EnumName(Source.ReportingMode)
				: TEXT("Unavailable")
		);
		Metadata.Add(MakeShared<FJsonValueObject>(Item));
	}
	bTruncated |= Snapshot.Metadata.Num() > MaximumMetadata;
	Root->SetArrayField(TEXT("metadata"), Metadata);

	TArray<TSharedPtr<FJsonValue>> Permissions;
	for (const FOpenMobileSensorPermissionDescriptor& Permission
		: Snapshot.Permissions)
	{
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(
			TEXT("name"),
			SafeString(Permission.PermissionName.ToString(), bTruncated)
		);
		Item->SetStringField(TEXT("status"), EnumName(Permission.Status));
		Permissions.Add(MakeShared<FJsonValueObject>(Item));
	}
	Root->SetArrayField(TEXT("permissions"), Permissions);

	TArray<TSharedPtr<FJsonValue>> Streams;
	for (int32 Index = 0;
		Index < Snapshot.Streams.Num() && Index < MaximumStreams;
		++Index)
	{
		const FOpenMobileSensorStreamDiagnostics& Source =
			Snapshot.Streams[Index];
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(
			TEXT("sensor"),
			SensorName(Source.Subscription.Sensor)
		);
		Item->SetStringField(
			TEXT("state"),
			EnumName(Source.Subscription.State)
		);
		Item->SetNumberField(
			TEXT("requestedHz"),
			Source.Rate.RequestedFrequencyHz
		);
		Item->SetNumberField(
			TEXT("appliedHz"),
			Source.Rate.AppliedFrequencyHz
		);
		Item->SetNumberField(TEXT("measuredHz"), Source.Rate.MeanFrequencyHz);
		Item->SetStringField(TEXT("batching"), EnumName(Source.BatchingMode));
		Item->SetNumberField(TEXT("queueDepth"), Source.QueueDepth);
		Item->SetStringField(
			TEXT("droppedSamples"),
			FString::Printf(TEXT("%lld"), Source.DroppedSamples)
		);
		Item->SetNumberField(
			TEXT("latestSampleAgeSeconds"),
			Source.LatestSampleAgeSeconds
		);
		Item->SetStringField(
			TEXT("lifecycle"),
			EnumName(Source.Subscription.AppliedOptions.LifecyclePolicy)
		);
		Item->SetBoolField(TEXT("hasSample"), Source.bHasSample);
		Item->SetNumberField(TEXT("sourceFlags"), Source.SourceFlags);
		Item->SetStringField(
			TEXT("accuracy"),
			Source.bHasAccuracy
				? EnumName(Source.Accuracy.Accuracy)
				: TEXT("Unavailable")
		);
		const FOpenMobileSensorFilterOptions& Filters =
			Source.Subscription.AppliedOptions.Filters;
		Item->SetBoolField(TEXT("lowPass"), Filters.bEnableLowPass);
		Item->SetBoolField(TEXT("highPass"), Filters.bEnableHighPass);
		Item->SetBoolField(
			TEXT("smoothing"),
			Filters.bEnableExponentialSmoothing
		);
		Streams.Add(MakeShared<FJsonValueObject>(Item));
	}
	bTruncated |= Snapshot.Streams.Num() > MaximumStreams;
	Root->SetArrayField(TEXT("streams"), Streams);

	TArray<TSharedPtr<FJsonValue>> PhysicalStreams;
	for (int32 Index = 0;
		Index < Snapshot.PhysicalStreams.Num()
			&& Index < MaximumPhysicalStreams;
		++Index)
	{
		const FOpenMobileSensorPhysicalStreamDiagnostics& Source =
			Snapshot.PhysicalStreams[Index];
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("sensor"), SensorName(Source.Sensor));
		Item->SetStringField(
			TEXT("backend"),
			SafeString(Source.BackendName.ToString(), bTruncated)
		);
		Item->SetNumberField(TEXT("subscriberCount"), Source.SubscriberCount);
		Item->SetNumberField(TEXT("appliedHz"), Source.AppliedFrequencyHz);
		Item->SetNumberField(
			TEXT("maximumDeliveryLatencySeconds"),
			Source.MaximumDeliveryLatencySeconds
		);
		Item->SetBoolField(TEXT("lowLatency"), Source.bLowLatency);
		Item->SetBoolField(
			TEXT("nativeBatching"),
			Source.bNativeBatchingApplied
		);
		PhysicalStreams.Add(MakeShared<FJsonValueObject>(Item));
	}
	bTruncated |= Snapshot.PhysicalStreams.Num() > MaximumPhysicalStreams;
	Root->SetArrayField(TEXT("physicalStreams"), PhysicalStreams);

	TArray<TSharedPtr<FJsonValue>> Errors;
	for (int32 Index = FMath::Max(0, Snapshot.RecentErrors.Num() - MaximumErrors);
		Index < Snapshot.RecentErrors.Num();
		++Index)
	{
		const FOpenMobileError& Source = Snapshot.RecentErrors[Index];
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("code"), EnumName(Source.Code));
		Item->SetStringField(
			TEXT("provider"),
			SafeString(Source.Provider, bTruncated)
		);
		Item->SetStringField(
			TEXT("message"),
			SafeString(Source.Message, bTruncated)
		);
		Errors.Add(MakeShared<FJsonValueObject>(Item));
	}
	bTruncated |= Snapshot.RecentErrors.Num() > MaximumErrors;
	Root->SetArrayField(TEXT("recentErrors"), Errors);
	Root->SetBoolField(TEXT("truncated"), bTruncated);

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(
		&OutJson
	);
	if (!FJsonSerializer::Serialize(Root, Writer)
		|| FTCHARToUTF8(*OutJson).Length() > MaximumOutputBytes)
	{
		OutJson.Reset();
		return MakeFailure(
			EOpenMobileSensorsDiagnosticsOutputCode::LimitExceeded,
			TEXT("The redacted diagnostics output exceeds its size limit.")
		);
	}
	return {};
}

FOpenMobileSensorsDiagnosticsOutputResult
FOpenMobileSensorsDiagnosticsOutput::CopyToClipboard(
	const FOpenMobileSensorDiagnosticsSnapshot& Snapshot
)
{
	using namespace OpenMobileSensorsDiagnosticsOutputPrivate;
	FString Json;
	FOpenMobileSensorsDiagnosticsOutputResult Result = Serialize(Snapshot, Json);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	return WriteClipboard(Json)
		? FOpenMobileSensorsDiagnosticsOutputResult()
		: MakeFailure(
			EOpenMobileSensorsDiagnosticsOutputCode::WriteFailed,
			TEXT("Could not copy sensor diagnostics.")
		);
}

FOpenMobileSensorsDiagnosticsOutputResult
FOpenMobileSensorsDiagnosticsOutput::ExportToFile(
	const FOpenMobileSensorDiagnosticsSnapshot& Snapshot,
	const FString& FilePath
)
{
	using namespace OpenMobileSensorsDiagnosticsOutputPrivate;
	if (FilePath.IsEmpty())
	{
		return MakeFailure(
			EOpenMobileSensorsDiagnosticsOutputCode::WriteFailed,
			TEXT("Choose a diagnostics export path.")
		);
	}
	FString Json;
	FOpenMobileSensorsDiagnosticsOutputResult Result = Serialize(Snapshot, Json);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	return WriteFile(FilePath, Json)
		? FOpenMobileSensorsDiagnosticsOutputResult()
		: MakeFailure(
			EOpenMobileSensorsDiagnosticsOutputCode::WriteFailed,
			TEXT("Could not write sensor diagnostics.")
		);
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileSensorsDiagnosticsOutput::SetClipboardWriterForTests(
	TFunction<bool(const FString&)>&& Writer
)
{
	OpenMobileSensorsDiagnosticsOutputPrivate::ClipboardWriterForTests =
		MoveTemp(Writer);
}

void FOpenMobileSensorsDiagnosticsOutput::SetFileWriterForTests(
	TFunction<bool(const FString&, const FString&)>&& Writer
)
{
	OpenMobileSensorsDiagnosticsOutputPrivate::FileWriterForTests =
		MoveTemp(Writer);
}

void FOpenMobileSensorsDiagnosticsOutput::ResetWritersForTests()
{
	OpenMobileSensorsDiagnosticsOutputPrivate::ClipboardWriterForTests = {};
	OpenMobileSensorsDiagnosticsOutputPrivate::FileWriterForTests = {};
}
#endif
