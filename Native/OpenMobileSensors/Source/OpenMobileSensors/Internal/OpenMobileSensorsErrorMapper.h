#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorResults.h"

class OPENMOBILESENSORS_API FOpenMobileSensorsErrorMapper final
{
public:
	static FOpenMobileSensorOperationResult Map(
		EOpenMobileSensorFailureReason Reason,
		FString NativeDomain = {},
		FString NativeCode = {}
	);
	static FOpenMobileSensorOperationResult FromCommon(
		const FOpenMobileError& Error
	);
	static FString FormatForLog(
		const FOpenMobileSensorOperationResult& Result,
		bool bShipping = UE_BUILD_SHIPPING
	);
};
