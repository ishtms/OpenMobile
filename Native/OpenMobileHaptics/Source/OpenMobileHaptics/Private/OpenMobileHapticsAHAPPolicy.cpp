#include "OpenMobileHapticsAHAPPolicy.h"

#include "Dom/JsonObject.h"
#include "OpenMobileHapticsAppleAudioResourcePolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace OpenMobileHapticsAHAPPolicyPrivate
{
	enum class EEntryType : uint8
	{
		Event,
		Parameter,
		Curve
	};

	struct FParameter
	{
		FString Id;
		double Value = 0.0;
	};

	struct FEvent
	{
		FString Type;
		double Time = 0.0;
		double Duration = 0.0;
		bool bHasDuration = false;
		FString WaveformPath;
		bool bHasWaveformLoopEnabled = false;
		bool bWaveformLoopEnabled = false;
		bool bHasWaveformUseVolumeEnvelope = false;
		bool bWaveformUseVolumeEnvelope = false;
		TArray<FParameter> Parameters;
	};

	struct FDynamicParameter
	{
		FString Id;
		double Time = 0.0;
		double Value = 0.0;
	};

	struct FCurvePoint
	{
		double Time = 0.0;
		double Value = 0.0;
	};

	struct FCurve
	{
		FString Id;
		double Time = 0.0;
		TArray<FCurvePoint> Points;
	};

	struct FEntry
	{
		EEntryType Type = EEntryType::Event;
		FEvent Event;
		FDynamicParameter Parameter;
		FCurve Curve;
	};

	FOpenMobileHapticsAHAPNormalizationResult Fail(
		EOpenMobileHapticsAHAPError Error,
		FString FieldPath
	)
	{
		FOpenMobileHapticsAHAPNormalizationResult Result;
		Result.Error = Error;
		Result.FieldPath = MoveTemp(FieldPath);
		return Result;
	}

	bool HasOnlyKeys(
		const FJsonObject& Object,
		std::initializer_list<const TCHAR*> Keys,
		FString& OutUnsupported
	)
	{
		for (const auto& Pair : Object.Values)
		{
			bool bKnown = false;
			for (const TCHAR* Key : Keys)
			{
				if (Pair.Key == Key)
				{
					bKnown = true;
					break;
				}
			}
			if (!bKnown)
			{
				OutUnsupported = Pair.Key;
				return false;
			}
		}
		return true;
	}

	bool ReadNumber(
		const FJsonObject& Object,
		const TCHAR* Field,
		double& OutValue,
		EOpenMobileHapticsAHAPError& OutError
	)
	{
		if (!Object.HasField(Field))
		{
			OutError = EOpenMobileHapticsAHAPError::MissingKey;
			return false;
		}
		if (!Object.TryGetNumberField(Field, OutValue))
		{
			OutError = EOpenMobileHapticsAHAPError::InvalidStructure;
			return false;
		}
		if (!FMath::IsFinite(OutValue))
		{
			OutError = EOpenMobileHapticsAHAPError::Nonfinite;
			return false;
		}
		return true;
	}

	bool ReadString(
		const FJsonObject& Object,
		const TCHAR* Field,
		FString& OutValue,
		EOpenMobileHapticsAHAPError& OutError
	)
	{
		if (!Object.HasField(Field))
		{
			OutError = EOpenMobileHapticsAHAPError::MissingKey;
			return false;
		}
		if (!Object.TryGetStringField(Field, OutValue)
			|| OutValue.IsEmpty())
		{
			OutError = EOpenMobileHapticsAHAPError::InvalidStructure;
			return false;
		}
		return true;
	}

	bool IsInRange(double Value, double Minimum, double Maximum)
	{
		return Value >= Minimum && Value <= Maximum;
	}

	bool EventParameterRange(
		const FString& Id,
		bool bAudio,
		double& OutMinimum,
		double& OutMaximum
	)
	{
		if (Id == TEXT("AttackTime")
			|| Id == TEXT("DecayTime")
			|| Id == TEXT("ReleaseTime")
			|| Id == TEXT("Sustained"))
		{
			OutMinimum = 0.0;
			OutMaximum = 1.0;
			return true;
		}
		if (!bAudio && (Id == TEXT("HapticIntensity")
			|| Id == TEXT("HapticSharpness")))
		{
			OutMinimum = 0.0;
			OutMaximum = 1.0;
			return true;
		}
		if (bAudio && (Id == TEXT("AudioBrightness")
			|| Id == TEXT("AudioVolume")))
		{
			OutMinimum = 0.0;
			OutMaximum = 1.0;
			return true;
		}
		if (bAudio && (Id == TEXT("AudioPan")
			|| Id == TEXT("AudioPitch")))
		{
			OutMinimum = -1.0;
			OutMaximum = 1.0;
			return true;
		}
		return false;
	}

	bool DynamicParameterRange(
		const FString& Id,
		double& OutMinimum,
		double& OutMaximum
	)
	{
		if (Id == TEXT("HapticIntensityControl")
			|| Id == TEXT("AudioVolumeControl"))
		{
			OutMinimum = 0.0;
			OutMaximum = 1.0;
			return true;
		}
		if (Id == TEXT("HapticSharpnessControl")
			|| Id == TEXT("HapticAttackTimeControl")
			|| Id == TEXT("HapticDecayTimeControl")
			|| Id == TEXT("HapticReleaseTimeControl")
			|| Id == TEXT("AudioBrightnessControl")
			|| Id == TEXT("AudioPanControl")
			|| Id == TEXT("AudioPitchControl")
			|| Id == TEXT("AudioAttackTimeControl")
			|| Id == TEXT("AudioDecayTimeControl")
			|| Id == TEXT("AudioReleaseTimeControl"))
		{
			OutMinimum = -1.0;
			OutMaximum = 1.0;
			return true;
		}
		return false;
	}

	FOpenMobileHapticsAHAPNormalizationResult ParseEvent(
		const FJsonObject& Object,
		int32 EntryIndex,
		const FOpenMobileHapticsAHAPLimits& Limits,
		FEntry& OutEntry,
		FOpenMobileHapticsAHAPResource& Resource,
		int32& TotalParameters,
		bool bAllowExternalAudioResources
	)
	{
		const FString Path = FString::Printf(
			TEXT("Pattern[%d].Event"), EntryIndex);
		FString Unsupported;
		if (!HasOnlyKeys(Object, {
			TEXT("EventType"),
			TEXT("Time"),
			TEXT("Duration"),
			TEXT("EventParameters"),
			TEXT("EventWaveformPath"),
			TEXT("EventWaveformLoopEnabled"),
			TEXT("EventWaveformUseVolumeEnvelope")
		}, Unsupported))
		{
			return Fail(EOpenMobileHapticsAHAPError::UnsupportedKey,
				Path + TEXT(".") + Unsupported);
		}

		EOpenMobileHapticsAHAPError ReadError;
		if (!ReadString(Object, TEXT("EventType"),
			OutEntry.Event.Type, ReadError))
		{
			return Fail(ReadError, Path + TEXT(".EventType"));
		}
		const bool bHaptic = OutEntry.Event.Type == TEXT("HapticTransient")
			|| OutEntry.Event.Type == TEXT("HapticContinuous");
		const bool bCustomAudio =
			OutEntry.Event.Type == TEXT("AudioCustom");
		const bool bAudio = OutEntry.Event.Type == TEXT("AudioContinuous")
			|| bCustomAudio;
		if (bCustomAudio && !bAllowExternalAudioResources)
		{
			return Fail(EOpenMobileHapticsAHAPError::ExternalResourcePath,
				Path + TEXT(".EventWaveformPath"));
		}
		if (!bHaptic && !bAudio)
		{
			return Fail(EOpenMobileHapticsAHAPError::InvalidValue,
				Path + TEXT(".EventType"));
		}
		const bool bHasWaveformPath =
			Object.HasField(TEXT("EventWaveformPath"));
		const bool bHasWaveformLoop =
			Object.HasField(TEXT("EventWaveformLoopEnabled"));
		const bool bHasWaveformEnvelope =
			Object.HasField(TEXT("EventWaveformUseVolumeEnvelope"));
		if (!bCustomAudio
			&& (bHasWaveformPath
				|| bHasWaveformLoop
				|| bHasWaveformEnvelope))
		{
			return Fail(EOpenMobileHapticsAHAPError::InvalidValue,
				Path + TEXT(".EventWaveformPath"));
		}
		if (bCustomAudio && !bHasWaveformPath)
		{
			return Fail(EOpenMobileHapticsAHAPError::MissingKey,
				Path + TEXT(".EventWaveformPath"));
		}
		if (bCustomAudio)
		{
			FString SourcePath;
			if (!ReadString(Object, TEXT("EventWaveformPath"),
				SourcePath, ReadError)
				|| !FOpenMobileHapticsAppleAudioResourcePolicy::NormalizeRelativePath(
					SourcePath,
					OutEntry.Event.WaveformPath
				))
			{
				return Fail(
					EOpenMobileHapticsAHAPError::ExternalResourcePath,
					Path + TEXT(".EventWaveformPath")
				);
			}
			if (bHasWaveformLoop
				&& !Object.TryGetBoolField(
					TEXT("EventWaveformLoopEnabled"),
					OutEntry.Event.bWaveformLoopEnabled
				))
			{
				return Fail(EOpenMobileHapticsAHAPError::InvalidStructure,
					Path + TEXT(".EventWaveformLoopEnabled"));
			}
			if (bHasWaveformEnvelope
				&& !Object.TryGetBoolField(
					TEXT("EventWaveformUseVolumeEnvelope"),
					OutEntry.Event.bWaveformUseVolumeEnvelope
				))
			{
				return Fail(EOpenMobileHapticsAHAPError::InvalidStructure,
					Path + TEXT(".EventWaveformUseVolumeEnvelope"));
			}
			OutEntry.Event.bHasWaveformLoopEnabled = bHasWaveformLoop;
			OutEntry.Event.bHasWaveformUseVolumeEnvelope =
				bHasWaveformEnvelope;
			Resource.ExternalAudioResourcePaths.AddUnique(
				OutEntry.Event.WaveformPath
			);
		}
		if (!ReadNumber(Object, TEXT("Time"), OutEntry.Event.Time, ReadError))
		{
			return Fail(ReadError, Path + TEXT(".Time"));
		}
		if (!IsInRange(OutEntry.Event.Time, 0.0,
			Limits.MaximumDurationSeconds))
		{
			return Fail(EOpenMobileHapticsAHAPError::InvalidValue,
				Path + TEXT(".Time"));
		}

		const bool bContinuous = OutEntry.Event.Type.EndsWith(
			TEXT("Continuous"));
		if (Object.HasField(TEXT("Duration")))
		{
			if (!ReadNumber(Object, TEXT("Duration"),
				OutEntry.Event.Duration, ReadError))
			{
				return Fail(ReadError, Path + TEXT(".Duration"));
			}
			OutEntry.Event.bHasDuration = true;
		}
		else if (bContinuous || bCustomAudio)
		{
			return Fail(EOpenMobileHapticsAHAPError::MissingKey,
				Path + TEXT(".Duration"));
		}
		if (((bContinuous || bCustomAudio)
				&& OutEntry.Event.Duration <= 0.0)
			|| OutEntry.Event.Duration < 0.0
			|| OutEntry.Event.Time + OutEntry.Event.Duration
				> Limits.MaximumDurationSeconds)
		{
			return Fail(EOpenMobileHapticsAHAPError::InvalidValue,
				Path + TEXT(".Duration"));
		}

		const TArray<TSharedPtr<FJsonValue>>* ParameterValues = nullptr;
		if (Object.TryGetArrayField(TEXT("EventParameters"), ParameterValues))
		{
			for (int32 ParameterIndex = 0;
				ParameterIndex < ParameterValues->Num(); ++ParameterIndex)
			{
				const FString ParameterPath = FString::Printf(
					TEXT("%s.EventParameters[%d]"),
					*Path,
					ParameterIndex
				);
				const TSharedPtr<FJsonObject> ParameterObject =
					(*ParameterValues)[ParameterIndex].IsValid()
						? (*ParameterValues)[ParameterIndex]->AsObject()
						: nullptr;
				if (!ParameterObject.IsValid())
				{
					return Fail(EOpenMobileHapticsAHAPError::InvalidStructure,
						ParameterPath);
				}
				if (!HasOnlyKeys(*ParameterObject, {
					TEXT("ParameterID"), TEXT("ParameterValue")
				}, Unsupported))
				{
					return Fail(EOpenMobileHapticsAHAPError::UnsupportedKey,
						ParameterPath + TEXT(".") + Unsupported);
				}
				FParameter Parameter;
				if (!ReadString(*ParameterObject, TEXT("ParameterID"),
					Parameter.Id, ReadError))
				{
					return Fail(ReadError,
						ParameterPath + TEXT(".ParameterID"));
				}
				if (!ReadNumber(*ParameterObject, TEXT("ParameterValue"),
					Parameter.Value, ReadError))
				{
					return Fail(ReadError,
						ParameterPath + TEXT(".ParameterValue"));
				}
				double Minimum = 0.0;
				double Maximum = 0.0;
				if (!EventParameterRange(Parameter.Id, bAudio,
					Minimum, Maximum)
					|| !IsInRange(Parameter.Value, Minimum, Maximum))
				{
					return Fail(EOpenMobileHapticsAHAPError::InvalidValue,
						ParameterPath + TEXT(".ParameterValue"));
				}
				OutEntry.Event.Parameters.Add(MoveTemp(Parameter));
				if (++TotalParameters > Limits.MaximumParameters)
				{
					return Fail(EOpenMobileHapticsAHAPError::LimitExceeded,
						Path + TEXT(".EventParameters"));
				}
			}
		}
		else if (Object.HasField(TEXT("EventParameters")))
		{
			return Fail(EOpenMobileHapticsAHAPError::InvalidStructure,
				Path + TEXT(".EventParameters"));
		}

		OutEntry.Type = EEntryType::Event;
		Resource.DurationSeconds = FMath::Max(
			Resource.DurationSeconds,
			OutEntry.Event.Time + OutEntry.Event.Duration
		);
		if (bAudio)
		{
			++Resource.AudioEventCount;
			Resource.bContainsAudioEvents = true;
			Resource.bRequiresAdvancedPlayer = true;
			Resource.bContainsCustomAudioEvents |= bCustomAudio;
		}
		else
		{
			++Resource.HapticEventCount;
			Resource.bContainsHapticEvents = true;
		}
		return {};
	}

	FOpenMobileHapticsAHAPNormalizationResult ParseParameter(
		const FJsonObject& Object,
		int32 EntryIndex,
		const FOpenMobileHapticsAHAPLimits& Limits,
		FEntry& OutEntry,
		FOpenMobileHapticsAHAPResource& Resource,
		int32& TotalParameters
	)
	{
		const FString Path = FString::Printf(
			TEXT("Pattern[%d].Parameter"), EntryIndex);
		FString Unsupported;
		if (!HasOnlyKeys(Object, {
			TEXT("ParameterID"), TEXT("Time"), TEXT("ParameterValue")
		}, Unsupported))
		{
			return Fail(EOpenMobileHapticsAHAPError::UnsupportedKey,
				Path + TEXT(".") + Unsupported);
		}
		EOpenMobileHapticsAHAPError ReadError;
		if (!ReadString(Object, TEXT("ParameterID"),
			OutEntry.Parameter.Id, ReadError))
		{
			return Fail(ReadError, Path + TEXT(".ParameterID"));
		}
		if (!ReadNumber(Object, TEXT("Time"),
			OutEntry.Parameter.Time, ReadError))
		{
			return Fail(ReadError, Path + TEXT(".Time"));
		}
		if (!ReadNumber(Object, TEXT("ParameterValue"),
			OutEntry.Parameter.Value, ReadError))
		{
			return Fail(ReadError, Path + TEXT(".ParameterValue"));
		}
		double Minimum = 0.0;
		double Maximum = 0.0;
		if (!IsInRange(OutEntry.Parameter.Time, 0.0,
			Limits.MaximumDurationSeconds)
			|| !DynamicParameterRange(OutEntry.Parameter.Id,
				Minimum, Maximum)
			|| !IsInRange(OutEntry.Parameter.Value, Minimum, Maximum))
		{
			return Fail(EOpenMobileHapticsAHAPError::InvalidValue, Path);
		}
		if (++TotalParameters > Limits.MaximumParameters)
		{
			return Fail(EOpenMobileHapticsAHAPError::LimitExceeded, Path);
		}
		OutEntry.Type = EEntryType::Parameter;
		++Resource.ParameterCount;
		Resource.DurationSeconds = FMath::Max(
			Resource.DurationSeconds, OutEntry.Parameter.Time);
		Resource.bRequiresAdvancedPlayer = true;
		return {};
	}

	FOpenMobileHapticsAHAPNormalizationResult ParseCurve(
		const FJsonObject& Object,
		int32 EntryIndex,
		const FOpenMobileHapticsAHAPLimits& Limits,
		FEntry& OutEntry,
		FOpenMobileHapticsAHAPResource& Resource,
		int32& TotalCurvePoints
	)
	{
		const FString Path = FString::Printf(
			TEXT("Pattern[%d].ParameterCurve"), EntryIndex);
		FString Unsupported;
		if (!HasOnlyKeys(Object, {
			TEXT("ParameterID"),
			TEXT("Time"),
			TEXT("ParameterCurveControlPoints")
		}, Unsupported))
		{
			return Fail(EOpenMobileHapticsAHAPError::UnsupportedKey,
				Path + TEXT(".") + Unsupported);
		}
		EOpenMobileHapticsAHAPError ReadError;
		if (!ReadString(Object, TEXT("ParameterID"),
			OutEntry.Curve.Id, ReadError))
		{
			return Fail(ReadError, Path + TEXT(".ParameterID"));
		}
		if (!ReadNumber(Object, TEXT("Time"),
			OutEntry.Curve.Time, ReadError))
		{
			return Fail(ReadError, Path + TEXT(".Time"));
		}
		double Minimum = 0.0;
		double Maximum = 0.0;
		if (!IsInRange(OutEntry.Curve.Time, 0.0,
			Limits.MaximumDurationSeconds)
			|| !DynamicParameterRange(OutEntry.Curve.Id, Minimum, Maximum))
		{
			return Fail(EOpenMobileHapticsAHAPError::InvalidValue, Path);
		}
		const TArray<TSharedPtr<FJsonValue>>* PointValues = nullptr;
		if (!Object.TryGetArrayField(
			TEXT("ParameterCurveControlPoints"), PointValues)
			|| !PointValues || PointValues->IsEmpty())
		{
			return Fail(EOpenMobileHapticsAHAPError::MissingKey,
				Path + TEXT(".ParameterCurveControlPoints"));
		}
		double PreviousTime = -1.0;
		for (int32 PointIndex = 0; PointIndex < PointValues->Num(); ++PointIndex)
		{
			const FString PointPath = FString::Printf(
				TEXT("%s.ParameterCurveControlPoints[%d]"),
				*Path,
				PointIndex
			);
			const TSharedPtr<FJsonObject> PointObject =
				(*PointValues)[PointIndex].IsValid()
					? (*PointValues)[PointIndex]->AsObject()
					: nullptr;
			if (!PointObject.IsValid())
			{
				return Fail(EOpenMobileHapticsAHAPError::InvalidStructure,
					PointPath);
			}
			if (!HasOnlyKeys(*PointObject, {
				TEXT("Time"), TEXT("ParameterValue")
			}, Unsupported))
			{
				return Fail(EOpenMobileHapticsAHAPError::UnsupportedKey,
					PointPath + TEXT(".") + Unsupported);
			}
			FCurvePoint Point;
			if (!ReadNumber(*PointObject, TEXT("Time"), Point.Time, ReadError))
			{
				return Fail(ReadError, PointPath + TEXT(".Time"));
			}
			if (!ReadNumber(*PointObject, TEXT("ParameterValue"),
				Point.Value, ReadError))
			{
				return Fail(ReadError,
					PointPath + TEXT(".ParameterValue"));
			}
			if (Point.Time < 0.0 || Point.Time <= PreviousTime
				|| !IsInRange(Point.Value, Minimum, Maximum)
				|| OutEntry.Curve.Time + Point.Time
					> Limits.MaximumDurationSeconds)
			{
				return Fail(EOpenMobileHapticsAHAPError::InvalidValue, PointPath);
			}
			PreviousTime = Point.Time;
			OutEntry.Curve.Points.Add(Point);
			if (++TotalCurvePoints > Limits.MaximumCurvePoints)
			{
				return Fail(EOpenMobileHapticsAHAPError::LimitExceeded,
					Path + TEXT(".ParameterCurveControlPoints"));
			}
		}
		OutEntry.Type = EEntryType::Curve;
		++Resource.ParameterCurveCount;
		if (Resource.ParameterCurveCount > Limits.MaximumParameterCurves)
		{
			return Fail(EOpenMobileHapticsAHAPError::LimitExceeded, Path);
		}
		Resource.DurationSeconds = FMath::Max(
			Resource.DurationSeconds,
			OutEntry.Curve.Time + OutEntry.Curve.Points.Last().Time
		);
		Resource.bRequiresAdvancedPlayer = true;
		return {};
	}

	FString WriteNormalized(const TArray<FEntry>& Entries)
	{
		FString Output;
		const TSharedRef<
			TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>
		> Writer = TJsonWriterFactory<
			TCHAR,
			TCondensedJsonPrintPolicy<TCHAR>
		>::Create(&Output);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("Version"), 1);
		Writer->WriteArrayStart(TEXT("Pattern"));
		for (const FEntry& Entry : Entries)
		{
			Writer->WriteObjectStart();
			if (Entry.Type == EEntryType::Event)
			{
				Writer->WriteObjectStart(TEXT("Event"));
				Writer->WriteValue(TEXT("EventType"), Entry.Event.Type);
				Writer->WriteValue(TEXT("Time"), Entry.Event.Time);
				if (Entry.Event.bHasDuration)
				{
					Writer->WriteValue(TEXT("Duration"), Entry.Event.Duration);
				}
				if (!Entry.Event.WaveformPath.IsEmpty())
				{
					Writer->WriteValue(
						TEXT("EventWaveformPath"),
						Entry.Event.WaveformPath
					);
				}
				if (Entry.Event.bHasWaveformLoopEnabled)
				{
					Writer->WriteValue(
						TEXT("EventWaveformLoopEnabled"),
						Entry.Event.bWaveformLoopEnabled
					);
				}
				if (Entry.Event.bHasWaveformUseVolumeEnvelope)
				{
					Writer->WriteValue(
						TEXT("EventWaveformUseVolumeEnvelope"),
						Entry.Event.bWaveformUseVolumeEnvelope
					);
				}
				if (!Entry.Event.Parameters.IsEmpty())
				{
					Writer->WriteArrayStart(TEXT("EventParameters"));
					for (const FParameter& Parameter : Entry.Event.Parameters)
					{
						Writer->WriteObjectStart();
						Writer->WriteValue(TEXT("ParameterID"), Parameter.Id);
						Writer->WriteValue(
							TEXT("ParameterValue"), Parameter.Value);
						Writer->WriteObjectEnd();
					}
					Writer->WriteArrayEnd();
				}
				Writer->WriteObjectEnd();
			}
			else if (Entry.Type == EEntryType::Parameter)
			{
				Writer->WriteObjectStart(TEXT("Parameter"));
				Writer->WriteValue(
					TEXT("ParameterID"), Entry.Parameter.Id);
				Writer->WriteValue(TEXT("Time"), Entry.Parameter.Time);
				Writer->WriteValue(
					TEXT("ParameterValue"), Entry.Parameter.Value);
				Writer->WriteObjectEnd();
			}
			else
			{
				Writer->WriteObjectStart(TEXT("ParameterCurve"));
				Writer->WriteValue(TEXT("ParameterID"), Entry.Curve.Id);
				Writer->WriteValue(TEXT("Time"), Entry.Curve.Time);
				Writer->WriteArrayStart(TEXT("ParameterCurveControlPoints"));
				for (const FCurvePoint& Point : Entry.Curve.Points)
				{
					Writer->WriteObjectStart();
					Writer->WriteValue(TEXT("Time"), Point.Time);
					Writer->WriteValue(TEXT("ParameterValue"), Point.Value);
					Writer->WriteObjectEnd();
				}
				Writer->WriteArrayEnd();
				Writer->WriteObjectEnd();
			}
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
		Writer->Close();
		return Output;
	}
}

