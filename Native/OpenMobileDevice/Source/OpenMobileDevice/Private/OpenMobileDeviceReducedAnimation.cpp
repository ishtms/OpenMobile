#include "OpenMobileDeviceReducedAnimation.h"

namespace
{
	bool IsValidAnimationScale(float Scale)
	{
		return FMath::IsFinite(Scale) && Scale >= 0.0f;
	}
}

FOpenMobileAccessibilitySnapshot
FOpenMobileDeviceReducedAnimation::FromAndroidAnimationScales(
	float AnimatorScale,
	float TransitionScale,
	float WindowScale
)
{
	FOpenMobileAccessibilitySnapshot Snapshot;
	if (!IsValidAnimationScale(AnimatorScale)
		|| !IsValidAnimationScale(TransitionScale)
		|| !IsValidAnimationScale(WindowScale))
	{
		return Snapshot;
	}
	Snapshot.bReducedAnimationPreferred =
		FOpenMobileDeviceOptionalBool::MakeAvailable(
			FMath::Min3(AnimatorScale, TransitionScale, WindowScale) < 1.0f
		);
	Snapshot.ReducedAnimationPlatformDetail =
		FOpenMobileDeviceOptionalString::MakeAvailable(FString::Printf(
			TEXT("Android animation scales: animator=%.3f, transition=%.3f, window=%.3f"),
			AnimatorScale,
			TransitionScale,
			WindowScale
		));
	return Snapshot;
}

FOpenMobileAccessibilitySnapshot
FOpenMobileDeviceReducedAnimation::FromIOSReduceMotion(bool bEnabled)
{
	FOpenMobileAccessibilitySnapshot Snapshot;
	Snapshot.bReducedAnimationPreferred =
		FOpenMobileDeviceOptionalBool::MakeAvailable(bEnabled);
	Snapshot.ReducedAnimationPlatformDetail =
		FOpenMobileDeviceOptionalString::MakeAvailable(
			bEnabled
				? TEXT("iOS Reduce Motion: enabled")
				: TEXT("iOS Reduce Motion: disabled")
		);
	return Snapshot;
}
