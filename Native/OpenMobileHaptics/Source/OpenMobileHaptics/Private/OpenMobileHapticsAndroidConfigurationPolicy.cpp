#include "OpenMobileHapticsAndroidConfigurationPolicy.h"

void FOpenMobileHapticsAndroidConfigurationPolicy::ApplyCapabilityMask(
	bool bCustomVibrationEnabled,
	FOpenMobileHapticCapabilities& Capabilities
)
{
	if (bCustomVibrationEnabled)
	{
		return;
	}

	const EOpenMobileHapticSupportState Unsupported =
		EOpenMobileHapticSupportState::Unsupported;
	Capabilities.BasicVibration = Unsupported;
	Capabilities.RichHaptics = Unsupported;
	Capabilities.AmplitudeControl = Unsupported;
	Capabilities.PredefinedEffects = Unsupported;
	Capabilities.WaveformTiming = Unsupported;
	Capabilities.Looping = Unsupported;
	Capabilities.Primitives = Unsupported;
	Capabilities.Envelopes = Unsupported;
	Capabilities.FrequencyControl = Unsupported;
	Capabilities.TransientEvents = Unsupported;
	Capabilities.ContinuousEvents = Unsupported;
	for (FOpenMobileHapticNamedSupport& Entry : Capabilities.PresetSupport)
	{
		Entry.Support = Unsupported;
	}
	for (FOpenMobileHapticNamedSupport& Entry : Capabilities.PrimitiveSupport)
	{
		Entry.Support = Unsupported;
	}
	Capabilities.MaximumEventCount = {};
	Capabilities.MaximumControlPointCount = {};
	Capabilities.MaximumDurationSeconds = {};
	Capabilities.MinimumTimingGranularitySeconds = {};
	Capabilities.MaximumControlPointDurationSeconds = {};
	Capabilities.FrequencyRange = {};
	Capabilities.Availability =
		Capabilities.SemanticEffects == EOpenMobileHapticSupportState::Supported
			? EOpenMobileHapticAvailability::SemanticFeedback
			: EOpenMobileHapticAvailability::DisabledByPolicy;
	Capabilities.Detail = TEXT(
		"Custom Android vibration is not packaged; semantic view feedback remains available."
	);
}
