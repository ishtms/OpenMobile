#include "OpenMobileHapticsChannelPolicy.h"

#include "OpenMobileHapticsBudgetPolicy.h"

namespace OpenMobileHapticsChannelPolicyPrivate
{
	int32 PriorityValue(EOpenMobileHapticChannelPriority Priority)
	{
		return static_cast<int32>(Priority);
	}
}

FOpenMobileHapticsResolvedChannel FOpenMobileHapticsChannelPolicy::Resolve(
	FName Channel,
	EOpenMobileHapticChannelPriority RequestPriority,
	const TArray<FOpenMobileHapticChannelSettings>& Channels,
	int32 MaximumActiveHandles,
	int32 MaximumQueueDepthPerChannel
)
{
	FOpenMobileHapticsResolvedChannel Result;
	Result.EffectivePriority = RequestPriority;
	Result.MaximumActiveHandles = FMath::Max(0, MaximumActiveHandles);
	Result.MaximumQueueDepth = FMath::Max(0, MaximumQueueDepthPerChannel);

	const FOpenMobileHapticChannelSettings* ConfiguredChannel =
		Channels.FindByPredicate(
			[Channel](const FOpenMobileHapticChannelSettings& Candidate)
			{
				return Candidate.Name == Channel;
			}
		);
	if (ConfiguredChannel == nullptr)
	{
		return Result;
	}

	Result.bConfigured = true;
	Result.UnsupportedMixFallbackPolicy =
		ConfiguredChannel->UnsupportedMixFallbackPolicy;
	if (OpenMobileHapticsChannelPolicyPrivate::PriorityValue(
		ConfiguredChannel->Priority
	) > OpenMobileHapticsChannelPolicyPrivate::PriorityValue(RequestPriority))
	{
		Result.EffectivePriority = ConfiguredChannel->Priority;
	}
	Result.MaximumQueueDepth = FMath::Clamp(
		ConfiguredChannel->MaximumQueueDepth,
		0,
		Result.MaximumQueueDepth
	);
	Result.MaximumActiveHandles = FMath::Clamp(
		ConfiguredChannel->MaximumActiveHandles,
		0,
		Result.MaximumActiveHandles
	);
	return Result;
}

void FOpenMobileHapticsChannelArbiter::Configure(
	const FOpenMobileHapticsChannelLimits& InLimits
)
{
	Limits.MaximumActiveHandles =
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumActiveHandles(
			InLimits.MaximumActiveHandles
		);
	Limits.MaximumQueuedHandles =
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumQueuedHandles(
			InLimits.MaximumQueuedHandles
		);
	Limits.MaximumQueueDepthPerChannel =
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumQueueDepthPerChannel(
			InLimits.MaximumQueueDepthPerChannel
		);
}

