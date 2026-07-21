#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorErrorReport.h"
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
	static FOpenMobileSensorErrorReport Describe(
		const FOpenMobileSensorOperationResult& Result,
		const FOpenMobileSensorErrorContext& Context,
		double TimestampSeconds = -1.0
	);
	static void ApplyRateAdjustmentText(
		FOpenMobileSensorRateResolution& Resolution
	);
	static FString FormatForLog(
		const FOpenMobileSensorOperationResult& Result,
		const FOpenMobileSensorErrorContext* Context,
		bool bShipping = UE_BUILD_SHIPPING
	);
	static FString FormatForLog(
		const FOpenMobileSensorOperationResult& Result,
		bool bShipping = UE_BUILD_SHIPPING
	)
	{
		return FormatForLog(Result, nullptr, bShipping);
	}
};
