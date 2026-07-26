#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorStreamOptions.h"
#include "OpenMobileSensorBlueprintLibrary.generated.h"

UCLASS()
class OPENMOBILESENSORS_API UOpenMobileSensorBlueprintLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Was Sensor Operation Successful", Keywords = "OpenMobile sensors result success accepted", ToolTip = "Returns true for both completed success and accepted asynchronous work."))
	static bool WasSensorOperationSuccessful(
		const FOpenMobileSensorOperationResult& Result
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Branch on Sensor Operation Result", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors result success accepted branch", ToolTip = "Routes both completed success and accepted asynchronous work through the True execution pin."))
	static bool BranchOnSensorOperationResult(
		const FOpenMobileSensorOperationResult& Result
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Is Sensor Subscription Handle Valid", Keywords = "OpenMobile sensors subscription handle valid", ToolTip = "Returns whether this raw subscription handle identifies an owned sensor stream."))
	static bool IsSensorSubscriptionHandleValid(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Are Sensor Subscription Handles Equal", Keywords = "OpenMobile sensors subscription handle compare equal", ToolTip = "Returns whether two raw subscription handles identify the same sensor stream generation."))
	static bool AreSensorSubscriptionHandlesEqual(
		const FOpenMobileSensorSubscriptionHandle& A,
		const FOpenMobileSensorSubscriptionHandle& B
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Make Invalid Sensor Subscription Handle", Keywords = "OpenMobile sensors subscription handle invalid clear", ToolTip = "Creates an intentional invalid raw subscription handle for initialization or comparison."))
	static FOpenMobileSensorSubscriptionHandle
	MakeInvalidSensorSubscriptionHandle();

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Invalidate Sensor Subscription Handle", Keywords = "OpenMobile sensors subscription handle invalidate reset clear", ToolTip = "Clears a stored raw subscription handle. This does not stop an active stream."))
	static void InvalidateSensorSubscriptionHandle(
		UPARAM(ref) FOpenMobileSensorSubscriptionHandle& Handle
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Sensor Sample Family", Keywords = "OpenMobile sensors sample family vector attitude scalar heading steps activity orientation proximity", ToolTip = "Returns the generic sample family used by an advanced raw sensor stream."))
	static EOpenMobileSensorSampleFamily GetSensorSampleFamily(
		EOpenMobileSensorType Sensor
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Make Sensor Identifier", NativeMakeFunc, Keywords = "OpenMobile sensors identifier preferred instance", AdvancedDisplay = "InstanceId", ToolTip = "Creates a raw sensor identifier. Leave Instance Id empty to select the preferred matching sensor instance."))
	static FOpenMobileSensorIdentifier MakeSensorIdentifier(
		EOpenMobileSensorType Sensor,
		FName InstanceId = NAME_None
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Default Sensor Stream Options", Keywords = "OpenMobile sensors default recommended stream options project settings", ToolTip = "Returns the stream defaults configured in Project Settings under OpenMobile Sensors."))
	static FOpenMobileSensorStreamOptions GetDefaultSensorStreamOptions();
};
