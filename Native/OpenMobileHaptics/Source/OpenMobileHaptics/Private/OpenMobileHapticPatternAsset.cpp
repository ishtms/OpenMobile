#include "OpenMobileHapticPatternAsset.h"

#include "Misc/Crc.h"
#include "OpenMobileHapticsPatternCompiler.h"
#include "OpenMobileHapticsSettings.h"
#include "UObject/ObjectSaveContext.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#include "UObject/UnrealType.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogOpenMobileHapticPatternAsset, Log, All);

namespace OpenMobileHapticPatternAssetPrivate
{
	constexpr int32 MaximumSerializedEventCount = 4096;

	uint32 ToMicroseconds(double Seconds)
	{
		return static_cast<uint32>(FMath::RoundToDouble(Seconds * 1000000.0));
	}

	uint16 ToNormalizedUInt16(float Value)
	{
		return static_cast<uint16>(FMath::RoundToInt(
			FMath::Clamp(Value, 0.0f, 1.0f) * MAX_uint16
		));
	}

	FString DescribeCompileError(
		EOpenMobileHapticsPatternCompileError Error,
		int32 EventIndex,
		const FOpenMobileHapticPattern& SourcePattern
	)
	{
		const TCHAR* Description = TEXT("unknown pattern error");
		switch (Error)
		{
		case EOpenMobileHapticsPatternCompileError::Empty:
			Description = TEXT("the pattern has no events");
			break;
		case EOpenMobileHapticsPatternCompileError::InvalidLimits:
			Description = TEXT("the configured pattern limits are invalid");
			break;
		case EOpenMobileHapticsPatternCompileError::EventLimit:
			Description = TEXT("the pattern exceeds the event limit");
			break;
		case EOpenMobileHapticsPatternCompileError::InvalidEventType:
			Description = TEXT("the event type is invalid");
			break;
		case EOpenMobileHapticsPatternCompileError::Nonfinite:
			Description = TEXT("an event value is not finite or normalized");
			break;
		case EOpenMobileHapticsPatternCompileError::InvalidRange:
			Description = TEXT("an event timing range is invalid");
			break;
		case EOpenMobileHapticsPatternCompileError::Unsorted:
			Description = TEXT("events are not sorted by start time");
			break;
		case EOpenMobileHapticsPatternCompileError::Overlap:
			Description = TEXT("an event overlaps the previous event");
			break;
		case EOpenMobileHapticsPatternCompileError::EventDurationLimit:
			Description = TEXT("an event exceeds the duration limit");
			break;
		case EOpenMobileHapticsPatternCompileError::DurationLimit:
			Description = TEXT("the pattern exceeds the duration limit");
			break;
		case EOpenMobileHapticsPatternCompileError::Granularity:
			Description = TEXT("an event is shorter than the timing granularity");
			break;
		default:
			break;
		}
		if (!SourcePattern.Events.IsValidIndex(EventIndex))
		{
			return FString(Description);
		}

		const FOpenMobileHapticPatternEvent& Event =
			SourcePattern.Events[EventIndex];
		const TCHAR* Field = nullptr;
		switch (Error)
		{
		case EOpenMobileHapticsPatternCompileError::InvalidEventType:
			Field = TEXT("Type");
			break;
		case EOpenMobileHapticsPatternCompileError::Nonfinite:
			if (!FMath::IsFinite(Event.StartTimeSeconds))
			{
				Field = TEXT("StartTimeSeconds");
			}
			else if (!FMath::IsFinite(Event.DurationSeconds))
			{
				Field = TEXT("DurationSeconds");
			}
			else if (!FMath::IsFinite(Event.Intensity)
				|| Event.Intensity < 0.0f || Event.Intensity > 1.0f)
			{
				Field = TEXT("Intensity");
			}
			else if (!FMath::IsFinite(Event.Sharpness)
				|| Event.Sharpness < 0.0f || Event.Sharpness > 1.0f)
			{
				Field = TEXT("Sharpness");
			}
			else
			{
				Field = TEXT("FrequencyIntent");
			}
			break;
		case EOpenMobileHapticsPatternCompileError::InvalidRange:
		case EOpenMobileHapticsPatternCompileError::EventDurationLimit:
		case EOpenMobileHapticsPatternCompileError::Granularity:
			Field = Event.StartTimeSeconds < 0.0
				? TEXT("StartTimeSeconds")
				: TEXT("DurationSeconds");
			break;
		case EOpenMobileHapticsPatternCompileError::Unsorted:
		case EOpenMobileHapticsPatternCompileError::Overlap:
		case EOpenMobileHapticsPatternCompileError::DurationLimit:
			Field = TEXT("StartTimeSeconds");
			break;
		default:
			break;
		}
		return Field
			? FString::Printf(
				TEXT("Event %d %s: %s"),
				EventIndex,
				Field,
				Description
			)
			: FString::Printf(TEXT("Event %d: %s"), EventIndex, Description);
	}

