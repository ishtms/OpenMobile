#pragma once

#include "CoreMinimal.h"

class OPENMOBILESENSORS_API FOpenMobileSensorsPermissionPolicy final
{
public:
	static FName MotionActivity();
	static FName ActivityRecognition();
	static FName TrueHeadingLocation();
	static FName TrueHeadingLocationInput();
	static bool IsSensorPermission(FName Permission);
	static FString GetExplanation(FName Permission);
};
