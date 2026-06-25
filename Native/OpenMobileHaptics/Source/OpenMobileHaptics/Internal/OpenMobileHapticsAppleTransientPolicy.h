#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticPatternAsset.h"

enum class EOpenMobileHapticsAppleTransientOutcome : uint8
{
	Ready,
	Suppressed,
	FallbackRequired,
	Invalid
};

struct FOpenMobileHapticsAppleTransientPattern
{
	TArray<double> StartTimesSeconds;
	TArray<float> Intensities;
	TArray<float> Sharpnesses;
	bool bHasInitialDynamicParameters = false;
	FOpenMobileHapticDynamicParameterUpdate InitialDynamicParameters;

	bool IsValid() const
	{
		return !StartTimesSeconds.IsEmpty()
			&& StartTimesSeconds.Num() == Intensities.Num()
			&& StartTimesSeconds.Num() == Sharpnesses.Num();
	}
};

struct FOpenMobileHapticsAppleTransientResolution
{
	EOpenMobileHapticsAppleTransientOutcome Outcome =
		EOpenMobileHapticsAppleTransientOutcome::Invalid;
	FOpenMobileHapticsAppleTransientPattern Pattern;
	FName Reason;
};

class FOpenMobileHapticsAppleTransientPolicy final
{
public:
	static FOpenMobileHapticsAppleTransientResolution Resolve(
		const FOpenMobileHapticCookedPatternData& Pattern,
		const FOpenMobileHapticCapabilities& Capabilities,
		float RequestIntensity
	);
};
