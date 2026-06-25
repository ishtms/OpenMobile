#include "OpenMobileHapticsDynamicParameterPolicy.h"

bool FOpenMobileHapticsDynamicParameterPolicy::IsValid(
	const FOpenMobileHapticDynamicParameterUpdate& Update
)
{
	if (!Update.bUpdateIntensity && !Update.bUpdateSharpness)
	{
		return false;
	}
	if (Update.bUpdateIntensity
		&& (!FMath::IsFinite(Update.Intensity)
			|| Update.Intensity < 0.0f
			|| Update.Intensity > 1.0f))
	{
		return false;
	}
	return !Update.bUpdateSharpness
		|| (FMath::IsFinite(Update.Sharpness)
			&& Update.Sharpness >= 0.0f
			&& Update.Sharpness <= 1.0f);
}

void FOpenMobileHapticsDynamicParameterPolicy::RegisterPlayback(
	uint64 RequestId,
	double SubmissionTimeSeconds
)
{
	if (RequestId == 0 || !FMath::IsFinite(SubmissionTimeSeconds))
	{
		return;
	}
	FPlaybackState State;
	State.LastSubmissionTimeSeconds = SubmissionTimeSeconds;
	Playbacks.Add(RequestId, State);
}

void FOpenMobileHapticsDynamicParameterPolicy::RemovePlayback(uint64 RequestId)
{
	Playbacks.Remove(RequestId);
}

bool FOpenMobileHapticsDynamicParameterPolicy::HasPending() const
{
	for (const TPair<uint64, FPlaybackState>& Pair : Playbacks)
	{
		if (Pair.Value.bHasPending)
		{
			return true;
		}
	}
	return false;
}

EOpenMobileHapticsDynamicParameterQueueOutcome
FOpenMobileHapticsDynamicParameterPolicy::Queue(
	uint64 RequestId,
	const FOpenMobileHapticDynamicParameterUpdate& Update,
	double NowSeconds,
	double MinimumIntervalSeconds,
	FOpenMobileHapticDynamicParameterUpdate& OutReady
)
{
	OutReady = {};
	FPlaybackState* State = Playbacks.Find(RequestId);
	if (!State
		|| !IsValid(Update)
		|| !FMath::IsFinite(NowSeconds)
		|| !FMath::IsFinite(MinimumIntervalSeconds)
		|| MinimumIntervalSeconds < 0.0)
	{
		return EOpenMobileHapticsDynamicParameterQueueOutcome::Invalid;
	}

	if (State->bHasPending)
	{
		Merge(State->Pending, Update);
	}
	else
	{
		State->Pending = Update;
		State->bHasPending = true;
	}
	if (!IsReady(*State, NowSeconds, MinimumIntervalSeconds))
	{
		return EOpenMobileHapticsDynamicParameterQueueOutcome::Coalesced;
	}

	OutReady = TakePending(*State);
	return EOpenMobileHapticsDynamicParameterQueueOutcome::Ready;
}

void FOpenMobileHapticsDynamicParameterPolicy::CollectReady(
	double NowSeconds,
	double MinimumIntervalSeconds,
	TArray<FOpenMobileHapticsScheduledDynamicParameterUpdate>& OutUpdates
)
{
	OutUpdates.Reset();
	if (!FMath::IsFinite(NowSeconds)
		|| !FMath::IsFinite(MinimumIntervalSeconds)
		|| MinimumIntervalSeconds < 0.0)
	{
		return;
	}

	TArray<uint64> RequestIds;
	Playbacks.GetKeys(RequestIds);
	RequestIds.Sort();
	for (const uint64 RequestId : RequestIds)
	{
		FPlaybackState* State = Playbacks.Find(RequestId);
		if (!State || !State->bHasPending
			|| !IsReady(*State, NowSeconds, MinimumIntervalSeconds))
		{
			continue;
		}
		FOpenMobileHapticsScheduledDynamicParameterUpdate& Scheduled =
			OutUpdates.AddDefaulted_GetRef();
		Scheduled.RequestId = RequestId;
		Scheduled.Update = TakePending(*State);
	}
}

void FOpenMobileHapticsDynamicParameterPolicy::MarkAttempted(
	uint64 RequestId,
	double AttemptTimeSeconds
)
{
	FPlaybackState* State = Playbacks.Find(RequestId);
	if (State && FMath::IsFinite(AttemptTimeSeconds))
	{
		State->LastSubmissionTimeSeconds = AttemptTimeSeconds;
	}
}

void FOpenMobileHapticsDynamicParameterPolicy::Merge(
	FOpenMobileHapticDynamicParameterUpdate& Target,
	const FOpenMobileHapticDynamicParameterUpdate& Source
)
{
	if (Source.bUpdateIntensity)
	{
		Target.bUpdateIntensity = true;
		Target.Intensity = Source.Intensity;
	}
	if (Source.bUpdateSharpness)
	{
		Target.bUpdateSharpness = true;
		Target.Sharpness = Source.Sharpness;
	}
}

bool FOpenMobileHapticsDynamicParameterPolicy::IsReady(
	const FPlaybackState& State,
	double NowSeconds,
	double MinimumIntervalSeconds
)
{
	return NowSeconds >= State.LastSubmissionTimeSeconds
		+ MinimumIntervalSeconds;
}

FOpenMobileHapticDynamicParameterUpdate
FOpenMobileHapticsDynamicParameterPolicy::TakePending(FPlaybackState& State)
{
	FOpenMobileHapticDynamicParameterUpdate Pending = State.Pending;
	State.Pending = {};
	State.bHasPending = false;
	return Pending;
}
