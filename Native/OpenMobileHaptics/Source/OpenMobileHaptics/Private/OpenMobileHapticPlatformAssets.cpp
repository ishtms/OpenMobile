#include "OpenMobileHapticPlatformAssets.h"

#include "OpenMobileHapticsAHAPPolicy.h"
#include "OpenMobileHapticsEnvelopePolicy.h"
#include "OpenMobileHapticsSettings.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#include "Misc/DataValidation.h"
#endif

namespace OpenMobileHapticPlatformAssetsPrivate
{
	FName PrimitiveName(EOpenMobileHapticAndroidPrimitive Primitive)
	{
		switch (Primitive)
		{
		case EOpenMobileHapticAndroidPrimitive::Tick:
			return TEXT("Tick");
		case EOpenMobileHapticAndroidPrimitive::LowTick:
			return TEXT("LowTick");
		case EOpenMobileHapticAndroidPrimitive::Click:
			return TEXT("Click");
		case EOpenMobileHapticAndroidPrimitive::Thud:
			return TEXT("Thud");
		case EOpenMobileHapticAndroidPrimitive::Spin:
			return TEXT("Spin");
		case EOpenMobileHapticAndroidPrimitive::QuickRise:
			return TEXT("QuickRise");
		case EOpenMobileHapticAndroidPrimitive::SlowRise:
			return TEXT("SlowRise");
		case EOpenMobileHapticAndroidPrimitive::QuickFall:
			return TEXT("QuickFall");
		default:
			return NAME_None;
		}
	}

	bool SupportsPrimitive(
		EOpenMobileHapticAndroidPrimitive Primitive,
		const FOpenMobileHapticCapabilities& Capabilities
	)
	{
		const FName Name = PrimitiveName(Primitive);
		for (const FOpenMobileHapticNamedSupport& Support :
			Capabilities.PrimitiveSupport)
		{
			if (Support.Name == Name)
			{
				return Support.Support
					== EOpenMobileHapticSupportState::Supported;
			}
		}
		return Capabilities.Primitives
			== EOpenMobileHapticSupportState::Supported;
	}
}

bool UOpenMobileHapticPlatformPatternAsset::ShouldCookForPlatform(
	FName PlatformName
) const
{
	if (GetOverridePlatform() == EOpenMobileHapticOverridePlatform::Android)
	{
		return PlatformName == TEXT("Android");
	}
	if (GetOverridePlatform() == EOpenMobileHapticOverridePlatform::IOS)
	{
		return PlatformName == TEXT("IOS")
			&& GetDefault<UOpenMobileHapticsSettings>()
				->IOS.bPackageAHAPResources;
	}
	return false;
}

bool UOpenMobileHapticPlatformPatternAsset::NeedsLoadForTargetPlatform(
	const ITargetPlatform* TargetPlatform
) const
{
#if WITH_EDITOR
	return (!TargetPlatform
			|| ShouldCookForPlatform(*TargetPlatform->IniPlatformName()))
		&& Super::NeedsLoadForTargetPlatform(TargetPlatform);
#else
	return Super::NeedsLoadForTargetPlatform(TargetPlatform);
#endif
}

#if WITH_EDITOR
EDataValidationResult UOpenMobileHapticPlatformPatternAsset::IsDataValid(
	FDataValidationContext& Context
) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TArray<FString> Errors;
	if (!Validate(Errors))
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

int32 UOpenMobileHapticAndroidPatternAsset::GetMinimumOSVersion() const
{
	int32 FormatMinimum = 26;
	if (Format == EOpenMobileHapticAndroidPatternFormat::Primitives)
	{
		FormatMinimum = 30;
	}
	else if (Format == EOpenMobileHapticAndroidPatternFormat::BasicEnvelope
		|| Format
			== EOpenMobileHapticAndroidPatternFormat::WaveformEnvelope)
	{
		FormatMinimum = 36;
	}
	return FMath::Max(FormatMinimum, MinimumAndroidAPI);
}

