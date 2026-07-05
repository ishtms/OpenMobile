#pragma once

#include "CoreMinimal.h"

struct FOpenMobileSensorDeclinationResult
{
	double DeclinationDegrees = 0.0;
	double HorizontalFieldNanoTesla = 0.0;
	double EstimatedErrorDegrees = 0.0;
};

class OPENMOBILESENSORS_API FOpenMobileSensorDeclination final
{
public:
	static bool CalculateWMM2025(
		double LatitudeDegrees,
		double LongitudeDegrees,
		double AltitudeMeters,
		double DecimalYear,
		FOpenMobileSensorDeclinationResult& OutResult
	);
};
