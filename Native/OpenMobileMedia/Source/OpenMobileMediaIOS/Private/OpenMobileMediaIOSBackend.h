#pragma once

#include "IOpenMobileMediaBackend.h"

class FOpenMobileMediaIOSBackend final : public IOpenMobileMediaBackend
{
public:
	virtual FName GetBackendName() const override { return TEXT("IOS"); }
	virtual bool IsAvailable() const override { return true; }
	virtual bool LaunchPhotoPicker(int64 RequestId, FString& OutError) override;
	virtual void CancelPhotoPicker(int64 RequestId) override;
};
