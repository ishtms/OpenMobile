#include "OpenMobileHapticsAppleTransientPolicy.h"

namespace OpenMobileHapticsAppleTransientPolicyPrivate
{
	/** Builds a transient resolution and keeps failure output empty for safe caller checks. */
	FOpenMobileHapticsAppleTransientResolution MakeResolution(
		EOpenMobileHapticsAppleTransientOutcome Outcome,
		FName Reason
	)
	{
		FOpenMobileHapticsAppleTransientResolution Resolution;
		Resolution.Outcome = Outcome;
		Resolution.Reason = Reason;
		return Resolution;
	}

	/** Restores cooked intensity and sharpness using the asset's normalized integer encoding. */
	float DecodeNormalized(uint16 Value)
	{
		return static_cast<float>(Value) / static_cast<float>(MAX_uint16);
	}
}

FOpenMobileHapticsAppleTransientResolution
FOpenMobileHapticsAppleTransientPolicy::Resolve(
	const FOpenMobileHapticCookedPatternData& Pattern,
	const FOpenMobileHapticCapabilities& Capabilities,
	float RequestIntensity
)
{
	using namespace OpenMobileHapticsAppleTransientPolicyPrivate;
	if (!FMath::IsFinite(RequestIntensity)
		|| RequestIntensity < 0.0f
		|| RequestIntensity > 1.0f)
	{
		return MakeResolution(
			EOpenMobileHapticsAppleTransientOutcome::Invalid,
			TEXT("InvalidIntensity")
		);
	}
	if (Pattern.DataFormatVersion
		!= FOpenMobileHapticCookedPatternData::CurrentFormatVersion
		|| Pattern.GranularityMicroseconds == 0
		|| Pattern.Events.IsEmpty())
	{
		return MakeResolution(
			EOpenMobileHapticsAppleTransientOutcome::Invalid,
			TEXT("InvalidCookedPattern")
		);
	}
	if (RequestIntensity == 0.0f)
	{
		return MakeResolution(
			EOpenMobileHapticsAppleTransientOutcome::Suppressed,
			TEXT("ZeroIntensity")
		);
	}
	if (Capabilities.RichHaptics
		!= EOpenMobileHapticSupportState::Supported
		|| Capabilities.TransientEvents
			!= EOpenMobileHapticSupportState::Supported)
	{
		return MakeResolution(
			EOpenMobileHapticsAppleTransientOutcome::FallbackRequired,
			TEXT("UnsupportedHardware")
		);
	}

	FOpenMobileHapticsAppleTransientResolution Resolution;
	Resolution.Outcome = EOpenMobileHapticsAppleTransientOutcome::Ready;
	Resolution.Reason = TEXT("Ready");
	Resolution.Pattern.StartTimesSeconds.Reserve(Pattern.Events.Num());
	Resolution.Pattern.Intensities.Reserve(Pattern.Events.Num());
	Resolution.Pattern.Sharpnesses.Reserve(Pattern.Events.Num());
	uint32 PreviousStartMicroseconds = 0;
	bool bHasPreviousStart = false;
	bool bHasAudibleEvent = false;
	for (const FOpenMobileHapticCookedPatternEvent& Event : Pattern.Events)
	{
		if (Event.StartTimeMicroseconds > Pattern.DurationMicroseconds
			|| Event.DurationMicroseconds
				> Pattern.DurationMicroseconds - Event.StartTimeMicroseconds)
		{
			return MakeResolution(
				EOpenMobileHapticsAppleTransientOutcome::Invalid,
				TEXT("InvalidTiming")
			);
		}
		if (static_cast<uint8>(Event.Type)
			> static_cast<uint8>(EOpenMobileHapticPatternEventType::Silence))
		{
			return MakeResolution(
				EOpenMobileHapticsAppleTransientOutcome::Invalid,
				TEXT("InvalidEventType")
			);
		}
		if (bHasPreviousStart
			&& Event.StartTimeMicroseconds < PreviousStartMicroseconds)
		{
			return MakeResolution(
				EOpenMobileHapticsAppleTransientOutcome::Invalid,
				TEXT("UnsortedEvents")
			);
		}
		bHasPreviousStart = true;
		PreviousStartMicroseconds = Event.StartTimeMicroseconds;
		if (Event.Type == EOpenMobileHapticPatternEventType::Silence)
		{
			continue;
		}
		if (Event.Type != EOpenMobileHapticPatternEventType::Transient)
		{
			return MakeResolution(
				EOpenMobileHapticsAppleTransientOutcome::FallbackRequired,
				TEXT("ContinuousEvent")
			);
		}
		if (Event.DurationMicroseconds != 0)
		{
			return MakeResolution(
				EOpenMobileHapticsAppleTransientOutcome::Invalid,
				TEXT("TransientDuration")
			);
		}
		Resolution.Pattern.StartTimesSeconds.Add(
			static_cast<double>(Event.StartTimeMicroseconds) / 1000000.0
		);
		const float Intensity = DecodeNormalized(Event.Intensity)
			* RequestIntensity;
		Resolution.Pattern.Intensities.Add(Intensity);
		Resolution.Pattern.Sharpnesses.Add(
			DecodeNormalized(Event.Sharpness)
		);
		bHasAudibleEvent |= Intensity > 0.0f;
	}

	if (Capabilities.MaximumEventCount.bKnown
		&& Resolution.Pattern.StartTimesSeconds.Num()
			> Capabilities.MaximumEventCount.Value)
	{
		return MakeResolution(
			EOpenMobileHapticsAppleTransientOutcome::FallbackRequired,
			TEXT("EventLimit")
		);
	}
	if (!Resolution.Pattern.IsValid() || !bHasAudibleEvent)
	{
		return MakeResolution(
			EOpenMobileHapticsAppleTransientOutcome::Suppressed,
			TEXT("NoActiveTransient")
		);
	}
	return Resolution;
}
