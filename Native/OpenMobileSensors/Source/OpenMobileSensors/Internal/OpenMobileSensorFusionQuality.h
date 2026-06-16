#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "OpenMobileSensorAccuracy.h"
#include "OpenMobileSensorQuality.h"

struct FOpenMobileSensorFusionInputObservation
{
	EOpenMobileSensorType Sensor = EOpenMobileSensorType::Unknown;
	bool bExpected = false;
	bool bAvailable = false;
	bool bValid = false;
	bool bContributed = false;
	EOpenMobileSensorAccuracy Accuracy = EOpenMobileSensorAccuracy::Unknown;
	bool bCalibrationRequired = false;
};

class OPENMOBILESENSORS_API FOpenMobileSensorFusionQualityEvaluator final
{
public:
	static FOpenMobileSensorFusionContext Evaluate(
		TConstArrayView<FOpenMobileSensorFusionInputObservation> Inputs
	);
	static FOpenMobileSensorFusionContext Evaluate(
		TConstArrayView<FOpenMobileSensorFusionInputObservation> Inputs,
		EOpenMobileSensorFusionQuality NativeQuality
	);
	static bool ValidateContext(
		const FOpenMobileSensorFusionContext& Context
	);
};
