#include "OpenMobileHapticsOverlapPolicy.h"

namespace OpenMobileHapticsOverlapPolicyPrivate
{
	/** Converts priority to explicit ordering instead of relying on public enum ordinal values. */
	int32 PriorityValue(EOpenMobileHapticChannelPriority Priority)
	{
		return static_cast<int32>(Priority);
	}

	/** Collects conflicts on the requested channel only, unrelated channel owners must survive replacement. */
	void GatherSameChannelRequestIds(
		FName Channel,
		const TArray<FOpenMobileHapticsOverlapConflict>& Conflicts,
		TArray<uint64>& OutRequestIds
	)
	{
		for (const FOpenMobileHapticsOverlapConflict& Conflict : Conflicts)
		{
			if (Conflict.Channel == Channel && Conflict.RequestId != 0)
			{
				OutRequestIds.AddUnique(Conflict.RequestId);
			}
		}
		OutRequestIds.Sort();
	}
}

FOpenMobileHapticsOverlapResolution FOpenMobileHapticsOverlapPolicy::Resolve(
	const FOpenMobileHapticsOverlapRequest& Request,
	const TArray<FOpenMobileHapticsOverlapConflict>& Conflicts
)
{
	FOpenMobileHapticsOverlapResolution Result;
	Result.ResolvedPolicy = Request.Policy;

	bool bHasSameChannelConflict = false;
	for (const FOpenMobileHapticsOverlapConflict& Conflict : Conflicts)
	{
		if (Conflict.Channel == Request.Channel)
		{
			bHasSameChannelConflict = true;
			break;
		}
	}
	if (!bHasSameChannelConflict)
	{
		return Result;
	}

	if (Result.ResolvedPolicy ==
		EOpenMobileHapticOverlapPolicy::MixWhenSupported)
	{
		if (Request.Mixing == EOpenMobileHapticSupportState::Supported)
		{
			return Result;
		}
		Result.bUsedMixFallback = true;
		Result.ResolvedPolicy = Request.UnsupportedMixFallback ==
			EOpenMobileHapticOverlapPolicy::MixWhenSupported
			? EOpenMobileHapticOverlapPolicy::Replace
			: Request.UnsupportedMixFallback;
	}

	switch (Result.ResolvedPolicy)
	{
	case EOpenMobileHapticOverlapPolicy::Ignore:
		Result.Outcome = EOpenMobileHapticsOverlapOutcome::Suppress;
		break;
	case EOpenMobileHapticOverlapPolicy::Queue:
		Result.Outcome = EOpenMobileHapticsOverlapOutcome::Queue;
		break;
	case EOpenMobileHapticOverlapPolicy::InterruptLowerPriority:
		for (const FOpenMobileHapticsOverlapConflict& Conflict : Conflicts)
		{
			if (Conflict.Channel == Request.Channel
				&& OpenMobileHapticsOverlapPolicyPrivate::PriorityValue(
					Conflict.Priority
				) >= OpenMobileHapticsOverlapPolicyPrivate::PriorityValue(
					Request.Priority
				))
			{
				Result.Outcome = EOpenMobileHapticsOverlapOutcome::Suppress;
				return Result;
			}
		}
		Result.Outcome = EOpenMobileHapticsOverlapOutcome::InterruptThenSubmit;
		OpenMobileHapticsOverlapPolicyPrivate::GatherSameChannelRequestIds(
			Request.Channel,
			Conflicts,
			Result.TerminalRequestIds
		);
		break;
	case EOpenMobileHapticOverlapPolicy::Replace:
	case EOpenMobileHapticOverlapPolicy::MixWhenSupported:
	default:
		Result.Outcome = EOpenMobileHapticsOverlapOutcome::InterruptThenSubmit;
		OpenMobileHapticsOverlapPolicyPrivate::GatherSameChannelRequestIds(
			Request.Channel,
			Conflicts,
			Result.TerminalRequestIds
		);
		break;
	}
	return Result;
}

uint64 FOpenMobileHapticsOverlapPolicy::SelectNext(
	FName Channel,
	const TArray<FOpenMobileHapticsOverlapQueueEntry>& Queue
)
{
	const FOpenMobileHapticsOverlapQueueEntry* Selected = nullptr;
	for (const FOpenMobileHapticsOverlapQueueEntry& Entry : Queue)
	{
		if (Entry.Channel != Channel || Entry.RequestId == 0)
		{
			continue;
		}
		if (Selected == nullptr
			|| OpenMobileHapticsOverlapPolicyPrivate::PriorityValue(
				Entry.Priority
			) > OpenMobileHapticsOverlapPolicyPrivate::PriorityValue(
				Selected->Priority
			)
			|| (Entry.Priority == Selected->Priority
				&& Entry.RequestId < Selected->RequestId))
		{
			Selected = &Entry;
		}
	}
	return Selected ? Selected->RequestId : 0;
}

bool FOpenMobileHapticsOverlapPolicy::IsExpired(
	double EnqueuedAtSeconds,
	double NowSeconds,
	double MaximumAgeSeconds
)
{
	if (!FMath::IsFinite(EnqueuedAtSeconds)
		|| !FMath::IsFinite(NowSeconds)
		|| !FMath::IsFinite(MaximumAgeSeconds)
		|| MaximumAgeSeconds < 0.0
		|| NowSeconds < EnqueuedAtSeconds)
	{
		return false;
	}
	return NowSeconds - EnqueuedAtSeconds > MaximumAgeSeconds;
}
