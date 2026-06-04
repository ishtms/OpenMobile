#include "OpenMobileHapticsIntensityPolicy.h"

float FOpenMobileHapticsIntensityPolicy::Scale(
	float BaseIntensity,
	float MasterScale,
	float CategoryScale,
	float EffectScale,
	float RequestScale,
	float ProjectScale
)
{
	const float Values[] = {
		BaseIntensity,
		MasterScale,
		CategoryScale,
		EffectScale,
		RequestScale,
		ProjectScale
	};
	double Scaled = 1.0;
	for (const float Value : Values)
	{
		if (!FMath::IsFinite(Value))
		{
			return 0.0f;
		}
		Scaled *= static_cast<double>(Value);
	}
	return FMath::Clamp(static_cast<float>(Scaled), 0.0f, 1.0f);
}

FOpenMobileHapticsIntensityResolution
FOpenMobileHapticsIntensityPolicy::ResolveBasicVibration(
	float Intensity,
	EOpenMobileHapticSupportState AmplitudeControl,
	EOpenMobileHapticFallbackPolicy FallbackPolicy
)
{
	FOpenMobileHapticsIntensityResolution Resolution;
	if (!FMath::IsFinite(Intensity) || Intensity < 0.0f || Intensity > 1.0f)
	{
		return Resolution;
	}
	Resolution.ResolvedIntensity = Intensity;
	if (Intensity == 0.0f)
	{
		Resolution.Outcome = EOpenMobileHapticsIntensityOutcome::Suppressed;
		return Resolution;
	}
	if (AmplitudeControl == EOpenMobileHapticSupportState::Supported)
	{
		Resolution.Outcome = EOpenMobileHapticsIntensityOutcome::Accepted;
		return Resolution;
	}
	if (Intensity == 1.0f)
	{
		Resolution.Outcome = EOpenMobileHapticsIntensityOutcome::Accepted;
		Resolution.bNativeIntensityKnown = true;
		Resolution.NativeIntensity = 1.0f;
		return Resolution;
	}
	if (FallbackPolicy == EOpenMobileHapticFallbackPolicy::NoEffectAllowed)
	{
		Resolution.Outcome = EOpenMobileHapticsIntensityOutcome::Suppressed;
		return Resolution;
	}
	if (FallbackPolicy == EOpenMobileHapticFallbackPolicy::ExactOnly)
	{
		return Resolution;
	}
	Resolution.Outcome =
		EOpenMobileHapticsIntensityOutcome::DefaultAmplitudeFallback;
	Resolution.bNativeIntensityKnown = true;
	Resolution.NativeIntensity = 1.0f;
	Resolution.bNativeClamped = true;
	return Resolution;
}
