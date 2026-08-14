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
	/** Use this when a missing number should fall back to Default Value. You won't need a separate availability branch. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Get Optional Number or Default", Keywords = "OpenMobile sensors optional number fallback default", ToolTip = "Returns the available number, otherwise Default Value."))
	static double GetOptionalNumberOrDefault(
		const FOpenMobileSensorOptionalNumber& Optional,
		double DefaultValue = 0.0);

	/** Use this when a missing integer should fall back to Default Value. You won't need a separate availability branch. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Get Optional Integer or Default", Keywords = "OpenMobile sensors optional integer fallback default", ToolTip = "Returns the available integer, otherwise Default Value."))
	static int64 GetOptionalIntegerOrDefault(
		const FOpenMobileSensorOptionalInteger& Optional,
		int64 DefaultValue = 0);

	/** Use this when a missing text should fall back to Default Value. You won't need a separate availability branch. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Get Optional Text or Default", Keywords = "OpenMobile sensors optional text string fallback default", ToolTip = "Returns the available text, otherwise Default Value."))
	static FString GetOptionalTextOrDefault(
		const FOpenMobileSensorOptionalText& Optional,
		const FString& DefaultValue);

	/** Use this when a missing Boolean should fall back to Default Value. You won't need a separate availability branch. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Get Optional Boolean or Default", Keywords = "OpenMobile sensors optional bool boolean fallback default", ToolTip = "Returns the available Boolean, otherwise Default Value."))
	static bool GetOptionalBooleanOrDefault(
		const FOpenMobileSensorOptionalBoolean& Optional,
		bool DefaultValue = false);

	/** Use this when missing number needs its own execution pin. The missing path sets Value to zero, so an old value can't slip through. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Try Get Optional Number", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors optional number available missing", ToolTip = "Branches on number availability and initializes Value to zero when missing."))
	static bool TryGetOptionalNumber(
		const FOpenMobileSensorOptionalNumber& Optional,
		double& Value);

	/** Use this when missing integer needs its own execution pin. The missing path sets Value to zero, so an old value can't slip through. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Try Get Optional Integer", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors optional integer available missing", ToolTip = "Branches on integer availability and initializes Value to zero when missing."))
	static bool TryGetOptionalInteger(
		const FOpenMobileSensorOptionalInteger& Optional,
		int64& Value);

	/** Use this when missing text needs its own execution pin. The missing path sets Value to empty text, so an old value can't slip through. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Try Get Optional Text", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors optional text available missing", ToolTip = "Branches on text availability and initializes Value to empty when missing."))
	static bool TryGetOptionalText(
		const FOpenMobileSensorOptionalText& Optional,
		FString& Value);

	/** Use this when missing Boolean needs its own execution pin. The missing path sets Value to false, so an old value can't slip through. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Try Get Optional Boolean", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors optional bool boolean available missing", ToolTip = "Branches on Boolean availability and initializes Value to false when missing."))
	static bool TryGetOptionalBoolean(
		const FOpenMobileSensorOptionalBoolean& Optional,
		bool& Value);

	/** Use this when missing pedometer distance needs its own execution pin. The missing path sets Value to zero metres, so an old value can't slip through. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Try Get Pedometer Distance", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors pedometer optional distance metres", ToolTip = "Branches on pedometer-distance availability and returns metres."))
	static bool TryGetPedometerDistance(
		const FOpenMobilePedometerMetrics& Metrics,
		double& DistanceMetres);

	/** Use this when missing floors ascended needs its own execution pin. The missing path sets Value to zero, so an old value can't slip through. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Try Get Floors Ascended", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors pedometer optional floors climbed", ToolTip = "Branches on ascending-floor availability."))
	static bool TryGetFloorsAscended(
		const FOpenMobilePedometerMetrics& Metrics,
		int64& Floors);

	/** Use this when missing floors descended needs its own execution pin. The missing path sets Value to zero, so an old value can't slip through. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Try Get Floors Descended", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors pedometer optional floors descended", ToolTip = "Branches on descending-floor availability."))
	static bool TryGetFloorsDescended(
		const FOpenMobilePedometerMetrics& Metrics,
		int64& Floors);

	/** Use this when missing heading accuracy needs its own execution pin. The missing path sets Value to zero degrees, so an old value can't slip through. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (DisplayName = "Try Get Heading Accuracy", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors compass optional heading accuracy degrees", ToolTip = "Branches on heading-accuracy availability and returns degrees."))
	static bool TryGetHeadingAccuracy(
		const FOpenMobileHeadingSensorSample& Sample,
		double& AccuracyDegrees);

	/** Use this when missing vertical accuracy needs its own execution pin. The missing path sets Value to zero metres, so an old value can't slip through. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Try Get Altitude Vertical Accuracy", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors optional altitude vertical accuracy metres", ToolTip = "Branches on absolute-altitude vertical-accuracy availability and returns metres."))
	static bool TryGetAltitudeVerticalAccuracy(
		const FOpenMobileAbsoluteAltitudeMetadata& Metadata,
		double& AccuracyMetres);

	/** Use this when missing proximity distance needs its own execution pin. The missing path sets Value to zero metres, so an old value can't slip through. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Try Get Proximity Distance", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors optional proximity distance metres", ToolTip = "Branches on proximity-distance availability and returns metres."))
	static bool TryGetProximityDistance(
		const FOpenMobileProximitySensorSample& Sample,
		double& DistanceMetres);

	/** Use this when missing proximity maximum range needs its own execution pin. The missing path sets Value to zero metres, so an old value can't slip through. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Try Get Proximity Maximum Range", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors optional proximity maximum range metres", ToolTip = "Branches on proximity-range availability and returns metres."))
	static bool TryGetProximityMaximumRange(
		const FOpenMobileProximitySensorSample& Sample,
		double& MaximumRangeMetres);
};
