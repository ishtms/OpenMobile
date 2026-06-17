#include "OpenMobileHapticsAndroidWaveformPolicy.h"

#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticPlatformAssets.h"
#include "OpenMobileHapticsSettings.h"

namespace OpenMobileHapticsAndroidWaveformPolicyPrivate
{
	constexpr int32 DefaultAmplitude = -1;
	constexpr int32 MaximumNativeSegmentCount = 4096;

	struct FAmplitudeChange
	{
		int64 TimeMilliseconds = 0;
		int32 Amplitude = 0;
		int32 Delta = 0;
	};

	FOpenMobileHapticsAndroidWaveformResolution Unavailable(
		EOpenMobileHapticFallbackPolicy FallbackPolicy,
		FName Reason
	)
	{
		FOpenMobileHapticsAndroidWaveformResolution Resolution;
		Resolution.Outcome =
			FallbackPolicy == EOpenMobileHapticFallbackPolicy::ExactOnly
				? EOpenMobileHapticsAndroidWaveformOutcome::Rejected
				: EOpenMobileHapticsAndroidWaveformOutcome::FallbackRequired;
		Resolution.Reason = Reason;
		return Resolution;
	}

	FOpenMobileHapticsAndroidWaveformResolution Rejected(FName Reason)
	{
		FOpenMobileHapticsAndroidWaveformResolution Resolution;
		Resolution.Reason = Reason;
		return Resolution;
	}

	int32 ScaleNormalizedAmplitude(
		float SourceAmplitude,
		float RequestIntensity,
		bool bHasAmplitudeControl
	)
	{
		if (SourceAmplitude == 0.0f || RequestIntensity == 0.0f)
		{
			return 0;
		}
		if (!bHasAmplitudeControl)
		{
			return DefaultAmplitude;
		}
		return FMath::Clamp(
			FMath::RoundToInt(
				SourceAmplitude * RequestIntensity * 255.0f
			),
			1,
			255
		);
	}

	int64 MillisecondsFromMicroseconds(uint32 Microseconds)
	{
		return static_cast<int64>(Microseconds + 500U) / 1000LL;
	}

	void AppendSegment(
		FOpenMobileHapticsAndroidWaveformResolution& Resolution,
		int64 DurationMilliseconds,
		int32 Amplitude,
		bool bForceBoundary
	)
	{
		if (DurationMilliseconds <= 0)
		{
			return;
		}
		if (!bForceBoundary && !Resolution.Amplitudes.IsEmpty()
			&& Resolution.Amplitudes.Last() == Amplitude)
		{
			Resolution.TimingsMilliseconds.Last() += DurationMilliseconds;
			return;
		}
		Resolution.TimingsMilliseconds.Add(DurationMilliseconds);
		Resolution.Amplitudes.Add(Amplitude);
	}

	int32 FindActiveAmplitude(const TArray<int32>& ActiveCounts)
	{
		for (int32 Amplitude = ActiveCounts.Num() - 1;
			Amplitude > 0;
			--Amplitude)
		{
			if (ActiveCounts[Amplitude] > 0)
			{
				return Amplitude;
			}
		}
		return 0;
	}
}

FOpenMobileHapticsAndroidWaveformResolution
FOpenMobileHapticsAndroidWaveformPolicy::ResolveOverride(
	const UOpenMobileHapticAndroidPatternAsset& Asset,
	const FOpenMobileHapticCapabilities& Capabilities,
	int32 AndroidAPI,
	float RequestIntensity,
	EOpenMobileHapticFallbackPolicy FallbackPolicy
)
{
	using namespace OpenMobileHapticsAndroidWaveformPolicyPrivate;
	if (!FMath::IsFinite(RequestIntensity)
		|| RequestIntensity < 0.0f || RequestIntensity > 1.0f)
	{
		return Rejected(TEXT("InvalidIntensity"));
	}
	if (Asset.Format != EOpenMobileHapticAndroidPatternFormat::Waveform)
	{
		return Rejected(TEXT("InvalidFormat"));
	}
	TArray<FString> Errors;
	if (!Asset.Validate(Errors))
	{
		return Rejected(TEXT("InvalidPattern"));
	}
	if (AndroidAPI < Asset.GetMinimumOSVersion())
	{
		return Unavailable(FallbackPolicy, TEXT("OSVersion"));
	}
	if (Capabilities.WaveformTiming
		!= EOpenMobileHapticSupportState::Supported)
	{
		return Unavailable(FallbackPolicy, TEXT("WaveformSupport"));
	}

	const int32 Count = Asset.WaveformTimingsMilliseconds.Num();
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	if (Count > Settings->MaximumPatternEventCount)
	{
		return Rejected(TEXT("ConfiguredEventCount"));
	}
	if (Capabilities.MaximumEventCount.bKnown
		&& Count > Capabilities.MaximumEventCount.Value)
	{
		return Unavailable(FallbackPolicy, TEXT("HardwareEventCount"));
	}

	int64 TotalDurationMilliseconds = 0;
	for (const int32 Timing : Asset.WaveformTimingsMilliseconds)
	{
		if (TotalDurationMilliseconds > MAX_int64 - Timing)
		{
			return Rejected(TEXT("DurationOverflow"));
		}
		TotalDurationMilliseconds += Timing;
	}
	const int64 MaximumDurationMilliseconds = static_cast<int64>(
		FMath::RoundToDouble(
			static_cast<double>(Settings->MaximumContinuousDurationSeconds)
				* 1000.0
		)
	);
	if (TotalDurationMilliseconds > MaximumDurationMilliseconds)
	{
		return Rejected(TEXT("ConfiguredDuration"));
	}

	FOpenMobileHapticsAndroidWaveformResolution Resolution;
	Resolution.Outcome = EOpenMobileHapticsAndroidWaveformOutcome::Ready;
	Resolution.RepeatIndex = Asset.WaveformRepeatIndex;
	Resolution.TimingsMilliseconds.Reserve(Count);
	Resolution.Amplitudes.Reserve(Count);
	const bool bHasAmplitudeControl = Capabilities.AmplitudeControl
		== EOpenMobileHapticSupportState::Supported;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Resolution.TimingsMilliseconds.Add(
			Asset.WaveformTimingsMilliseconds[Index]
		);
		const int32 Amplitude = ScaleNormalizedAmplitude(
			static_cast<float>(Asset.WaveformAmplitudes[Index]) / 255.0f,
			RequestIntensity,
			bHasAmplitudeControl
		);
		Resolution.Amplitudes.Add(Amplitude);
		Resolution.bUsesDefaultAmplitude |= Amplitude == DefaultAmplitude;
	}
	return Resolution;
}

