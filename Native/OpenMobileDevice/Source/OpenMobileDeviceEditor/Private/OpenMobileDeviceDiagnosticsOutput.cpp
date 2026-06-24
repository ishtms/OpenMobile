#include "OpenMobileDeviceDiagnosticsOutput.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "OpenMobileDeviceDiagnosticsSource.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace OpenMobileDeviceDiagnosticsOutputPrivate
{
	constexpr int32 MaximumExportBytes = 64 * 1024;
	constexpr int32 MaximumStringLength = 256;
	constexpr int32 MaximumSerializedCapabilities = 128;
	constexpr int32 MaximumSerializedMonitoringGroups = 16;
	constexpr int32 MaximumSerializedControlLeases = 16;
	constexpr int32 MaximumSerializedErrors = 16;
	constexpr int32 MaximumSerializedIssues = 32;
	constexpr double StaleAfterSeconds = 15.0 * 60.0;

#if WITH_DEV_AUTOMATION_TESTS
	TFunction<bool(const FString&)> ClipboardWriterForTests;
	TFunction<bool(const FString&, const FString&)> FileWriterForTests;
#endif

	FString SafeString(const FString& Value, bool& bTruncated)
	{
		FString Result = Value;
		Result.ReplaceInline(TEXT("\r"), TEXT(" "));
		Result.ReplaceInline(TEXT("\n"), TEXT(" "));
		Result.ReplaceInline(TEXT("\t"), TEXT(" "));
		if (Result.Contains(TEXT("://"))
			|| Result.Contains(TEXT("?"))
			|| Result.Contains(TEXT("#")))
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

	template <typename EnumType>
	FString EnumName(EnumType Value)
	{
		const UEnum* Enum = StaticEnum<EnumType>();
		return Enum
			? Enum->GetNameStringByValue(static_cast<int64>(Value))
			: TEXT("Unknown");
	}

	void SetOptionalString(
		const TSharedRef<FJsonObject>& Object,
		const TCHAR* Field,
		const FOpenMobileDeviceOptionalString& Value,
		bool& bTruncated
	)
	{
		if (Value.bIsAvailable)
		{
			Object->SetStringField(Field, SafeString(Value.Value, bTruncated));
		}
		else
		{
			Object->SetField(Field, MakeShared<FJsonValueNull>());
		}
	}

	void SetOptionalNumber(
		const TSharedRef<FJsonObject>& Object,
		const TCHAR* Field,
		const FOpenMobileDeviceOptionalFloat& Value
	)
	{
		if (Value.bIsAvailable && FMath::IsFinite(Value.Value))
		{
			Object->SetNumberField(Field, Value.Value);
		}
		else
		{
			Object->SetField(Field, MakeShared<FJsonValueNull>());
		}
	}

	void SetOptionalBytes(
		const TSharedRef<FJsonObject>& Object,
		const TCHAR* Field,
		const FOpenMobileDeviceOptionalInt64& Value
	)
	{
		if (Value.bIsAvailable && Value.Value >= 0)
		{
			Object->SetStringField(Field, FString::Printf(TEXT("%lld"), Value.Value));
		}
		else
		{
			Object->SetField(Field, MakeShared<FJsonValueNull>());
		}
	}

	void SetOptionalBool(
		const TSharedRef<FJsonObject>& Object,
		const TCHAR* Field,
		const FOpenMobileDeviceOptionalBool& Value
	)
	{
		if (Value.bIsAvailable)
		{
			Object->SetBoolField(Field, Value.Value);
		}
		else
		{
			Object->SetField(Field, MakeShared<FJsonValueNull>());
		}
	}

	bool IsAllowedControlLease(FName Name)
	{
		return Name == TEXT("Brightness")
			|| Name == TEXT("KeepScreenAwake")
			|| Name == TEXT("SystemUi")
			|| Name == TEXT("PreferredRefreshRate")
			|| Name == TEXT("Orientation");
	}

	FOpenMobileDeviceDiagnosticsOutputResult MakeFailure(
		EOpenMobileDeviceDiagnosticsOutputCode Code,
		const TCHAR* Message
	)
	{
		FOpenMobileDeviceDiagnosticsOutputResult Result;
		Result.Code = Code;
		Result.Message = Message;
		return Result;
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

FOpenMobileDeviceDiagnosticsSnapshot FOpenMobileDeviceDiagnosticsOutput::Capture()
{
	check(IsInGameThread());
	FOpenMobileDeviceDiagnosticsSnapshot Snapshot =
		FOpenMobileDeviceDiagnosticsSource::Capture();
	if (const TSharedPtr<IPlugin> Plugin =
		IPluginManager::Get().FindPlugin(TEXT("OpenMobileDevice")))
	{
		Snapshot.OpenMobileVersion = Plugin->GetDescriptor().VersionName;
	}
	return Snapshot;
}

FOpenMobileDeviceDiagnosticsOutputResult
FOpenMobileDeviceDiagnosticsOutput::Serialize(
	const FOpenMobileDeviceDiagnosticsSnapshot& Snapshot,
	FString& OutJson
)
{
	using namespace OpenMobileDeviceDiagnosticsOutputPrivate;
	OutJson.Reset();
	if (Snapshot.CapturedAtUtc == FDateTime())
	{
		return MakeFailure(
			EOpenMobileDeviceDiagnosticsOutputCode::InvalidSnapshot,
			TEXT("Capture diagnostics before exporting them.")
		);
	}

	bool bTruncated = Snapshot.bTruncated;
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), 1);
	Root->SetStringField(TEXT("capturedAtUtc"), Snapshot.CapturedAtUtc.ToIso8601());
	Root->SetBoolField(TEXT("stale"), IsStale(Snapshot));
	Root->SetStringField(
		TEXT("openMobileVersion"),
		SafeString(Snapshot.OpenMobileVersion, bTruncated)
	);
	Root->SetStringField(
		TEXT("backend"),
		SafeString(Snapshot.BackendName.ToString(), bTruncated)
	);

	const TSharedRef<FJsonObject> Application = MakeShared<FJsonObject>();
	SetOptionalString(
		Application,
		TEXT("version"),
		Snapshot.ApplicationMetadata.VersionName,
		bTruncated
	);
	SetOptionalString(
		Application,
		TEXT("buildVersion"),
		Snapshot.ApplicationMetadata.BuildNumber,
		bTruncated
	);
	Application->SetStringField(
		TEXT("buildConfiguration"),
		EnumName(Snapshot.ApplicationMetadata.BuildConfiguration)
	);
	Root->SetObjectField(TEXT("application"), Application);

	const TSharedRef<FJsonObject> Platform = MakeShared<FJsonObject>();
	Platform->SetStringField(
		TEXT("name"),
		EnumName(Snapshot.DeviceInformation.Platform)
	);
	SetOptionalString(
		Platform,
		TEXT("version"),
		Snapshot.DeviceInformation.ReadableOsVersion,
		bTruncated
	);
	Platform->SetStringField(
		TEXT("formFactor"),
		EnumName(Snapshot.DeviceInformation.FormFactor)
	);
	Root->SetObjectField(TEXT("platform"), Platform);

	TArray<TSharedPtr<FJsonValue>> Capabilities;
	for (int32 Index = 0;
		Index < Snapshot.Capabilities.Num()
			&& Index < MaximumSerializedCapabilities;
		++Index)
	{
		const FOpenMobileDeviceCapability& Capability =
			Snapshot.Capabilities[Index];
		if (!FOpenMobileDeviceCapabilityNames::IsKnown(Capability.Name))
		{
			bTruncated = true;
			continue;
		}
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Capability.Name.ToString());
		Entry->SetStringField(TEXT("state"), EnumName(Capability.State));
		Entry->SetStringField(TEXT("limit"), EnumName(Capability.Limit));
		SetOptionalString(
			Entry,
			TEXT("minimumOsVersion"),
			Capability.MinimumOsVersion,
			bTruncated
		);
		Capabilities.Add(MakeShared<FJsonValueObject>(Entry));
	}
	bTruncated |= Snapshot.Capabilities.Num() > MaximumSerializedCapabilities;
	Root->SetArrayField(TEXT("capabilities"), Capabilities);

	const TSharedRef<FJsonObject> State = MakeShared<FJsonObject>();
	const TSharedRef<FJsonObject> Power = MakeShared<FJsonObject>();
	SetOptionalNumber(Power, TEXT("batteryPercent"), Snapshot.Power.BatteryPercent);
	Power->SetStringField(TEXT("chargingState"), EnumName(Snapshot.Power.ChargingState));
	Power->SetStringField(TEXT("chargingSource"), EnumName(Snapshot.Power.ChargingSource));
	Power->SetStringField(TEXT("thermalState"), EnumName(Snapshot.Power.ThermalState));
	State->SetObjectField(TEXT("power"), Power);
	const TSharedRef<FJsonObject> Memory = MakeShared<FJsonObject>();
	SetOptionalBytes(
		Memory,
		TEXT("availablePhysicalBytes"),
		Snapshot.Memory.AvailablePhysicalBytes
	);
	Memory->SetStringField(TEXT("pressureState"), EnumName(Snapshot.Memory.PressureState));
	State->SetObjectField(TEXT("memory"), Memory);
	const TSharedRef<FJsonObject> Storage = MakeShared<FJsonObject>();
	SetOptionalBytes(Storage, TEXT("availableBytes"), Snapshot.Storage.AvailableBytes);
	SetOptionalBool(Storage, TEXT("lowStorage"), Snapshot.Storage.bIsLowStorage);
	State->SetObjectField(TEXT("storage"), Storage);
	const TSharedRef<FJsonObject> Network = MakeShared<FJsonObject>();
	Network->SetStringField(TEXT("pathState"), EnumName(Snapshot.Network.PathState));
	Network->SetStringField(
		TEXT("defaultTransport"),
		EnumName(Snapshot.Network.DefaultTransport)
	);
	SetOptionalBool(Network, TEXT("metered"), Snapshot.Network.bIsMetered);
	SetOptionalBool(Network, TEXT("expensive"), Snapshot.Network.bIsExpensive);
	SetOptionalBool(Network, TEXT("constrained"), Snapshot.Network.bIsConstrained);
	SetOptionalBool(Network, TEXT("captivePortal"), Snapshot.Network.bIsCaptivePortal);
	State->SetObjectField(TEXT("network"), Network);
	const TSharedRef<FJsonObject> Display = MakeShared<FJsonObject>();
	Display->SetNumberField(TEXT("cutoutCount"), Snapshot.Window.DisplayCutouts.Num());
	Display->SetStringField(TEXT("foldPosture"), EnumName(Snapshot.Window.FoldablePosture));
	Display->SetStringField(TEXT("orientation"), EnumName(Snapshot.Window.Orientation));
	Display->SetStringField(TEXT("windowMode"), EnumName(Snapshot.Window.WindowMode));
	State->SetObjectField(TEXT("display"), Display);
	const TSharedRef<FJsonObject> Appearance = MakeShared<FJsonObject>();
	Appearance->SetStringField(TEXT("system"), EnumName(Snapshot.Appearance.Appearance));
	State->SetObjectField(TEXT("appearance"), Appearance);
	const TSharedRef<FJsonObject> Accessibility = MakeShared<FJsonObject>();
	SetOptionalNumber(
		Accessibility,
		TEXT("preferredTextScale"),
		Snapshot.Accessibility.PreferredTextScale
	);
	SetOptionalBool(
		Accessibility,
		TEXT("reducedAnimation"),
		Snapshot.Accessibility.bReducedAnimationPreferred
	);
	SetOptionalBool(
		Accessibility,
		TEXT("screenReader"),
		Snapshot.Accessibility.bScreenReaderActive
	);
	SetOptionalBool(
		Accessibility,
		TEXT("touchExploration"),
		Snapshot.Accessibility.bTouchExplorationActive
	);
	State->SetObjectField(TEXT("accessibility"), Accessibility);
	Root->SetObjectField(TEXT("state"), State);

	TArray<TSharedPtr<FJsonValue>> MonitoringGroups;
	for (int32 Index = 0;
		Index < Snapshot.ActiveMonitoringGroups.Num()
			&& Index < MaximumSerializedMonitoringGroups;
		++Index)
	{
		MonitoringGroups.Add(MakeShared<FJsonValueString>(
			EnumName(Snapshot.ActiveMonitoringGroups[Index])
		));
	}
	bTruncated |= Snapshot.ActiveMonitoringGroups.Num()
		> MaximumSerializedMonitoringGroups;
	Root->SetArrayField(TEXT("monitoringGroups"), MonitoringGroups);
	const TSharedRef<FJsonObject> ControlLeases = MakeShared<FJsonObject>();
	for (int32 Index = 0;
		Index < Snapshot.ControlLeases.Num()
			&& Index < MaximumSerializedControlLeases;
		++Index)
	{
		const FOpenMobileDeviceDiagnosticControlLease& Lease =
			Snapshot.ControlLeases[Index];
		if (IsAllowedControlLease(Lease.Name) && Lease.Count > 0)
		{
			ControlLeases->SetNumberField(Lease.Name.ToString(), Lease.Count);
		}
	}
	bTruncated |= Snapshot.ControlLeases.Num() > MaximumSerializedControlLeases;
	Root->SetObjectField(TEXT("controlLeases"), ControlLeases);

	TArray<TSharedPtr<FJsonValue>> RecentErrors;
	for (int32 Index = 0;
		Index < Snapshot.RecentErrors.Num() && Index < MaximumSerializedErrors;
		++Index)
	{
		const FOpenMobileDeviceDiagnosticError& Error = Snapshot.RecentErrors[Index];
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("occurredAtUtc"), Error.OccurredAtUtc.ToIso8601());
		Entry->SetStringField(
			TEXT("operation"),
			SafeString(Error.Operation.ToString(), bTruncated)
		);
		Entry->SetStringField(TEXT("code"), EnumName(Error.Code));
		RecentErrors.Add(MakeShared<FJsonValueObject>(Entry));
	}
	bTruncated |= Snapshot.RecentErrors.Num() > MaximumSerializedErrors;
	Root->SetArrayField(TEXT("recentErrors"), RecentErrors);

	TArray<TSharedPtr<FJsonValue>> ConfigurationIssues;
	for (int32 Index = 0;
		Index < Snapshot.ConfigurationIssues.Num()
			&& Index < MaximumSerializedIssues;
		++Index)
	{
		const FOpenMobileDeviceDiagnosticConfigurationIssue& Issue =
			Snapshot.ConfigurationIssues[Index];
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(
			TEXT("code"),
			SafeString(Issue.Code.ToString(), bTruncated)
		);
		Entry->SetStringField(
			TEXT("severity"),
			Issue.Severity == EOpenMobileDeviceDiagnosticIssueSeverity::Error
				? TEXT("Error")
				: TEXT("Warning")
		);
		ConfigurationIssues.Add(MakeShared<FJsonValueObject>(Entry));
	}
	bTruncated |= Snapshot.ConfigurationIssues.Num() > MaximumSerializedIssues;
	Root->SetArrayField(TEXT("configurationIssues"), ConfigurationIssues);
	Root->SetBoolField(TEXT("truncated"), bTruncated);

	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		OutJson.Reset();
		return MakeFailure(
			EOpenMobileDeviceDiagnosticsOutputCode::SerializationFailed,
			TEXT("The diagnostics snapshot could not be serialized.")
		);
	}
	if (FTCHARToUTF8(*OutJson).Length() > MaximumExportBytes)
	{
		OutJson.Reset();
		return MakeFailure(
			EOpenMobileDeviceDiagnosticsOutputCode::TooLarge,
			TEXT("The diagnostics report exceeded its size limit.")
		);
	}
	return {};
}

