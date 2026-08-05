#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileSensorMetadata.h"
#include "OpenMobileSensorSamples.h"
#include "OpenMobileSensorOptionalValueLibrary.generated.h"

UCLASS()
class OPENMOBILESENSORS_API UOpenMobileSensorOptionalValueLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Get Optional Number or Default", Keywords = "OpenMobile sensors optional number fallback default", ToolTip = "Returns the available number, otherwise Default Value."))
	static double GetOptionalNumberOrDefault(
		const FOpenMobileSensorOptionalNumber& Optional,
		double DefaultValue = 0.0);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Get Optional Integer or Default", Keywords = "OpenMobile sensors optional integer fallback default", ToolTip = "Returns the available integer, otherwise Default Value."))
	static int64 GetOptionalIntegerOrDefault(
		const FOpenMobileSensorOptionalInteger& Optional,
		int64 DefaultValue = 0);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Get Optional Text or Default", Keywords = "OpenMobile sensors optional text string fallback default", ToolTip = "Returns the available text, otherwise Default Value."))
	static FString GetOptionalTextOrDefault(
		const FOpenMobileSensorOptionalText& Optional,
		const FString& DefaultValue);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Get Optional Boolean or Default", Keywords = "OpenMobile sensors optional bool boolean fallback default", ToolTip = "Returns the available Boolean, otherwise Default Value."))
	static bool GetOptionalBooleanOrDefault(
		const FOpenMobileSensorOptionalBoolean& Optional,
		bool DefaultValue = false);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Try Get Optional Number", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors optional number available missing", ToolTip = "Branches on number availability and initializes Value to zero when missing."))
	static bool TryGetOptionalNumber(
		const FOpenMobileSensorOptionalNumber& Optional,
		double& Value);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Try Get Optional Integer", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors optional integer available missing", ToolTip = "Branches on integer availability and initializes Value to zero when missing."))
	static bool TryGetOptionalInteger(
		const FOpenMobileSensorOptionalInteger& Optional,
		int64& Value);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Try Get Optional Text", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors optional text available missing", ToolTip = "Branches on text availability and initializes Value to empty when missing."))
	static bool TryGetOptionalText(
		const FOpenMobileSensorOptionalText& Optional,
		FString& Value);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Try Get Optional Boolean", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors optional bool boolean available missing", ToolTip = "Branches on Boolean availability and initializes Value to false when missing."))
	static bool TryGetOptionalBoolean(
		const FOpenMobileSensorOptionalBoolean& Optional,
		bool& Value);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Try Get Pedometer Distance", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors pedometer optional distance metres", ToolTip = "Branches on pedometer-distance availability and returns metres."))
	static bool TryGetPedometerDistance(
		const FOpenMobilePedometerMetrics& Metrics,
		double& DistanceMetres);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Try Get Floors Ascended", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors pedometer optional floors climbed", ToolTip = "Branches on ascending-floor availability."))
	static bool TryGetFloorsAscended(
		const FOpenMobilePedometerMetrics& Metrics,
		int64& Floors);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Try Get Floors Descended", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors pedometer optional floors descended", ToolTip = "Branches on descending-floor availability."))
	static bool TryGetFloorsDescended(
		const FOpenMobilePedometerMetrics& Metrics,
		int64& Floors);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (DisplayName = "Try Get Heading Accuracy", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors compass optional heading accuracy degrees", ToolTip = "Branches on heading-accuracy availability and returns degrees."))
	static bool TryGetHeadingAccuracy(
		const FOpenMobileHeadingSensorSample& Sample,
		double& AccuracyDegrees);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Try Get Altitude Vertical Accuracy", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors optional altitude vertical accuracy metres", ToolTip = "Branches on absolute-altitude vertical-accuracy availability and returns metres."))
	static bool TryGetAltitudeVerticalAccuracy(
		const FOpenMobileAbsoluteAltitudeMetadata& Metadata,
		double& AccuracyMetres);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Try Get Proximity Distance", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors optional proximity distance metres", ToolTip = "Branches on proximity-distance availability and returns metres."))
	static bool TryGetProximityDistance(
		const FOpenMobileProximitySensorSample& Sample,
		double& DistanceMetres);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Try Get Proximity Maximum Range", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors optional proximity maximum range metres", ToolTip = "Branches on proximity-range availability and returns metres."))
	static bool TryGetProximityMaximumRange(
		const FOpenMobileProximitySensorSample& Sample,
		double& MaximumRangeMetres);
};