FOpenMobileHapticsAHAPNormalizationResult
FOpenMobileHapticsAHAPPolicy::Normalize(
	const FString& Source,
	const FOpenMobileHapticsAHAPLimits& Limits,
	bool bAllowExternalAudioResources
)
{
	using namespace OpenMobileHapticsAHAPPolicyPrivate;
	if (Limits.MaximumSourceBytes < 1
		|| Limits.MaximumPatternEntries < 1
		|| Limits.MaximumParameters < 0
		|| Limits.MaximumParameterCurves < 0
		|| Limits.MaximumCurvePoints < 1
		|| !FMath::IsFinite(Limits.MaximumDurationSeconds)
		|| Limits.MaximumDurationSeconds <= 0.0)
	{
		return Fail(EOpenMobileHapticsAHAPError::InvalidValue,
			TEXT("Limits"));
	}
	const FTCHARToUTF8 UTF8(*Source);
	if (UTF8.Length() > Limits.MaximumSourceBytes)
	{
		return Fail(EOpenMobileHapticsAHAPError::SourceTooLarge,
			TEXT("Source"));
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(Source);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return Fail(EOpenMobileHapticsAHAPError::MalformedJson,
			TEXT("Root"));
	}
	double Version = 0.0;
	if (!Root->HasField(TEXT("Version")))
	{
		return Fail(EOpenMobileHapticsAHAPError::MissingVersion,
			TEXT("Version"));
	}
	if (!Root->TryGetNumberField(TEXT("Version"), Version))
	{
		return Fail(EOpenMobileHapticsAHAPError::InvalidStructure,
			TEXT("Version"));
	}
	if (!FMath::IsFinite(Version))
	{
		return Fail(EOpenMobileHapticsAHAPError::Nonfinite,
			TEXT("Version"));
	}
	if (Version != 1.0)
	{
		return Fail(EOpenMobileHapticsAHAPError::UnsupportedVersion,
			TEXT("Version"));
	}
	FString Unsupported;
	if (!HasOnlyKeys(*Root, {TEXT("Version"), TEXT("Pattern")},
		Unsupported))
	{
		return Fail(EOpenMobileHapticsAHAPError::UnsupportedKey,
			Unsupported);
	}
	const TArray<TSharedPtr<FJsonValue>>* PatternValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("Pattern"), PatternValues)
		|| !PatternValues || PatternValues->IsEmpty())
	{
		return Fail(EOpenMobileHapticsAHAPError::MissingPattern,
			TEXT("Pattern"));
	}
	if (PatternValues->Num() > Limits.MaximumPatternEntries)
	{
		return Fail(EOpenMobileHapticsAHAPError::LimitExceeded,
			TEXT("Pattern"));
	}

	TArray<FEntry> Entries;
	Entries.Reserve(PatternValues->Num());
	FOpenMobileHapticsAHAPResource Resource;
	int32 TotalParameters = 0;
	int32 TotalCurvePoints = 0;
	for (int32 EntryIndex = 0;
		EntryIndex < PatternValues->Num(); ++EntryIndex)
	{
		const TSharedPtr<FJsonObject> EntryObject =
			(*PatternValues)[EntryIndex].IsValid()
				? (*PatternValues)[EntryIndex]->AsObject()
				: nullptr;
		const FString EntryPath = FString::Printf(
			TEXT("Pattern[%d]"), EntryIndex);
		if (!EntryObject.IsValid())
		{
			return Fail(EOpenMobileHapticsAHAPError::InvalidStructure,
				EntryPath);
		}
		if (!HasOnlyKeys(*EntryObject, {
			TEXT("Event"), TEXT("Parameter"), TEXT("ParameterCurve")
		}, Unsupported))
		{
			return Fail(EOpenMobileHapticsAHAPError::UnsupportedKey,
				EntryPath + TEXT(".") + Unsupported);
		}
		const bool bHasEvent = EntryObject->HasField(TEXT("Event"));
		const bool bHasParameter = EntryObject->HasField(TEXT("Parameter"));
		const bool bHasCurve = EntryObject->HasField(TEXT("ParameterCurve"));
		if (static_cast<int32>(bHasEvent)
			+ static_cast<int32>(bHasParameter)
			+ static_cast<int32>(bHasCurve) != 1)
		{
			return Fail(EOpenMobileHapticsAHAPError::InvalidStructure,
				EntryPath);
		}

		const TCHAR* PayloadName = bHasEvent
			? TEXT("Event")
			: bHasParameter ? TEXT("Parameter") : TEXT("ParameterCurve");
		const TSharedPtr<FJsonObject>* Payload = nullptr;
		if (!EntryObject->TryGetObjectField(PayloadName, Payload)
			|| !Payload || !Payload->IsValid())
		{
			return Fail(EOpenMobileHapticsAHAPError::InvalidStructure,
				EntryPath + TEXT(".") + PayloadName);
		}
		FEntry Entry;
		FOpenMobileHapticsAHAPNormalizationResult ParseResult;
		if (bHasEvent)
		{
			ParseResult = ParseEvent(**Payload, EntryIndex, Limits,
				Entry, Resource, TotalParameters,
				bAllowExternalAudioResources);
		}
		else if (bHasParameter)
		{
			ParseResult = ParseParameter(**Payload, EntryIndex, Limits,
				Entry, Resource, TotalParameters);
		}
		else
		{
			ParseResult = ParseCurve(**Payload, EntryIndex, Limits,
				Entry, Resource, TotalCurvePoints);
		}
		if (ParseResult.Error != EOpenMobileHapticsAHAPError::None)
		{
			return ParseResult;
		}
		Entries.Add(MoveTemp(Entry));
	}
	if (Resource.HapticEventCount + Resource.AudioEventCount == 0)
	{
		return Fail(EOpenMobileHapticsAHAPError::MissingPattern,
			TEXT("Pattern.Event"));
	}
	Resource.PatternEntryCount = Entries.Num();
	Resource.NormalizedJson = WriteNormalized(Entries);
	FOpenMobileHapticsAHAPNormalizationResult Result;
	Result.bSuccess = true;
	Result.Resource = MoveTemp(Resource);
	return Result;
}

