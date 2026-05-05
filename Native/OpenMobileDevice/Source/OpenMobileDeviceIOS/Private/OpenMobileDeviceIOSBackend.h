#pragma once

#include "IOpenMobileDeviceBackend.h"

class FOpenMobileDeviceIOSBackend final : public IOpenMobileDeviceBackend
{
public:
	virtual FName GetBackendName() const override { return TEXT("IOS"); }
};
