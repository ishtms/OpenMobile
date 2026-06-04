#include "OpenMobileDeviceBrightnessControl.h"

#include "OpenMobileDeviceBrightnessControlService.h"
#include "OpenMobileDeviceSubsystem.h"

void UOpenMobileBrightnessHandle::Release()
{
	if (!bActive)
	{
		return;
	}
	if (UOpenMobileDeviceSubsystem* DeviceSubsystem = Subsystem.Get())
	{
		DeviceSubsystem->ReleaseBrightnessHandle(this);
		return;
	}
	FOpenMobileDeviceBrightnessControlService::RemoveRequest(RequestId);
	bActive = false;
	RequestId.Invalidate();
	Subsystem.Reset();
}

void UOpenMobileBrightnessHandle::BeginDestroy()
{
	Release();
	Super::BeginDestroy();
}
