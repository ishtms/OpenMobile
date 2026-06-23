#include "OpenMobileDeviceMockSettings.h"

#include "OpenMobileDeviceEditorMock.h"

void UOpenMobileDeviceMockSettings::ResetOverrides()
{
	FOpenMobileDeviceEditorMock::ResetOverrides();
}

void UOpenMobileDeviceMockSettings::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent
)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FOpenMobileDeviceEditorMock::ApplySettings();
}