FOpenMobileDeviceDiagnosticsOutputResult
FOpenMobileDeviceDiagnosticsOutput::CopyToClipboard(
	const FOpenMobileDeviceDiagnosticsSnapshot& Snapshot
)
{
	using namespace OpenMobileDeviceDiagnosticsOutputPrivate;
	FString Json;
	FOpenMobileDeviceDiagnosticsOutputResult Result = Serialize(Snapshot, Json);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	return WriteClipboard(Json)
		? FOpenMobileDeviceDiagnosticsOutputResult()
		: MakeFailure(
			EOpenMobileDeviceDiagnosticsOutputCode::ClipboardWriteFailed,
			TEXT("The diagnostics report could not be copied.")
		);
}

FOpenMobileDeviceDiagnosticsOutputResult
FOpenMobileDeviceDiagnosticsOutput::ExportToFile(
	const FOpenMobileDeviceDiagnosticsSnapshot& Snapshot,
	const FString& FilePath
)
{
	using namespace OpenMobileDeviceDiagnosticsOutputPrivate;
	if (FilePath.IsEmpty())
	{
		return MakeFailure(
			EOpenMobileDeviceDiagnosticsOutputCode::FileWriteFailed,
			TEXT("Choose a diagnostics export path.")
		);
	}
	FString Json;
	FOpenMobileDeviceDiagnosticsOutputResult Result = Serialize(Snapshot, Json);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	return WriteFile(FilePath, Json)
		? FOpenMobileDeviceDiagnosticsOutputResult()
		: MakeFailure(
			EOpenMobileDeviceDiagnosticsOutputCode::FileWriteFailed,
			TEXT("The diagnostics report could not be written.")
		);
}