FOpenMobileHapticsAndroidWaveformResolution
FOpenMobileHapticsAndroidWaveformPolicy::ResolvePortable(
	const UOpenMobileHapticPatternAsset& Pattern,
	const FOpenMobileHapticCapabilities& Capabilities,
	float RequestIntensity,
	EOpenMobileHapticFallbackPolicy FallbackPolicy
)
{
	using namespace OpenMobileHapticsAndroidWaveformPolicyPrivate;
	if (!FMath::IsFinite(RequestIntensity)
		|| RequestIntensity < 0.0f || RequestIntensity > 1.0f)
	{
		return Rejected(TEXT("InvalidIntensity"));
	}
	if (FallbackPolicy == EOpenMobileHapticFallbackPolicy::ExactOnly
		|| Pattern.FallbackPolicy
			== EOpenMobileHapticFallbackPolicy::ExactOnly)
	{
		return Rejected(TEXT("ExactOnly"));
	}
	if (!Pattern.IsDerivedDataCurrent())
	{
		return Rejected(TEXT("InvalidPattern"));
	}
	if (Capabilities.WaveformTiming
		!= EOpenMobileHapticSupportState::Supported)
	{
		return Unavailable(FallbackPolicy, TEXT("WaveformSupport"));
	}

	const FOpenMobileHapticCookedPatternData& Cooked =
		Pattern.GetCookedPattern();
	const UOpenMobileHapticsSettings* Settings =
		GetDefault<UOpenMobileHapticsSettings>();
	if (Cooked.Events.Num() > Settings->MaximumPatternEventCount)
	{
		return Rejected(TEXT("ConfiguredEventCount"));
	}
	if (Capabilities.MaximumEventCount.bKnown
		&& Cooked.Events.Num() > Capabilities.MaximumEventCount.Value)
	{
		return Unavailable(FallbackPolicy, TEXT("HardwareEventCount"));
	}

	const int64 GranularityMilliseconds = FMath::Max<int64>(
		1,
		MillisecondsFromMicroseconds(Cooked.GranularityMicroseconds)
	);
	TArray<FAmplitudeChange> Changes;
	Changes.Reserve(Cooked.Events.Num() * 2 + 2);
	int64 MaximumEndMilliseconds = MillisecondsFromMicroseconds(
		Cooked.DurationMicroseconds
	);
	for (const FOpenMobileHapticCookedPatternEvent& Event : Cooked.Events)
	{
		const int64 StartMilliseconds = MillisecondsFromMicroseconds(
			Event.StartTimeMicroseconds
		);
		const int64 DurationMilliseconds =
			Event.Type == EOpenMobileHapticPatternEventType::Transient
				? GranularityMilliseconds
				: MillisecondsFromMicroseconds(Event.DurationMicroseconds);
		if (DurationMilliseconds <= 0
			|| StartMilliseconds > MAX_int64 - DurationMilliseconds)
		{
			return Rejected(TEXT("Timing"));
		}
		const int64 EndMilliseconds =
			StartMilliseconds + DurationMilliseconds;
		MaximumEndMilliseconds = FMath::Max(
			MaximumEndMilliseconds,
			EndMilliseconds
		);
		if (Event.Type == EOpenMobileHapticPatternEventType::Silence
			|| Event.Intensity == 0)
		{
			continue;
		}
		const int32 Amplitude = FMath::Clamp(
			FMath::RoundToInt(
				static_cast<float>(Event.Intensity)
					/ static_cast<float>(MAX_uint16)
					* RequestIntensity * 255.0f
			),
			RequestIntensity == 0.0f ? 0 : 1,
			255
		);
		if (Amplitude > 0)
		{
			Changes.Add({StartMilliseconds, Amplitude, 1});
			Changes.Add({EndMilliseconds, Amplitude, -1});
		}
	}
	if (MaximumEndMilliseconds <= 0)
	{
		return Rejected(TEXT("Timing"));
	}

	int64 RepeatStartMilliseconds = INDEX_NONE;
	if (Pattern.Loop.bLoop)
	{
		RepeatStartMilliseconds = static_cast<int64>(FMath::RoundToDouble(
			Pattern.Loop.RepeatStartTimeSeconds * 1000.0
		));
		if (RepeatStartMilliseconds < 0
			|| RepeatStartMilliseconds >= MaximumEndMilliseconds)
		{
			return Rejected(TEXT("RepeatStart"));
		}
		Changes.Add({RepeatStartMilliseconds, 0, 0});
	}
	Changes.Add({MaximumEndMilliseconds, 0, 0});
	Changes.Sort([](const FAmplitudeChange& Left, const FAmplitudeChange& Right)
	{
		return Left.TimeMilliseconds < Right.TimeMilliseconds;
	});

	FOpenMobileHapticsAndroidWaveformResolution Resolution;
	Resolution.Outcome = EOpenMobileHapticsAndroidWaveformOutcome::Ready;
	TArray<int32> ActiveCounts;
	ActiveCounts.SetNumZeroed(256);
	int64 PreviousTimeMilliseconds = 0;
	int32 ActiveAmplitude = 0;
	for (int32 ChangeIndex = 0; ChangeIndex < Changes.Num();)
	{
		const int64 ChangeTime = Changes[ChangeIndex].TimeMilliseconds;
		if (ChangeTime > PreviousTimeMilliseconds)
		{
			const bool bRepeatBoundary =
				PreviousTimeMilliseconds == RepeatStartMilliseconds;
			if (bRepeatBoundary)
			{
				Resolution.RepeatIndex = Resolution.Amplitudes.Num();
			}
			const int32 NativeAmplitude = ActiveAmplitude == 0
				? 0
				: Capabilities.AmplitudeControl
					== EOpenMobileHapticSupportState::Supported
						? ActiveAmplitude
						: DefaultAmplitude;
			AppendSegment(
				Resolution,
				ChangeTime - PreviousTimeMilliseconds,
				NativeAmplitude,
				bRepeatBoundary
			);
			Resolution.bUsesDefaultAmplitude |=
				NativeAmplitude == DefaultAmplitude;
			PreviousTimeMilliseconds = ChangeTime;
		}
		while (ChangeIndex < Changes.Num()
			&& Changes[ChangeIndex].TimeMilliseconds == ChangeTime)
		{
			const FAmplitudeChange& Change = Changes[ChangeIndex++];
			if (Change.Amplitude > 0)
			{
				ActiveCounts[Change.Amplitude] += Change.Delta;
			}
		}
		ActiveAmplitude = FindActiveAmplitude(ActiveCounts);
	}

	if (Resolution.TimingsMilliseconds.IsEmpty()
		|| Resolution.TimingsMilliseconds.Num() > MaximumNativeSegmentCount)
	{
		return Rejected(TEXT("NativeSegmentCount"));
	}
	if (!Pattern.Loop.bLoop)
	{
		Resolution.RepeatIndex = INDEX_NONE;
		return Resolution;
	}
	if (Resolution.RepeatIndex == INDEX_NONE)
	{
		return Rejected(TEXT("RepeatStart"));
	}
	if (Pattern.Loop.RepeatCount == 0)
	{
		return Resolution;
	}

	const TArray<int64> BaseTimings = Resolution.TimingsMilliseconds;
	const TArray<int32> BaseAmplitudes = Resolution.Amplitudes;
	const int32 BaseRepeatIndex = Resolution.RepeatIndex;
	Resolution.RepeatIndex = INDEX_NONE;
	for (int32 Repeat = 0; Repeat < Pattern.Loop.RepeatCount; ++Repeat)
	{
		for (int32 Index = BaseRepeatIndex; Index < BaseTimings.Num(); ++Index)
		{
			AppendSegment(
				Resolution,
				BaseTimings[Index],
				BaseAmplitudes[Index],
				false
			);
			if (Resolution.TimingsMilliseconds.Num()
				> MaximumNativeSegmentCount)
			{
				return Rejected(TEXT("NativeSegmentCount"));
			}
		}
	}
	return Resolution;
}
