#pragma once

#include "IOpenMobileDeviceBackend.h"

class FOpenMobileDeviceAndroidBackend final : public IOpenMobileDeviceBackend
{
public:
	virtual FName GetBackendName() const override { return TEXT("Android"); }
};
