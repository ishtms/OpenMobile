#pragma once

#include "CoreMinimal.h"
#include "IOpenMobileSensorsBackend.h"

class FOpenMobileSensorsAndroidBackend final : public IOpenMobileSensorsBackend
{
public:
	virtual FName GetBackendName() const override;
	virtual FOpenMobileCapability GetBackendCapability() const override;
};