bool UOpenMobileHapticAndroidPatternAsset::Supports(
	const FOpenMobileHapticCapabilities& Capabilities,
	int32 OSVersion
) const
{
	if (OSVersion < GetMinimumOSVersion())
	{
		return false;
	}
	switch (Format)
	{
	case EOpenMobileHapticAndroidPatternFormat::Primitives:
		if (Capabilities.MaximumEventCount.bKnown
			&& Primitives.Num() > Capabilities.MaximumEventCount.Value)
		{
			return false;
		}
		for (const FOpenMobileHapticAndroidPrimitiveStep& Step : Primitives)
		{
			if (!OpenMobileHapticPlatformAssetsPrivate::SupportsPrimitive(
				Step.Primitive,
				Capabilities
			))
			{
				return false;
			}
		}
		return true;
	case EOpenMobileHapticAndroidPatternFormat::Waveform:
		return Capabilities.WaveformTiming
				== EOpenMobileHapticSupportState::Supported
			&& (!Capabilities.MaximumEventCount.bKnown
				|| WaveformTimingsMilliseconds.Num()
					<= Capabilities.MaximumEventCount.Value);
	case EOpenMobileHapticAndroidPatternFormat::BasicEnvelope:
	case EOpenMobileHapticAndroidPatternFormat::WaveformEnvelope:
		return FOpenMobileHapticsEnvelopePolicy::Resolve(
			*this,
			Capabilities,
			OSVersion,
			1.0f,
			EOpenMobileHapticFallbackPolicy::ExactOnly
		).Outcome == EOpenMobileHapticsEnvelopeOutcome::Ready;
	default:
		return false;
	}
}

bool UOpenMobileHapticAndroidPatternAsset::Validate(
	TArray<FString>& Errors
) const
{
	Errors.Reset();
	if (MinimumAndroidAPI < 26)
	{
		Errors.Add(TEXT("MinimumAndroidAPI must be at least 26."));
	}
	if (static_cast<uint8>(Format) > static_cast<uint8>(
		EOpenMobileHapticAndroidPatternFormat::WaveformEnvelope))
	{
		Errors.Add(TEXT("Format is invalid."));
		return false;
	}
	if (Format == EOpenMobileHapticAndroidPatternFormat::Primitives)
	{
		if (Primitives.IsEmpty())
		{
			Errors.Add(TEXT("Primitives must contain at least one step."));
		}
		for (int32 Index = 0; Index < Primitives.Num(); ++Index)
		{
			const FOpenMobileHapticAndroidPrimitiveStep& Step = Primitives[Index];
			if (static_cast<uint8>(Step.Primitive) > static_cast<uint8>(
				EOpenMobileHapticAndroidPrimitive::QuickFall))
			{
				Errors.Add(FString::Printf(
					TEXT("Primitive %d type is invalid."),
					Index
				));
			}
			if (!FMath::IsFinite(Step.Scale)
				|| Step.Scale < 0.0f || Step.Scale > 1.0f)
			{
				Errors.Add(FString::Printf(
					TEXT("Primitive %d Scale must be normalized."),
					Index
				));
			}
			if (Step.DelayMilliseconds < 0)
			{
				Errors.Add(FString::Printf(
					TEXT("Primitive %d DelayMilliseconds cannot be negative."),
					Index
				));
			}
		}
	}
	else if (Format == EOpenMobileHapticAndroidPatternFormat::Waveform)
	{
		if (WaveformTimingsMilliseconds.IsEmpty()
			|| WaveformTimingsMilliseconds.Num() != WaveformAmplitudes.Num())
		{
			Errors.Add(TEXT(
				"Waveform timings and amplitudes must have the same nonzero count."
			));
		}
		for (int32 Index = 0; Index < WaveformTimingsMilliseconds.Num(); ++Index)
		{
			if (WaveformTimingsMilliseconds[Index] < 0)
			{
				Errors.Add(FString::Printf(
					TEXT("Waveform timing %d cannot be negative."),
					Index
				));
			}
		}
		if (!WaveformTimingsMilliseconds.ContainsByPredicate(
			[](int32 Timing) { return Timing > 0; }))
		{
			Errors.Add(TEXT("Waveform must contain a positive timing."));
		}
		for (int32 Index = 0; Index < WaveformAmplitudes.Num(); ++Index)
		{
			if (WaveformAmplitudes[Index] < 0
				|| WaveformAmplitudes[Index] > 255)
			{
				Errors.Add(FString::Printf(
					TEXT("Waveform amplitude %d must be from 0 through 255."),
					Index
				));
			}
		}
		if (WaveformRepeatIndex < -1
			|| WaveformRepeatIndex >= WaveformTimingsMilliseconds.Num())
		{
			Errors.Add(TEXT("WaveformRepeatIndex is outside the waveform."));
		}
	}
	else
	{
		if (EnvelopePoints.Num() < 2)
		{
			Errors.Add(TEXT("EnvelopePoints must contain at least two points."));
		}
		float PreviousTime = 0.0f;
		for (int32 Index = 0; Index < EnvelopePoints.Num(); ++Index)
		{
			const FOpenMobileHapticAndroidEnvelopePoint& Point =
				EnvelopePoints[Index];
			if (!FMath::IsFinite(Point.TimeSeconds)
				|| Point.TimeSeconds <= PreviousTime)
			{
				Errors.Add(FString::Printf(
					TEXT("Envelope point %d TimeSeconds must increase."),
					Index
				));
			}
			if (!FMath::IsFinite(Point.Amplitude)
				|| Point.Amplitude < 0.0f || Point.Amplitude > 1.0f)
			{
				Errors.Add(FString::Printf(
					TEXT("Envelope point %d Amplitude must be normalized."),
					Index
				));
			}
			if (Format
					== EOpenMobileHapticAndroidPatternFormat::WaveformEnvelope
				&& (!FMath::IsFinite(Point.FrequencyHz)
					|| Point.FrequencyHz <= 0.0f))
			{
				Errors.Add(FString::Printf(
					TEXT("Envelope point %d FrequencyHz must be positive."),
					Index
				));
			}
			if (Format
					== EOpenMobileHapticAndroidPatternFormat::BasicEnvelope
				&& (!FMath::IsFinite(Point.Sharpness)
					|| Point.Sharpness < 0.0f || Point.Sharpness > 1.0f))
			{
				Errors.Add(FString::Printf(
					TEXT("Envelope point %d Sharpness must be normalized."),
					Index
				));
			}
			PreviousTime = Point.TimeSeconds;
		}
		if (Format == EOpenMobileHapticAndroidPatternFormat::BasicEnvelope
			&& !EnvelopePoints.IsEmpty()
			&& !FMath::IsNearlyZero(EnvelopePoints.Last().Amplitude))
		{
			Errors.Add(TEXT("Basic envelopes must end at zero intensity."));
		}
	}
	return Errors.IsEmpty();
}