FOpenMobileHapticsChannelAdmissionResult
FOpenMobileHapticsChannelArbiter::TryReserve(
	const FOpenMobileHapticsChannelAdmissionRequest& Request
)
{
	FOpenMobileHapticsChannelAdmissionResult Result;
	const FOpenMobileHapticsChannelAdmissionRequest* ExistingReservation =
		Reservations.Find(Request.RequestId);
	const bool bPromotingOverlapWait = ExistingReservation
		&& ExistingReservation->bWaitingForOverlap
		&& !Request.bWaitingForOverlap;
	if (ExistingReservation && !bPromotingOverlapWait)
	{
		Result.Outcome = EOpenMobileHapticsChannelAdmissionOutcome::Admitted;
		return Result;
	}

	if (!ExistingReservation && Request.bQueued)
	{
		const int32 ChannelLimit = FMath::Clamp(
			Request.MaximumQueueDepth,
			0,
			Limits.MaximumQueueDepthPerChannel
		);
		const int32 ChannelQueuedCount =
			QueuedCountsByChannel.FindRef(Request.Channel);
		if (ChannelQueuedCount >= ChannelLimit)
		{
			Result.Outcome = EOpenMobileHapticsChannelAdmissionOutcome::
				ChannelQueueCapacityReached;
			return Result;
		}
		if (QueuedCount >= Limits.MaximumQueuedHandles)
		{
			Result.Outcome = EOpenMobileHapticsChannelAdmissionOutcome::
				GlobalQueueCapacityReached;
			return Result;
		}
	}

	const int32 ChannelActiveLimit = FMath::Clamp(
		Request.MaximumActiveHandles,
		0,
		Limits.MaximumActiveHandles
	);
	const bool bChannelAtCapacity = !Request.bWaitingForOverlap
		&& ActiveCountsByChannel.FindRef(Request.Channel) >= ChannelActiveLimit;
	const bool bGlobalAtCapacity = !Request.bWaitingForOverlap
		&& ActiveCount >= Limits.MaximumActiveHandles;
	if (bChannelAtCapacity || bGlobalAtCapacity)
	{
		const int32 IncomingPriority =
			OpenMobileHapticsChannelPolicyPrivate::PriorityValue(Request.Priority);
		if (!Request.bRepeating)
		{
			for (const TPair<
				uint64,
				FOpenMobileHapticsChannelAdmissionRequest
			>& Pair : Reservations)
			{
				const FOpenMobileHapticsChannelAdmissionRequest& Existing =
					Pair.Value;
				if (Existing.bWaitingForOverlap
					|| (bChannelAtCapacity
						&& Existing.Channel != Request.Channel)
					|| !Existing.bRepeating
					|| OpenMobileHapticsChannelPolicyPrivate::PriorityValue(
						Existing.Priority
					) >= IncomingPriority)
				{
					continue;
				}
				if (Result.PreemptRequestId == 0)
				{
					Result.PreemptRequestId = Existing.RequestId;
					continue;
				}

				const FOpenMobileHapticsChannelAdmissionRequest& Current =
					Reservations.FindChecked(Result.PreemptRequestId);
				const int32 ExistingPriority =
					OpenMobileHapticsChannelPolicyPrivate::PriorityValue(
						Existing.Priority
					);
				const int32 CurrentPriority =
					OpenMobileHapticsChannelPolicyPrivate::PriorityValue(
						Current.Priority
					);
				if (ExistingPriority < CurrentPriority
					|| (ExistingPriority == CurrentPriority
						&& Existing.RequestId > Current.RequestId))
				{
					Result.PreemptRequestId = Existing.RequestId;
				}
			}
		}

		if (Result.PreemptRequestId != 0)
		{
			Result.Outcome = EOpenMobileHapticsChannelAdmissionOutcome::
				PreemptRequired;
		}
		else if (bChannelAtCapacity)
		{
			Result.Outcome = EOpenMobileHapticsChannelAdmissionOutcome::
				ChannelActiveCapacityReached;
		}
		return Result;
	}

	if (bPromotingOverlapWait)
	{
		const bool bWasQueued = ExistingReservation->bQueued;
		Reservations.Add(Request.RequestId, Request);
		if (bWasQueued && !Request.bQueued)
		{
			QueuedCount = FMath::Max(0, QueuedCount - 1);
			int32* ChannelQueuedCount =
				QueuedCountsByChannel.Find(Request.Channel);
			if (ChannelQueuedCount)
			{
				--(*ChannelQueuedCount);
				if (*ChannelQueuedCount <= 0)
				{
					QueuedCountsByChannel.Remove(Request.Channel);
				}
			}
		}
	}
	else
	{
		Reservations.Add(Request.RequestId, Request);
		if (Request.bQueued)
		{
			++QueuedCount;
			++QueuedCountsByChannel.FindOrAdd(Request.Channel);
		}
	}
	if (!Request.bWaitingForOverlap)
	{
		++ActiveCount;
		++ActiveCountsByChannel.FindOrAdd(Request.Channel);
	}
	Result.Outcome = EOpenMobileHapticsChannelAdmissionOutcome::Admitted;
	return Result;
}

void FOpenMobileHapticsChannelArbiter::Release(uint64 RequestId)
{
	FOpenMobileHapticsChannelAdmissionRequest Removed;
	if (!Reservations.RemoveAndCopyValue(RequestId, Removed))
	{
		return;
	}
	if (!Removed.bWaitingForOverlap)
	{
		ActiveCount = FMath::Max(0, ActiveCount - 1);
		int32* ChannelActiveCount = ActiveCountsByChannel.Find(Removed.Channel);
		if (ChannelActiveCount)
		{
			--(*ChannelActiveCount);
			if (*ChannelActiveCount <= 0)
			{
				ActiveCountsByChannel.Remove(Removed.Channel);
			}
		}
	}
	if (!Removed.bQueued)
	{
		return;
	}

	QueuedCount = FMath::Max(0, QueuedCount - 1);
	int32* ChannelCount = QueuedCountsByChannel.Find(Removed.Channel);
	if (ChannelCount == nullptr)
	{
		return;
	}
	--(*ChannelCount);
	if (*ChannelCount <= 0)
	{
		QueuedCountsByChannel.Remove(Removed.Channel);
	}
}

void FOpenMobileHapticsChannelArbiter::Reset()
{
	Reservations.Reset();
	ActiveCountsByChannel.Reset();
	QueuedCountsByChannel.Reset();
	ActiveCount = 0;
	QueuedCount = 0;
}
