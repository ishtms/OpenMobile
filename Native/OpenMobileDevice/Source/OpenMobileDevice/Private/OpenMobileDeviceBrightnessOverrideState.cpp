#include "OpenMobileDeviceBrightnessOverrideState.h"

void FOpenMobileDeviceBrightnessOverrideState::BeginScope(
	uint64 ScopeId,
	float CurrentValue
)
{
	if (bHasScope && ActiveScopeId == ScopeId)
	{
		if (bHasAppliedValue
			&& !FMath::IsNearlyEqual(CurrentValue, AppliedValue, 0.001f))
		{
			OriginalValue = CurrentValue;
			bHasAppliedValue = false;
		}
		return;
	}
	ActiveScopeId = ScopeId;
	OriginalValue = CurrentValue;
	AppliedValue = 0.0f;
	bHasScope = true;
	bHasAppliedValue = false;
}

void FOpenMobileDeviceBrightnessOverrideState::RecordApplied(
	uint64 ScopeId,
	float InAppliedValue
)
{
	if (!bHasScope || ActiveScopeId != ScopeId)
	{
		return;
	}
	AppliedValue = InAppliedValue;
	bHasAppliedValue = true;
}

TOptional<float> FOpenMobileDeviceBrightnessOverrideState::ReleaseScope(
	uint64 ScopeId,
	float CurrentValue
)
{
	if (!bHasScope || ActiveScopeId != ScopeId)
	{
		return {};
	}
	const TOptional<float> RestoreValue = bHasAppliedValue
		&& FMath::IsNearlyEqual(CurrentValue, AppliedValue, 0.001f)
		? TOptional<float>(OriginalValue)
		: TOptional<float>();
	Reset();
	return RestoreValue;
}

void FOpenMobileDeviceBrightnessOverrideState::Reset()
{
	ActiveScopeId = 0;
	OriginalValue = 0.0f;
	AppliedValue = 0.0f;
	bHasScope = false;
	bHasAppliedValue = false;
}
