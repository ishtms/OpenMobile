#include "OpenMobileHapticPatternAsset.h"

#include "Misc/Crc.h"
#include "OpenMobileHapticsBudgetPolicy.h"
#include "OpenMobileHapticsPatternCompiler.h"
#include "OpenMobileHapticsRepeatPolicy.h"
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
	constexpr int32 MaximumSerializedCurveCount = 128;
	constexpr int32 MaximumSerializedCurvePointCount = 4096;

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
		int32 CurveIndex,
		int32 ControlPointIndex,
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
		case EOpenMobileHapticsPatternCompileError::CurveLimit:
			Description = TEXT("the pattern exceeds the parameter curve limit");
			break;
		case EOpenMobileHapticsPatternCompileError::CurvePointLimit:
			Description = TEXT("the pattern exceeds the curve control-point limit");
			break;
		case EOpenMobileHapticsPatternCompileError::InvalidCurveType:
			Description = TEXT("the curve parameter is invalid");
			break;
		case EOpenMobileHapticsPatternCompileError::InvalidCurve:
			Description = TEXT("a curve value or structure is invalid");
			break;
		case EOpenMobileHapticsPatternCompileError::CurveUnsorted:
			Description = TEXT("curve times are not strictly increasing");
			break;
		case EOpenMobileHapticsPatternCompileError::CurveOverlap:
			Description = TEXT("curves for the same parameter overlap");
			break;
		case EOpenMobileHapticsPatternCompileError::CurveDurationLimit:
			Description = TEXT("a curve exceeds the compiled timeline");
			break;
		default:
			break;
		}
		if (!SourcePattern.Events.IsValidIndex(EventIndex))
		{
			if (!SourcePattern.ParameterCurves.IsValidIndex(CurveIndex))
			{
				return FString(Description);
			}
			const FOpenMobileHapticParameterCurve& Curve =
				SourcePattern.ParameterCurves[CurveIndex];
			const TCHAR* CurveField = TEXT("ControlPoints");
			if (Error
				== EOpenMobileHapticsPatternCompileError::InvalidCurveType)
			{
				CurveField = TEXT("Parameter");
			}
			else if (Error
					== EOpenMobileHapticsPatternCompileError::CurveOverlap
				|| Error
					== EOpenMobileHapticsPatternCompileError::CurveDurationLimit)
			{
				CurveField = TEXT("StartTimeSeconds");
			}
			if (Curve.ControlPoints.IsValidIndex(ControlPointIndex))
			{
				const FOpenMobileHapticCurvePoint& Point =
					Curve.ControlPoints[ControlPointIndex];
				CurveField = !FMath::IsFinite(Point.Value)
					|| Point.Value < 0.0f || Point.Value > 1.0f
						? TEXT("Value")
						: TEXT("RelativeTimeSeconds");
				return FString::Printf(
					TEXT("Curve %d control point %d %s: %s"),
					CurveIndex,
					ControlPointIndex,
					CurveField,
					Description
				);
			}
			return FString::Printf(
				TEXT("Curve %d %s: %s"),
				CurveIndex,
				CurveField,
				Description
			);
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

	bool LessEvent(
		const FOpenMobileHapticPatternEvent& Left,
		const FOpenMobileHapticPatternEvent& Right
	)
	{
		if (Left.StartTimeSeconds != Right.StartTimeSeconds)
		{
			return Left.StartTimeSeconds < Right.StartTimeSeconds;
		}
		if (Left.Type != Right.Type)
		{
			return static_cast<uint8>(Left.Type)
				< static_cast<uint8>(Right.Type);
		}
		if (Left.DurationSeconds != Right.DurationSeconds)
		{
			return Left.DurationSeconds < Right.DurationSeconds;
		}
		if (Left.Intensity != Right.Intensity)
		{
			return Left.Intensity < Right.Intensity;
		}
		if (Left.Sharpness != Right.Sharpness)
		{
			return Left.Sharpness < Right.Sharpness;
		}
		return Left.FrequencyIntent < Right.FrequencyIntent;
	}

	bool LessCurvePoint(
		const FOpenMobileHapticCurvePoint& Left,
		const FOpenMobileHapticCurvePoint& Right
	)
	{
		if (Left.RelativeTimeSeconds != Right.RelativeTimeSeconds)
		{
			return Left.RelativeTimeSeconds < Right.RelativeTimeSeconds;
		}
		return Left.Value < Right.Value;
	}

	bool LessCurve(
		const FOpenMobileHapticParameterCurve& Left,
		const FOpenMobileHapticParameterCurve& Right
	)
	{
		if (Left.StartTimeSeconds != Right.StartTimeSeconds)
		{
			return Left.StartTimeSeconds < Right.StartTimeSeconds;
		}
		if (Left.Parameter != Right.Parameter)
		{
			return static_cast<uint8>(Left.Parameter)
				< static_cast<uint8>(Right.Parameter);
		}
		if (Left.ControlPoints.Num() != Right.ControlPoints.Num())
		{
			return Left.ControlPoints.Num() < Right.ControlPoints.Num();
		}
		for (int32 Index = 0; Index < Left.ControlPoints.Num(); ++Index)
		{
			const FOpenMobileHapticCurvePoint& LeftPoint =
				Left.ControlPoints[Index];
			const FOpenMobileHapticCurvePoint& RightPoint =
				Right.ControlPoints[Index];
			if (LeftPoint.RelativeTimeSeconds
				!= RightPoint.RelativeTimeSeconds)
			{
				return LeftPoint.RelativeTimeSeconds
					< RightPoint.RelativeTimeSeconds;
			}
			if (LeftPoint.Value != RightPoint.Value)
			{
				return LeftPoint.Value < RightPoint.Value;
			}
		}
		return false;
	}

	bool LessMarker(
		const FOpenMobileHapticPatternMarker& Left,
		const FOpenMobileHapticPatternMarker& Right
	)
	{
		if (Left.TimeSeconds != Right.TimeSeconds)
		{
			return Left.TimeSeconds < Right.TimeSeconds;
		}
		return Left.Name.LexicalLess(Right.Name);
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

	if (SerializedVersion >= 3)
	{
		int32 CurveCount = ParameterCurves.Num();
		Archive << CurveCount;
		if (CurveCount < 0 || CurveCount > MaximumSerializedCurveCount)
		{
			Archive.SetError();
			return false;
		}
		if (Archive.IsLoading())
		{
			ParameterCurves.SetNum(CurveCount);
		}
		int32 TotalPointCount = 0;
		for (FOpenMobileHapticCookedParameterCurve& Curve : ParameterCurves)
		{
			uint8 Parameter = static_cast<uint8>(Curve.Parameter);
			Archive << Parameter;
			Archive << Curve.StartTimeMicroseconds;
			int32 PointCount = Curve.ControlPoints.Num();
			Archive << PointCount;
			if (PointCount < 0
				|| PointCount
					> MaximumSerializedCurvePointCount - TotalPointCount)
			{
				Archive.SetError();
				return false;
			}
			TotalPointCount += PointCount;
			if (Archive.IsLoading())
			{
				if (Parameter > static_cast<uint8>(
					EOpenMobileHapticCurveParameter::SharpnessControl))
				{
					Archive.SetError();
					return false;
				}
				Curve.Parameter =
					static_cast<EOpenMobileHapticCurveParameter>(Parameter);
				Curve.ControlPoints.SetNum(PointCount);
			}
			for (FOpenMobileHapticCookedCurvePoint& Point : Curve.ControlPoints)
			{
				Archive << Point.RelativeTimeMicroseconds;
				Archive << Point.Value;
			}
		}
	}
	else if (Archive.IsLoading())
	{
		ParameterCurves.Reset();
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
	ParameterCurves.Reset();
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
	const int32 CurveCount = SourcePattern.ParameterCurves.Num();
	HashValue(Hash, CurveCount);
	for (const FOpenMobileHapticParameterCurve& Curve
		: SourcePattern.ParameterCurves)
	{
		const uint8 Parameter = static_cast<uint8>(Curve.Parameter);
		HashValue(Hash, Parameter);
		HashValue(Hash, Curve.StartTimeSeconds);
		const int32 PointCount = Curve.ControlPoints.Num();
		HashValue(Hash, PointCount);
		for (const FOpenMobileHapticCurvePoint& Point : Curve.ControlPoints)
		{
			HashValue(Hash, Point.RelativeTimeSeconds);
			HashValue(Hash, Point.Value);
		}
	}
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	HashValue(
		Hash,
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternEvents(
			Settings->MaximumPatternEventCount
		)
	);
	HashValue(
		Hash,
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternCurves(
			Settings->MaximumPatternCurveCount
		)
	);
	HashValue(
		Hash,
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternCurvePoints(
			Settings->MaximumPatternCurvePointCount
		)
	);
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
#if !UE_BUILD_SHIPPING
	if (bHasCookedPreviewData)
	{
		return CookedPattern.DataFormatVersion
				== FOpenMobileHapticCookedPatternData::CurrentFormatVersion
			&& !CookedPattern.Events.IsEmpty();
	}
#endif
	return CookedPattern.DataFormatVersion
			== FOpenMobileHapticCookedPatternData::CurrentFormatVersion
		&& !CookedPattern.Events.IsEmpty()
		&& CookedPattern.SourceHash == ComputeSourceHash();
}

#if !UE_BUILD_SHIPPING
bool UOpenMobileHapticPatternAsset::InitializeCookedPreviewData(
	const FOpenMobileHapticCookedPatternData& InCookedPattern,
	const FOpenMobileHapticLoopOptions& InLoop,
	FName InCategory,
	EOpenMobileHapticFallbackPolicy InFallbackPolicy,
	EOpenMobileHapticFallbackFloor InLowestAllowedFallback,
	FName InPrimitiveOrPresetFallback,
	bool bInAllowSemanticFallback,
	EOpenMobileHapticSemanticEffect InSemanticFallback
)
{
	if (InCookedPattern.DataFormatVersion
			!= FOpenMobileHapticCookedPatternData::CurrentFormatVersion
		|| InCookedPattern.Events.IsEmpty()
		|| InCategory.IsNone()
		|| static_cast<uint8>(InFallbackPolicy) > static_cast<uint8>(
			EOpenMobileHapticFallbackPolicy::NoEffectAllowed)
		|| static_cast<uint8>(InLowestAllowedFallback) > static_cast<uint8>(
			EOpenMobileHapticFallbackFloor::BasicVibration)
		|| static_cast<uint8>(InSemanticFallback) > static_cast<uint8>(
			EOpenMobileHapticSemanticEffect::Achievement))
	{
		return false;
	}
	CookedPattern = InCookedPattern;
	Loop = InLoop;
	DefaultCategory = InCategory;
	FallbackPolicy = InFallbackPolicy;
	LowestAllowedFallback = InLowestAllowedFallback;
	PrimitiveOrPresetFallback = InPrimitiveOrPresetFallback;
	bAllowSemanticFallback = bInAllowSemanticFallback;
	SemanticFallback = InSemanticFallback;
	bHasCookedPreviewData = true;
	return true;
}
#endif

bool UOpenMobileHapticPatternAsset::ValidateMetadata(
	TArray<FString>& Errors
) const
{
	if (PatternVersion < 1)
	{
		Errors.Add(TEXT("PatternVersion must be at least one."));
	}
	if (DefaultCategory.IsNone())
	{
		Errors.Add(TEXT("DefaultCategory cannot be empty."));
	}
	if (static_cast<uint8>(Priority) > static_cast<uint8>(
		EOpenMobileHapticChannelPriority::Critical))
	{
		Errors.Add(TEXT("Priority is invalid."));
	}
	if (static_cast<uint8>(OverlapPolicy) > static_cast<uint8>(
		EOpenMobileHapticOverlapPolicy::MixWhenSupported))
	{
		Errors.Add(TEXT("OverlapPolicy is invalid."));
	}
	if (static_cast<uint8>(FallbackPolicy) > static_cast<uint8>(
		EOpenMobileHapticFallbackPolicy::NoEffectAllowed))
	{
		Errors.Add(TEXT("FallbackPolicy is invalid."));
	}
	if (static_cast<uint8>(LowestAllowedFallback) > static_cast<uint8>(
		EOpenMobileHapticFallbackFloor::BasicVibration))
	{
		Errors.Add(TEXT("LowestAllowedFallback is invalid."));
	}
	if (bAllowSemanticFallback
		&& static_cast<uint8>(SemanticFallback) > static_cast<uint8>(
			EOpenMobileHapticSemanticEffect::Achievement))
	{
		Errors.Add(TEXT("SemanticFallback is invalid."));
	}
	return Errors.IsEmpty();
}

bool UOpenMobileHapticPatternAsset::ValidatePlatformOverrides(
	TArray<FString>& Errors
) const
{
	auto ValidateOverride = [&Errors](
		const UOpenMobileHapticPlatformPatternAsset* Override,
		EOpenMobileHapticOverridePlatform Platform,
		const TCHAR* Label
	)
	{
		if (!Override)
		{
			Errors.Add(FString::Printf(
				TEXT("%s could not be loaded."),
				Label
			));
			return;
		}
		if (Override->GetOverridePlatform() != Platform)
		{
			Errors.Add(FString::Printf(
				TEXT("%s targets the wrong platform."),
				Label
			));
			return;
		}
		TArray<FString> OverrideErrors;
		if (!Override->Validate(OverrideErrors))
		{
			for (const FString& Error : OverrideErrors)
			{
				Errors.Add(FString::Printf(
					TEXT("%s: %s"),
					Label,
					*Error
				));
			}
		}
	};

	if (!AndroidOverride.IsNull())
	{
		const UOpenMobileHapticAndroidPatternAsset* Override =
			AndroidOverride.LoadSynchronous();
		ValidateOverride(
			Override,
			EOpenMobileHapticOverridePlatform::Android,
			TEXT("AndroidOverride")
		);
		if (Override
			&& (!Override->ShouldCookForPlatform(TEXT("Android"))
				|| Override->ShouldCookForPlatform(TEXT("IOS"))))
		{
			Errors.Add(TEXT("AndroidOverride cook filtering is invalid."));
		}
	}
	if (!IOSOverride.IsNull())
	{
		const UOpenMobileHapticIOSPatternAsset* Override =
			IOSOverride.LoadSynchronous();
		ValidateOverride(
			Override,
			EOpenMobileHapticOverridePlatform::IOS,
			TEXT("IOSOverride")
		);
		if (Override
			&& Override->ShouldCookForPlatform(TEXT("Android")))
		{
			Errors.Add(TEXT("IOSOverride cook filtering is invalid."));
		}
	}
	return Errors.IsEmpty();
}

#if WITH_EDITORONLY_DATA
void UOpenMobileHapticPatternAsset::NormalizeEditorData()
{
	for (FOpenMobileHapticParameterCurve& Curve :
		SourcePattern.ParameterCurves)
	{
		Curve.ControlPoints.StableSort(
			OpenMobileHapticPatternAssetPrivate::LessCurvePoint
		);
	}
	SourcePattern.Events.StableSort(
		OpenMobileHapticPatternAssetPrivate::LessEvent
	);
	SourcePattern.ParameterCurves.StableSort(
		OpenMobileHapticPatternAssetPrivate::LessCurve
	);
	Markers.StableSort(OpenMobileHapticPatternAssetPrivate::LessMarker);
}
#endif

#if WITH_EDITOR
bool UOpenMobileHapticPatternAsset::ValidateEditorData(
	TArray<FString>& Errors
) const
{
#if WITH_EDITORONLY_DATA
	constexpr int32 MaximumMarkerCount = 64;
	if (Markers.Num() > MaximumMarkerCount)
	{
		Errors.Add(TEXT("Markers exceed the editor limit of 64."));
	}
	TSet<FName> MarkerNames;
	double PreviousTime = -1.0;
	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		const FOpenMobileHapticPatternMarker& Marker = Markers[Index];
		if (Marker.Name.IsNone())
		{
			Errors.Add(FString::Printf(
				TEXT("Marker %d must have a name."),
				Index
			));
		}
		else if (MarkerNames.Contains(Marker.Name))
		{
			Errors.Add(FString::Printf(
				TEXT("Marker name %s is duplicated."),
				*Marker.Name.ToString()
			));
		}
		MarkerNames.Add(Marker.Name);
		if (!FMath::IsFinite(Marker.TimeSeconds)
			|| Marker.TimeSeconds < 0.0)
		{
			Errors.Add(FString::Printf(
				TEXT("Marker %d TimeSeconds must be finite and nonnegative."),
				Index
			));
		}
		if (Marker.TimeSeconds < PreviousTime)
		{
			Errors.Add(TEXT("Markers must be sorted by time."));
		}
		PreviousTime = Marker.TimeSeconds;
	}
#endif
	return Errors.IsEmpty();
}
#endif

bool UOpenMobileHapticPatternAsset::RebuildDerivedData(
	TArray<FString>& Errors
)
{
	Errors.Reset();
#if WITH_EDITORONLY_DATA
	if (!ValidateMetadata(Errors))
	{
		CookedPattern.Reset();
		return false;
	}
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
			Result.CurveIndex,
			Result.ControlPointIndex,
			SourcePattern
		));
		return false;
	}
	if (Loop.bLoop)
	{
		const FOpenMobileHapticsRepeatPlanResult Repeat =
			FOpenMobileHapticsRepeatPolicy::Resolve(
				Loop,
				Result.Pattern->GetDurationSeconds(),
				Settings->MaximumFiniteRepeatCount,
				Settings->MaximumContinuousDurationSeconds
			);
		if (!Repeat.IsSuccess())
		{
			CookedPattern.Reset();
			Errors.Add(TEXT("Loop options are outside the configured limits."));
			return false;
		}
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
	Rebuilt.ParameterCurves.Reserve(
		Result.Pattern->GetParameterCurves().Num()
	);
	for (const FOpenMobileHapticsCompiledParameterCurve& SourceCurve
		: Result.Pattern->GetParameterCurves())
	{
		FOpenMobileHapticCookedParameterCurve& Curve =
			Rebuilt.ParameterCurves.AddDefaulted_GetRef();
		Curve.Parameter = SourceCurve.Parameter;
		Curve.StartTimeMicroseconds = ToMicroseconds(
			SourceCurve.StartTimeSeconds
		);
		Curve.ControlPoints.Reserve(SourceCurve.ControlPoints.Num());
		for (const FOpenMobileHapticsCompiledCurvePoint& SourcePoint
			: SourceCurve.ControlPoints)
		{
			FOpenMobileHapticCookedCurvePoint& Point =
				Curve.ControlPoints.AddDefaulted_GetRef();
			Point.RelativeTimeMicroseconds = ToMicroseconds(
				SourcePoint.RelativeTimeSeconds
			);
			Point.Value = ToNormalizedUInt16(SourcePoint.Value);
		}
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
	NormalizeEditorData();
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
	TArray<FString> ValidationErrors;
	if (!IsTemplate() && !ValidateForEditor(ValidationErrors))
	{
		for (const FString& Error : ValidationErrors)
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
#endif
	Super::PreSave(SaveContext);
}

#if WITH_EDITOR
bool UOpenMobileHapticPatternAsset::ValidateForEditor(
	TArray<FString>& Errors
) const
{
	Errors.Reset();
	ValidateMetadata(Errors);
	ValidateEditorData(Errors);
	ValidatePlatformOverrides(Errors);
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	const FOpenMobileHapticsPatternCompileResult CompileResult =
		FOpenMobileHapticsPatternCompiler::Compile(
			SourcePattern,
			FOpenMobileHapticsPatternCompiler::MakeLimits(*Settings, {})
		);
	if (!CompileResult.IsSuccess())
	{
		Errors.Add(OpenMobileHapticPatternAssetPrivate::DescribeCompileError(
			CompileResult.Error,
			CompileResult.EventIndex,
			CompileResult.CurveIndex,
			CompileResult.ControlPointIndex,
			SourcePattern
		));
	}
	if (!IsDerivedDataCurrent())
	{
		Errors.Add(TEXT("Cooked pattern data is out of date."));
	}
	return Errors.IsEmpty();
}

void UOpenMobileHapticPatternAsset::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent
)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	NormalizeEditorData();
	TArray<FString> Errors;
	RebuildDerivedData(Errors);
}

EDataValidationResult UOpenMobileHapticPatternAsset::IsDataValid(
	FDataValidationContext& Context
) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TArray<FString> Errors;
	if (!ValidateForEditor(Errors))
	{
		for (const FString& Error : Errors)
		{
			Context.AddError(FText::FromString(Error));
		}
		return EDataValidationResult::Invalid;
	}
	return CombineDataValidationResults(Result, EDataValidationResult::Valid);
}
#endif