bool UOpenMobileHapticIOSPatternAsset::Supports(
	const FOpenMobileHapticCapabilities& Capabilities,
	int32 OSVersion
) const
{
	return OSVersion >= GetMinimumOSVersion()
		&& Capabilities.AHAP == EOpenMobileHapticSupportState::Supported
		&& (!ContainsAudioEvents()
			|| Capabilities.AudioEvents
				== EOpenMobileHapticSupportState::Supported);
}

void UOpenMobileHapticIOSPatternAsset::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject) && !AssetImportData)
	{
		AssetImportData = NewObject<UAssetImportData>(
			this,
			TEXT("AssetImportData")
		);
	}
#endif
}

bool UOpenMobileHapticIOSPatternAsset::SetAHAPSource(
	const FString& Source,
	TArray<FString>& Errors
)
{
	Errors.Reset();
	const FOpenMobileHapticsAHAPNormalizationResult Result =
		FOpenMobileHapticsAHAPPolicy::Normalize(Source);
	if (!Result.bSuccess)
	{
		Errors.Add(FOpenMobileHapticsAHAPPolicy::DescribeError(Result));
		return false;
	}
	AHAPJson = Result.Resource.NormalizedJson;
	AHAPDurationSeconds = Result.Resource.DurationSeconds;
	bRequiresAdvancedPlayer = Result.Resource.bRequiresAdvancedPlayer;
	bContainsAudioEvents = Result.Resource.bContainsAudioEvents;
	return true;
}

bool UOpenMobileHapticIOSPatternAsset::Validate(
	TArray<FString>& Errors
) const
{
	Errors.Reset();
	if (MinimumIOSMajorVersion < 13)
	{
		Errors.Add(TEXT("MinimumIOSMajorVersion must be at least 13."));
	}
	const FOpenMobileHapticsAHAPNormalizationResult Result =
		FOpenMobileHapticsAHAPPolicy::Normalize(AHAPJson);
	if (!Result.bSuccess)
	{
		Errors.Add(FOpenMobileHapticsAHAPPolicy::DescribeError(Result));
		return false;
	}
	if (Result.Resource.NormalizedJson != AHAPJson
		|| Result.Resource.DurationSeconds != AHAPDurationSeconds
		|| Result.Resource.bRequiresAdvancedPlayer
			!= bRequiresAdvancedPlayer
		|| Result.Resource.bContainsAudioEvents != bContainsAudioEvents)
	{
		Errors.Add(TEXT("AHAP derived data is stale."));
	}
	return Errors.IsEmpty();
}
