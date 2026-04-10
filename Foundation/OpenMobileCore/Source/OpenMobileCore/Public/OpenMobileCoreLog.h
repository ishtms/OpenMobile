#pragma once

#include "CoreMinimal.h"

OPENMOBILECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogOpenMobile, Log, All);

class OPENMOBILECORE_API FOpenMobileLogFilter
{
public:
	static int32 GetGlobalLevel();
	static int32 GetDefaultGlobalLevel();
};
