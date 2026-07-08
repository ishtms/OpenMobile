#include "OpenMobileProximityMonitoringPolicy.h"

FOpenMobileProximityMonitoringAction
FOpenMobileProximityMonitoringPolicy::Acquire(
	bool bMonitoringEnabled,
	bool bInApplicationActive
)
{
	if (LeaseCount == 0)
	{
		bHasInitialState = true;
		bInitialMonitoringEnabled = bMonitoringEnabled;
		bObservedMonitoringEnabled = bMonitoringEnabled;
	}
	bApplicationActive = bInApplicationActive;
	++LeaseCount;
	return ResolveDesiredState();
}

FOpenMobileProximityMonitoringAction
FOpenMobileProximityMonitoringPolicy::Release()
{
	if (LeaseCount == 0)
	{
		return {};
	}
	--LeaseCount;
	const FOpenMobileProximityMonitoringAction Action = ResolveDesiredState();
	if (LeaseCount == 0)
	{
		bHasInitialState = false;
	}
	return Action;
}

FOpenMobileProximityMonitoringAction
FOpenMobileProximityMonitoringPolicy::SetApplicationActive(bool bActive)
{
	bApplicationActive = bActive;
	return ResolveDesiredState();
}

FOpenMobileProximityMonitoringAction
FOpenMobileProximityMonitoringPolicy::Shutdown()
{
	LeaseCount = 0;
	const FOpenMobileProximityMonitoringAction Action = ResolveDesiredState();
	bHasInitialState = false;
	bApplicationActive = false;
	return Action;
}

void FOpenMobileProximityMonitoringPolicy::ObserveMonitoringEnabled(
	bool bEnabled
)
{
	bObservedMonitoringEnabled = bEnabled;
}

int32 FOpenMobileProximityMonitoringPolicy::GetLeaseCount() const
{
	return LeaseCount;
}

FOpenMobileProximityMonitoringAction
FOpenMobileProximityMonitoringPolicy::ResolveDesiredState()
{
	if (!bHasInitialState)
	{
		return {};
	}
	const bool bDesiredMonitoringEnabled = bInitialMonitoringEnabled
		|| (LeaseCount > 0 && bApplicationActive);
	if (bDesiredMonitoringEnabled == bObservedMonitoringEnabled)
	{
		return {};
	}
	bObservedMonitoringEnabled = bDesiredMonitoringEnabled;
	FOpenMobileProximityMonitoringAction Action;
	Action.bShouldSetMonitoringEnabled = true;
	Action.bMonitoringEnabled = bDesiredMonitoringEnabled;
	return Action;
}
