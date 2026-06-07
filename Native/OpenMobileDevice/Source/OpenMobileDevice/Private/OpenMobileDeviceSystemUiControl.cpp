#include "OpenMobileDeviceSystemUiControl.h"

#include "OpenMobileDeviceSubsystem.h"
#include "OpenMobileDeviceSystemUiControlService.h"

void UOpenMobileSystemUiHandle::Release()
{
	if (!bActive)
	{
		return;
	}
	if (UOpenMobileDeviceSubsystem* DeviceSubsystem = Subsystem.Get())
	{
		DeviceSubsystem->ReleaseSystemUiHandle(this);
		return;
	}
	FOpenMobileDeviceSystemUiControlService::RemoveRequest(RequestId);
	bActive = false;
	RequestId.Invalidate();
	Subsystem.Reset();
}

void UOpenMobileSystemUiHandle::BeginDestroy()
{
	Release();
	Super::BeginDestroy();
}
