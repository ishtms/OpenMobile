#include "OpenMobileHapticsEnvelopePolicy.h"

#include "OpenMobileHapticsBudgetPolicy.h"
#include "OpenMobileHapticsSettings.h"

namespace OpenMobileHapticsEnvelopePolicyPrivate
{
	FOpenMobileHapticsEnvelopeResolution Unavailable(
		EOpenMobileHapticFallbackPolicy Policy,
		FName Reason
	)
	{
		FOpenMobileHapticsEnvelopeResolution Resolution;
		Resolution.Outcome =
			Policy == EOpenMobileHapticFallbackPolicy::ExactOnly
				? EOpenMobileHapticsEnvelopeOutcome::Rejected
				: EOpenMobileHapticsEnvelopeOutcome::FallbackRequired;
		Resolution.Reason = Reason;
		return Resolution;
	}

	FOpenMobileHapticsEnvelopeResolution Invalid(FName Reason)
	{
		FOpenMobileHapticsEnvelopeResolution Resolution;
		Resolution.Outcome = EOpenMobileHapticsEnvelopeOutcome::Rejected;
		Resolution.Reason = Reason;
		return Resolution;
	}

	bool HasUsableEnvelopeLimits(
		const FOpenMobileHapticCapabilities& Capabilities
	)
	{
		return Capabilities.MaximumControlPointCount.bKnown
			&& Capabilities.MaximumControlPointCount.Value > 0
			&& Capabilities.MaximumDurationSeconds.bKnown
			&& FMath::IsFinite(Capabilities.MaximumDurationSeconds.Seconds)
			&& Capabilities.MaximumDurationSeconds.Seconds > 0.0
			&& Capabilities.MinimumTimingGranularitySeconds.bKnown
			&& FMath::IsFinite(
				Capabilities.MinimumTimingGranularitySeconds.Seconds
			)
			&& Capabilities.MinimumTimingGranularitySeconds.Seconds > 0.0
			&& Capabilities.MaximumControlPointDurationSeconds.bKnown
			&& FMath::IsFinite(
				Capabilities.MaximumControlPointDurationSeconds.Seconds
			)
			&& Capabilities.MaximumControlPointDurationSeconds.Seconds
				>= Capabilities.MinimumTimingGranularitySeconds.Seconds;
	}

	bool HasUsableFrequencyRange(
		const FOpenMobileHapticCapabilities& Capabilities
	)
	{
		return Capabilities.FrequencyRange.bKnown
			&& FMath::IsFinite(Capabilities.FrequencyRange.MinimumHertz)
			&& FMath::IsFinite(Capabilities.FrequencyRange.MaximumHertz)
			&& Capabilities.FrequencyRange.MinimumHertz > 0.0f
			&& Capabilities.FrequencyRange.MaximumHertz
				>= Capabilities.FrequencyRange.MinimumHertz;
	}
}

