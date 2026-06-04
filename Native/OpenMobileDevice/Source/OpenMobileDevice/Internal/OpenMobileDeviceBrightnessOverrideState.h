#pragma once

#include "CoreMinimal.h"

class FOpenMobileDeviceBrightnessOverrideState final
{
public:
	void BeginScope(uint64 ScopeId, float CurrentValue);
	void RecordApplied(uint64 ScopeId, float AppliedValue);
	TOptional<float> ReleaseScope(uint64 ScopeId, float CurrentValue);
	bool HasScope() const { return bHasScope; }
	uint64 GetScopeId() const { return ActiveScopeId; }
	void Reset();

private:
	uint64 ActiveScopeId = 0;
	float OriginalValue = 0.0f;
	float AppliedValue = 0.0f;
	bool bHasScope = false;
	bool bHasAppliedValue = false;
};
