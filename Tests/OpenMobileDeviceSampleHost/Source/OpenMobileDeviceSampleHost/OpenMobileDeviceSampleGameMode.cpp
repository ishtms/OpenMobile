#include "OpenMobileDeviceSampleGameMode.h"

#include "OpenMobileDeviceSamplePlayerController.h"

AOpenMobileDeviceSampleGameMode::AOpenMobileDeviceSampleGameMode()
{
	PlayerControllerClass = AOpenMobileDeviceSamplePlayerController::StaticClass();
}
