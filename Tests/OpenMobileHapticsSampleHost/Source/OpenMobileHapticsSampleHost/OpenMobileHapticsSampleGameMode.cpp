#include "OpenMobileHapticsSampleGameMode.h"

#include "OpenMobileHapticsSamplePlayerController.h"

AOpenMobileHapticsSampleGameMode::AOpenMobileHapticsSampleGameMode()
{
	PlayerControllerClass =
		AOpenMobileHapticsSamplePlayerController::StaticClass();
}
