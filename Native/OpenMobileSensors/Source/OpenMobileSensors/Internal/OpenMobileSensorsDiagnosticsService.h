#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorDiagnostics.h"

class OPENMOBILESENSORS_API FOpenMobileSensorsDiagnosticsService final
{
public:
	static FOpenMobileSensorDiagnosticsSnapshot Capture(
		const FGuid* OwnerIdentifier = nullptr
	);
};
