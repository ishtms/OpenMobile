#include "OpenMobileDeviceRefreshRateControl.h"

#include "OpenMobileDeviceRefreshRateControlService.h"
#include "OpenMobileDeviceSubsystem.h"

void UOpenMobilePreferredRefreshRateHandle::Release()
{
	if (!bActive)
	{
		return;
	}
	if (UOpenMobileDeviceSubsystem* DeviceSubsystem = Subsystem.Get())
	{
		DeviceSubsystem->ReleasePreferredRefreshRateHandle(this);
		return;
	}
	FOpenMobileDeviceRefreshRateControlService::RemoveRequest(RequestId);
	bActive = false;
	RequestId.Invalidate();
	Subsystem.Reset();
}

void UOpenMobilePreferredRefreshRateHandle::BeginDestroy()
{
	Release();
	Super::BeginDestroy();
}
