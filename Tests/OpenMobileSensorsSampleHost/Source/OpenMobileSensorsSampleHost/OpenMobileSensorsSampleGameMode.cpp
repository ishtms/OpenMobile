#include "OpenMobileSensorsSampleGameMode.h"

#include "OpenMobileSensorsSamplePlayerController.h"

AOpenMobileSensorsSampleGameMode::AOpenMobileSensorsSampleGameMode()
{
	PlayerControllerClass = AOpenMobileSensorsSamplePlayerController::StaticClass();
}
