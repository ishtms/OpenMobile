#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsTypes.h"

struct FOpenMobileAdsCooldownDecision
{
	bool bPlacementActive = false;
	bool bGlobalActive = false;
	FDateTime NextEligibleAt;

	/** Keeps placement and fullscreen cooldowns independent while exposing one combined answer. */
	bool IsActive() const
	{
		return bPlacementActive || bGlobalActive;
	}
};

class FOpenMobileAdsCooldownTracker
{
public:
	/** Records placement time for every impression and global time only for fullscreen formats. */
	void RecordImpression(
		FName Placement,
		EOpenMobileAdFormat Format,
		double MonotonicSeconds
	);
	/** Resolves placement and global remaining time against monotonic history and reports a wall-clock answer. */
	FOpenMobileAdsCooldownDecision Evaluate(
		FName Placement,
		EOpenMobileAdFormat Format,
		double PlacementCooldownSeconds,
		double GlobalCooldownSeconds,
		FDateTime UtcNow,
		double MonotonicSeconds
	) const;
	/** Clears in-memory pacing history when the owning Game Instance shuts down. */
	void Reset();

private:
	/** Keeps banner and native impressions from extending the global fullscreen cooldown. */
	static bool IsFullscreen(EOpenMobileAdFormat Format);

	TMap<FName, double> LastImpressionByPlacement;
	double LastFullscreenImpression = 0.0;
	bool bHasFullscreenImpression = false;
};
