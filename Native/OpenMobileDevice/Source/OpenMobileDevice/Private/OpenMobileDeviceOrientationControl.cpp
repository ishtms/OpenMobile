#include "OpenMobileDeviceOrientationControl.h"

#include "OpenMobileDeviceOrientationControlService.h"
#include "OpenMobileDeviceSubsystem.h"

void UOpenMobileOrientationPolicyHandle::Release()
{
	if (!bActive)
	{
		return;
	}
	if (UOpenMobileDeviceSubsystem* DeviceSubsystem = Subsystem.Get())
	{
		DeviceSubsystem->ReleaseOrientationPolicyHandle(this);
		return;
	}
	FOpenMobileDeviceOrientationControlService::RemoveRequest(RequestId);
	bActive = false;
	RequestId.Invalidate();
	Subsystem.Reset();
}

void UOpenMobileOrientationPolicyHandle::BeginDestroy()
{
	Release();
	Super::BeginDestroy();
}
