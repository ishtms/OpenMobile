#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsTypes.h"

struct FOpenMobileAdsCooldownDecision
{
	bool bPlacementActive = false;
	bool bGlobalActive = false;
	FDateTime NextEligibleAt;

	bool IsActive() const
	{
		return bPlacementActive || bGlobalActive;
	}
};

class FOpenMobileAdsCooldownTracker
{
public:
	void RecordImpression(
		FName Placement,
		EOpenMobileAdFormat Format,
		double MonotonicSeconds
	);
	FOpenMobileAdsCooldownDecision Evaluate(
		FName Placement,
		EOpenMobileAdFormat Format,
		double PlacementCooldownSeconds,
		double GlobalCooldownSeconds,
		FDateTime UtcNow,
		double MonotonicSeconds
	) const;
	void Reset();

private:
	static bool IsFullscreen(EOpenMobileAdFormat Format);

	TMap<FName, double> LastImpressionByPlacement;
	double LastFullscreenImpression = 0.0;
	bool bHasFullscreenImpression = false;
};
