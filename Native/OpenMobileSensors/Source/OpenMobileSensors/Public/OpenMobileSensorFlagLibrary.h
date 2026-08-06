#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileSensorCapabilities.h"
#include "OpenMobileSensorSamples.h"
#include "OpenMobileSensorFlagLibrary.generated.h"

UCLASS()
class OPENMOBILESENSORS_API UOpenMobileSensorFlagLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Has Sensor Source", Keywords = "OpenMobile sensors source flags raw native fused derived mock replay", ToolTip = "Returns whether Source Flags contains the selected semantic source."))
	static bool HasSensorSource(
		int32 SourceFlags,
		EOpenMobileSensorSourceFlags Source);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Has Timestamp Issue", Keywords = "OpenMobile sensors timestamp issue invalid duplicate backward", ToolTip = "Returns whether Timestamp Issue Flags contains the selected issue."))
	static bool HasTimestampIssue(
		int32 TimestampIssueFlags,
		EOpenMobileSensorTimestampIssue Issue);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Has Attitude Representation", Keywords = "OpenMobile sensors attitude output quaternion euler matrix", ToolTip = "Returns whether Attitude Representations contains the selected output."))
	static bool HasAttitudeRepresentation(
		int32 AttitudeRepresentations,
		EOpenMobileAttitudeRepresentation Representation);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Has Altitude Limitation", Keywords = "OpenMobile sensors relative altitude quality weather atmosphere native model", ToolTip = "Returns whether Quality Limitation Flags contains the selected relative-altitude limitation."))
	static bool HasAltitudeLimitation(
		int32 QualityLimitationFlags,
		EOpenMobileRelativeAltitudeQualityLimitation Limitation);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Has Unsupported Fallback Condition", Keywords = "OpenMobile sensors fallback unsupported missing rate calibration permission lifecycle", ToolTip = "Returns whether Unsupported Condition Flags contains the selected derived-fallback restriction."))
	static bool HasUnsupportedFallbackCondition(
		int32 UnsupportedConditionFlags,
		EOpenMobileSensorFallbackUnsupportedCondition Condition);
};