FString FOpenMobileHapticsAHAPPolicy::DescribeError(
	const FOpenMobileHapticsAHAPNormalizationResult& Result
)
{
	const TCHAR* Description = TEXT("AHAP validation failed");
	switch (Result.Error)
	{
	case EOpenMobileHapticsAHAPError::SourceTooLarge:
		Description = TEXT("AHAP exceeds the source size limit");
		break;
	case EOpenMobileHapticsAHAPError::MalformedJson:
		Description = TEXT("AHAP is not valid JSON");
		break;
	case EOpenMobileHapticsAHAPError::MissingVersion:
		Description = TEXT("AHAP is missing Version");
		break;
	case EOpenMobileHapticsAHAPError::UnsupportedVersion:
		Description = TEXT("AHAP Version is not supported");
		break;
	case EOpenMobileHapticsAHAPError::MissingPattern:
		Description = TEXT("AHAP requires a nonempty event pattern");
		break;
	case EOpenMobileHapticsAHAPError::MissingKey:
		Description = TEXT("AHAP is missing a required key");
		break;
	case EOpenMobileHapticsAHAPError::UnsupportedKey:
		Description = TEXT("AHAP contains an unsupported key");
		break;
	case EOpenMobileHapticsAHAPError::InvalidStructure:
		Description = TEXT("AHAP contains an invalid value type or structure");
		break;
	case EOpenMobileHapticsAHAPError::Nonfinite:
		Description = TEXT("AHAP contains a nonfinite number");
		break;
	case EOpenMobileHapticsAHAPError::InvalidValue:
		Description = TEXT("AHAP contains an unsupported or out-of-range value");
		break;
	case EOpenMobileHapticsAHAPError::LimitExceeded:
		Description = TEXT("AHAP exceeds a pattern limit");
		break;
	case EOpenMobileHapticsAHAPError::ExternalResourcePath:
		Description = TEXT("AHAP external audio resources are not cookable");
		break;
	default:
		break;
	}
	return Result.FieldPath.IsEmpty()
		? FString(Description)
		: FString::Printf(TEXT("%s: %s"), *Result.FieldPath, Description);
}
