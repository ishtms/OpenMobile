#pragma once

#include "OpenMobileDeviceApplicationSettingsTypes.h"

DECLARE_MULTICAST_DELEGATE(FOpenMobileDeviceApplicationSettingsReturned);

class FOpenMobileDeviceApplicationSettingsService final
{
public:
	static void Start();
	static void Shutdown();
	static FOpenMobileApplicationSettingsOpenResult Open();
	static FOpenMobileDeviceApplicationSettingsReturned& OnReturned();

#if WITH_DEV_AUTOMATION_TESTS
	static void ResetForTests();
	static void SetApplicationActiveForTests(bool bActive);
#endif
};