bool FOpenMobileDeviceDiagnosticsOutput::IsStale(
	const FOpenMobileDeviceDiagnosticsSnapshot& Snapshot
)
{
	return Snapshot.CapturedAtUtc == FDateTime()
		|| (FDateTime::UtcNow() - Snapshot.CapturedAtUtc).GetTotalSeconds()
			> OpenMobileDeviceDiagnosticsOutputPrivate::StaleAfterSeconds;
}

int32 FOpenMobileDeviceDiagnosticsOutput::GetMaximumExportBytes()
{
	return OpenMobileDeviceDiagnosticsOutputPrivate::MaximumExportBytes;
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileDeviceDiagnosticsOutput::SetClipboardWriterForTests(
	TFunction<bool(const FString&)> Writer
)
{
	OpenMobileDeviceDiagnosticsOutputPrivate::ClipboardWriterForTests =
		MoveTemp(Writer);
}

void FOpenMobileDeviceDiagnosticsOutput::SetFileWriterForTests(
	TFunction<bool(const FString&, const FString&)> Writer
)
{
	OpenMobileDeviceDiagnosticsOutputPrivate::FileWriterForTests =
		MoveTemp(Writer);
}

void FOpenMobileDeviceDiagnosticsOutput::ResetWritersForTests()
{
	OpenMobileDeviceDiagnosticsOutputPrivate::ClipboardWriterForTests = nullptr;
	OpenMobileDeviceDiagnosticsOutputPrivate::FileWriterForTests = nullptr;
}
#endif
