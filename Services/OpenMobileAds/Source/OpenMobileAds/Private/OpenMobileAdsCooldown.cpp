#include "OpenMobileAdsCooldown.h"

bool FOpenMobileAdsCooldownTracker::IsFullscreen(EOpenMobileAdFormat Format)
{
	switch (Format)
	{
	case EOpenMobileAdFormat::Interstitial:
	case EOpenMobileAdFormat::Rewarded:
	case EOpenMobileAdFormat::RewardedInterstitial:
	case EOpenMobileAdFormat::AppOpen:
		return true;
	default:
		return false;
	}
}

void FOpenMobileAdsCooldownTracker::RecordImpression(
	FName Placement,
	EOpenMobileAdFormat Format,
	double MonotonicSeconds
)
{
	if (
		Placement.IsNone()
		|| !IsFullscreen(Format)
		|| !FMath::IsFinite(MonotonicSeconds)
	)
	{
		return;
	}
	LastImpressionByPlacement.Add(Placement, MonotonicSeconds);
	LastFullscreenImpression = MonotonicSeconds;
	bHasFullscreenImpression = true;
}

FOpenMobileAdsCooldownDecision FOpenMobileAdsCooldownTracker::Evaluate(
	FName Placement,
	EOpenMobileAdFormat Format,
	double PlacementCooldownSeconds,
	double GlobalCooldownSeconds,
	FDateTime UtcNow,
	double MonotonicSeconds
) const
{
	FOpenMobileAdsCooldownDecision Decision;
	if (
		Placement.IsNone()
		|| !IsFullscreen(Format)
		|| !FMath::IsFinite(MonotonicSeconds)
	)
	{
		return Decision;
	}
	double RemainingSeconds = 0.0;
	if (
		PlacementCooldownSeconds > 0.0
		&& FMath::IsFinite(PlacementCooldownSeconds)
	)
	{
		if (const double* LastImpression =
			LastImpressionByPlacement.Find(Placement))
		{
			const double PlacementRemaining = *LastImpression
				+ PlacementCooldownSeconds - MonotonicSeconds;
			if (PlacementRemaining > 0.0 && FMath::IsFinite(PlacementRemaining))
			{
				Decision.bPlacementActive = true;
				RemainingSeconds = PlacementRemaining;
			}
		}
	}
	if (
		bHasFullscreenImpression
		&& GlobalCooldownSeconds > 0.0
		&& FMath::IsFinite(GlobalCooldownSeconds)
	)
	{
		const double GlobalRemaining = LastFullscreenImpression
			+ GlobalCooldownSeconds - MonotonicSeconds;
		if (GlobalRemaining > 0.0 && FMath::IsFinite(GlobalRemaining))
		{
			Decision.bGlobalActive = true;
			RemainingSeconds = FMath::Max(RemainingSeconds, GlobalRemaining);
		}
	}
	if (Decision.IsActive())
	{
		const double MaximumFutureSeconds =
			(FDateTime::MaxValue() - UtcNow).GetTotalSeconds();
		Decision.NextEligibleAt = RemainingSeconds >= MaximumFutureSeconds
			? FDateTime::MaxValue()
			: UtcNow + FTimespan::FromSeconds(RemainingSeconds);
	}
	return Decision;
}

void FOpenMobileAdsCooldownTracker::Reset()
{
	LastImpressionByPlacement.Reset();
	LastFullscreenImpression = 0.0;
	bHasFullscreenImpression = false;
}