	template <typename ValueType>
	void HashValue(uint32& Hash, const ValueType& Value)
	{
		Hash = FCrc::TypeCrc32(Value, Hash);
	}
}

FSoftObjectPath UOpenMobileHapticPatternAsset::GetOverrideForPlatform(
	EOpenMobileHapticOverridePlatform Platform
) const
{
	if (Platform == EOpenMobileHapticOverridePlatform::Android)
	{
		return AndroidOverride.ToSoftObjectPath();
	}
	if (Platform == EOpenMobileHapticOverridePlatform::IOS)
	{
		return IOSOverride.ToSoftObjectPath();
	}
	return {};
}

FSoftObjectPath UOpenMobileHapticPatternAsset::GetOverrideForCurrentPlatform() const
{
#if PLATFORM_ANDROID
	return AndroidOverride.ToSoftObjectPath();
#elif PLATFORM_IOS
	return IOSOverride.ToSoftObjectPath();
#else
	return {};
#endif
}

bool FOpenMobileHapticCookedPatternData::Serialize(FArchive& Archive)
{
	using namespace OpenMobileHapticPatternAssetPrivate;
	uint8 SerializedVersion = DataFormatVersion;
	Archive << SerializedVersion;
	if (SerializedVersion < 1 || SerializedVersion > CurrentFormatVersion)
	{
		Archive.SetError();
		return false;
	}
	if (Archive.IsLoading())
	{
		DataFormatVersion = SerializedVersion;
		Events.Reset();
	}

	Archive << SourceHash;
	Archive << DurationMicroseconds;
	Archive << GranularityMicroseconds;
	int32 EventCount = Events.Num();
	Archive << EventCount;
	if (EventCount < 0 || EventCount > MaximumSerializedEventCount)
	{
		Archive.SetError();
		return false;
	}
	if (Archive.IsLoading())
	{
		Events.SetNum(EventCount);
	}

	for (FOpenMobileHapticCookedPatternEvent& Event : Events)
	{
		uint8 Type = static_cast<uint8>(Event.Type);
		Archive << Type;
		Archive << Event.StartTimeMicroseconds;
		Archive << Event.DurationMicroseconds;
		Archive << Event.Intensity;
		Archive << Event.Sharpness;
		if (SerializedVersion >= 2)
		{
			Archive << Event.FrequencyIntent;
		}
		else if (Archive.IsLoading())
		{
			Event.FrequencyIntent = MAX_uint16 / 2;
		}
		if (Archive.IsLoading())
		{
			if (Type > static_cast<uint8>(
				EOpenMobileHapticPatternEventType::Silence))
			{
				Archive.SetError();
				return false;
			}
			Event.Type = static_cast<EOpenMobileHapticPatternEventType>(Type);
		}
	}
	return !Archive.IsError();
}

void FOpenMobileHapticCookedPatternData::Reset()
{
	DataFormatVersion = CurrentFormatVersion;
	SourceHash = 0;
	DurationMicroseconds = 0;
	GranularityMicroseconds = 1000;
	Events.Reset();
}

uint32 UOpenMobileHapticPatternAsset::ComputeSourceHash() const
{
#if WITH_EDITORONLY_DATA
	using namespace OpenMobileHapticPatternAssetPrivate;
	uint32 Hash = 0;
	const int32 EventCount = SourcePattern.Events.Num();
	HashValue(Hash, EventCount);
	for (const FOpenMobileHapticPatternEvent& Event : SourcePattern.Events)
	{
		const uint8 Type = static_cast<uint8>(Event.Type);
		HashValue(Hash, Type);
		HashValue(Hash, Event.StartTimeSeconds);
		HashValue(Hash, Event.DurationSeconds);
		HashValue(Hash, Event.Intensity);
		HashValue(Hash, Event.Sharpness);
		HashValue(Hash, Event.FrequencyIntent);
	}
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	HashValue(Hash, Settings->MaximumPatternEventCount);
	HashValue(Hash, Settings->MaximumContinuousDurationSeconds);
	HashValue(Hash, Settings->MaximumPatternEventDurationSeconds);
	HashValue(Hash, Settings->MinimumPatternGranularitySeconds);
	return Hash;
#else
	return CookedPattern.SourceHash;
#endif
}

