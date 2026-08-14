#pragma once

#include "OpenMobileHapticsAppleBridgeService.h"

/** Creates the Objective-C Core Haptics implementation behind the platform-neutral bridge interface. */
TUniquePtr<IOpenMobileHapticsAppleBridge> CreateOpenMobileHapticsIOSBridge();
