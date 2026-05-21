#pragma once

#include "CoreMinimal.h"
#include "IOpenMobileSensorsBackend.h"

class FOpenMobileSensorsIOSBackend final : public IOpenMobileSensorsBackend
{
public:
	virtual FName GetBackendName() const override;
	virtual FOpenMobileCapability GetBackendCapability() const override;
};
