#include "OpenMobileDeviceKeepScreenAwakeControl.h"

#include "OpenMobileDeviceKeepScreenAwakeControlService.h"
#include "OpenMobileDeviceSubsystem.h"

void UOpenMobileKeepScreenAwakeHandle::Release()
{
	if (!bActive)
	{
		return;
	}
	if (UOpenMobileDeviceSubsystem* DeviceSubsystem = Subsystem.Get())
	{
		DeviceSubsystem->ReleaseKeepScreenAwakeHandle(this);
		return;
	}
	FOpenMobileDeviceKeepScreenAwakeControlService::RemoveRequest(RequestId);
	bActive = false;
	RequestId.Invalidate();
	Subsystem.Reset();
}

void UOpenMobileKeepScreenAwakeHandle::BeginDestroy()
{
	Release();
	Super::BeginDestroy();
}