FOpenMobileHapticsEnvelopeResolution
FOpenMobileHapticsEnvelopePolicy::Resolve(
	const UOpenMobileHapticAndroidPatternAsset& Asset,
	const FOpenMobileHapticCapabilities& Capabilities,
	int32 AndroidAPI,
	float RequestIntensity,
	EOpenMobileHapticFallbackPolicy FallbackPolicy
)
{
	using namespace OpenMobileHapticsEnvelopePolicyPrivate;
	if (!FMath::IsFinite(RequestIntensity)
		|| RequestIntensity < 0.0f || RequestIntensity > 1.0f)
	{
		return Invalid(TEXT("InvalidIntensity"));
	}
	if (Asset.Format != EOpenMobileHapticAndroidPatternFormat::BasicEnvelope
		&& Asset.Format
			!= EOpenMobileHapticAndroidPatternFormat::WaveformEnvelope)
	{
		return Invalid(TEXT("InvalidFormat"));
	}
	TArray<FString> Errors;
	if (!Asset.Validate(Errors))
	{
		return Invalid(TEXT("InvalidPattern"));
	}
	if (AndroidAPI < Asset.GetMinimumOSVersion())
	{
		return Unavailable(FallbackPolicy, TEXT("OSVersion"));
	}
	if (Capabilities.Envelopes != EOpenMobileHapticSupportState::Supported)
	{
		return Unavailable(FallbackPolicy, TEXT("EnvelopeSupport"));
	}
	if (!HasUsableEnvelopeLimits(Capabilities))
	{
		return Unavailable(FallbackPolicy, TEXT("EnvelopeLimits"));
	}
	if (Asset.EnvelopePoints.Num()
		> FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternEvents(
			GetDefault<UOpenMobileHapticsSettings>()->MaximumPatternEventCount
		))
	{
		return Invalid(TEXT("ConfiguredPointCount"));
	}
	if (Asset.EnvelopePoints.Num()
		> Capabilities.MaximumControlPointCount.Value)
	{
		return Unavailable(FallbackPolicy, TEXT("PointCount"));
	}

	const bool bDirectFrequency = Asset.Format
		== EOpenMobileHapticAndroidPatternFormat::WaveformEnvelope;
	if (bDirectFrequency
		&& (Capabilities.FrequencyControl
				!= EOpenMobileHapticSupportState::Supported
			|| !HasUsableFrequencyRange(Capabilities)))
	{
		return Unavailable(FallbackPolicy, TEXT("FrequencySupport"));
	}

	const int64 MinimumDurationMillis = FMath::CeilToInt64(
		Capabilities.MinimumTimingGranularitySeconds.Seconds * 1000.0
	);
	const int64 MaximumControlPointDurationMillis = FMath::FloorToInt64(
		Capabilities.MaximumControlPointDurationSeconds.Seconds * 1000.0
	);
	const int64 MaximumDurationMillis = FMath::FloorToInt64(
		Capabilities.MaximumDurationSeconds.Seconds * 1000.0
	);
	if (MinimumDurationMillis <= 0
		|| MaximumControlPointDurationMillis < MinimumDurationMillis
		|| MaximumDurationMillis < MinimumDurationMillis)
	{
		return Unavailable(FallbackPolicy, TEXT("EnvelopeLimits"));
	}

	FOpenMobileHapticsEnvelopeResolution Resolution;
	Resolution.Outcome = EOpenMobileHapticsEnvelopeOutcome::Ready;
	Resolution.Format = Asset.Format;
	Resolution.Reason = TEXT("Supported");
	Resolution.Amplitudes.Reserve(Asset.EnvelopePoints.Num());
	Resolution.ControlValues.Reserve(Asset.EnvelopePoints.Num());
	Resolution.DurationsMilliseconds.Reserve(Asset.EnvelopePoints.Num());
	int64 PreviousTimeMilliseconds = 0;
	for (const FOpenMobileHapticAndroidEnvelopePoint& Point :
		Asset.EnvelopePoints)
	{
		const double TimeMilliseconds =
			static_cast<double>(Point.TimeSeconds) * 1000.0;
		if (!FMath::IsFinite(TimeMilliseconds)
			|| TimeMilliseconds > static_cast<double>(MAX_int64))
		{
			return Invalid(TEXT("Time"));
		}
		const int64 Time = FMath::RoundToInt64(TimeMilliseconds);
		const int64 SegmentDuration = Time - PreviousTimeMilliseconds;
		if (SegmentDuration < MinimumDurationMillis
			|| SegmentDuration > MaximumControlPointDurationMillis)
		{
			return Unavailable(FallbackPolicy, TEXT("SegmentDuration"));
		}
		if (Time > MaximumDurationMillis)
		{
			return Unavailable(FallbackPolicy, TEXT("TotalDuration"));
		}
		if (bDirectFrequency
			&& (Point.FrequencyHz < Capabilities.FrequencyRange.MinimumHertz
				|| Point.FrequencyHz
					> Capabilities.FrequencyRange.MaximumHertz))
		{
			return Unavailable(FallbackPolicy, TEXT("FrequencyRange"));
		}

		Resolution.Amplitudes.Add(Point.Amplitude * RequestIntensity);
		Resolution.ControlValues.Add(
			bDirectFrequency ? Point.FrequencyHz : Point.Sharpness
		);
		Resolution.DurationsMilliseconds.Add(SegmentDuration);
		PreviousTimeMilliseconds = Time;
	}
	if (!bDirectFrequency)
	{
		Resolution.Amplitudes.Last() = 0.0f;
	}
	return Resolution;
}
