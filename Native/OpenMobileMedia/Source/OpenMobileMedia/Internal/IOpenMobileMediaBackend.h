#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

/** Platform implementation seam. This header is internal to OpenMobileMedia. */
class IOpenMobileMediaBackend : public IModularFeature
{
public:
	virtual ~IOpenMobileMediaBackend() = default;

	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("OpenMobile.Media.Backend"));
		return FeatureName;
	}

	virtual FName GetBackendName() const = 0;
	virtual int32 GetPriority() const { return 0; }
	virtual bool IsAvailable() const = 0;
	virtual bool LaunchPhotoPicker(int64 RequestId, FString& OutError) = 0;
	virtual void CancelPhotoPicker(int64 RequestId) = 0;
};