bool UOpenMobileHapticPatternAsset::IsDerivedDataCurrent() const
{
	return CookedPattern.DataFormatVersion
			== FOpenMobileHapticCookedPatternData::CurrentFormatVersion
		&& !CookedPattern.Events.IsEmpty()
		&& CookedPattern.SourceHash == ComputeSourceHash();
}

bool UOpenMobileHapticPatternAsset::RebuildDerivedData(
	TArray<FString>& Errors
)
{
	Errors.Reset();
#if WITH_EDITORONLY_DATA
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	const FOpenMobileHapticsPatternCompileResult Result =
		FOpenMobileHapticsPatternCompiler::Compile(
			SourcePattern,
			FOpenMobileHapticsPatternCompiler::MakeLimits(*Settings, {})
		);
	if (!Result.IsSuccess())
	{
		CookedPattern.Reset();
		Errors.Add(OpenMobileHapticPatternAssetPrivate::DescribeCompileError(
			Result.Error,
			Result.EventIndex,
			SourcePattern
		));
		return false;
	}

	using namespace OpenMobileHapticPatternAssetPrivate;
	FOpenMobileHapticCookedPatternData Rebuilt;
	Rebuilt.SourceHash = ComputeSourceHash();
	Rebuilt.DurationMicroseconds =
		ToMicroseconds(Result.Pattern->GetDurationSeconds());
	Rebuilt.GranularityMicroseconds =
		ToMicroseconds(Result.Pattern->GetGranularitySeconds());
	Rebuilt.Events.Reserve(Result.Pattern->GetEvents().Num());
	for (const FOpenMobileHapticsCompiledPatternEvent& SourceEvent
		: Result.Pattern->GetEvents())
	{
		FOpenMobileHapticCookedPatternEvent& Event =
			Rebuilt.Events.AddDefaulted_GetRef();
		Event.Type = SourceEvent.Type;
		Event.StartTimeMicroseconds = ToMicroseconds(
			SourceEvent.StartTimeSeconds
		);
		Event.DurationMicroseconds = ToMicroseconds(
			SourceEvent.DurationSeconds
		);
		Event.Intensity = ToNormalizedUInt16(SourceEvent.Intensity);
		Event.Sharpness = ToNormalizedUInt16(SourceEvent.Sharpness);
		Event.FrequencyIntent = ToNormalizedUInt16(
			SourceEvent.FrequencyIntent
		);
	}
	CookedPattern = MoveTemp(Rebuilt);
	return true;
#else
	Errors.Add(TEXT("Pattern source data is unavailable outside the editor."));
	return false;
#endif
}

void UOpenMobileHapticPatternAsset::PreSave(FObjectPreSaveContext SaveContext)
{
#if WITH_EDITORONLY_DATA
	if (!IsTemplate() && !IsDerivedDataCurrent())
	{
		TArray<FString> Errors;
		if (!RebuildDerivedData(Errors))
		{
			for (const FString& Error : Errors)
			{
				UE_LOG(
					LogOpenMobileHapticPatternAsset,
					Error,
					TEXT("%s: %s"),
					*GetPathName(),
					*Error
				);
			}
		}
	}
#endif
	Super::PreSave(SaveContext);
}

#if WITH_EDITOR
void UOpenMobileHapticPatternAsset::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent
)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	TArray<FString> Errors;
	RebuildDerivedData(Errors);
}

EDataValidationResult UOpenMobileHapticPatternAsset::IsDataValid(
	FDataValidationContext& Context
) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	const FOpenMobileHapticsPatternCompileResult CompileResult =
		FOpenMobileHapticsPatternCompiler::Compile(
			SourcePattern,
			FOpenMobileHapticsPatternCompiler::MakeLimits(*Settings, {})
		);
	if (!CompileResult.IsSuccess())
	{
		Context.AddError(FText::FromString(
			OpenMobileHapticPatternAssetPrivate::DescribeCompileError(
				CompileResult.Error,
				CompileResult.EventIndex,
				SourcePattern
			)
		));
		return EDataValidationResult::Invalid;
	}
	if (!IsDerivedDataCurrent())
	{
		Context.AddError(FText::FromString(
			TEXT("Cooked pattern data is out of date.")
		));
		return EDataValidationResult::Invalid;
	}
	return CombineDataValidationResults(Result, EDataValidationResult::Valid);
}
#endif
