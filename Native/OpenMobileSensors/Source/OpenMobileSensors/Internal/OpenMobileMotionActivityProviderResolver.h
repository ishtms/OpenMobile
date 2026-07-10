#pragma once

#include "IOpenMobileMotionActivityProvider.h"

class FOpenMobileMotionActivityProviderResolver
{
public:
	static IOpenMobileMotionActivityProvider* FindProvider();
	static FOpenMobileSensorCapability GetCapability();
};
