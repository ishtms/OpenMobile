#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileNativeStepCount.generated.h"

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileNativeStepCountQuery
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Inclusive query start in Unix time seconds."))
	double StartUnixTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Sensors", meta = (ToolTip = "Exclusive query end in Unix time seconds."))
	double EndUnixTimeSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileNativeStepCountQueryResult
{
	GENERATED_BODY()

	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Activity", meta = (ToolTip = "Typed identity for this historical step query. Different request-handle types cannot be connected in Blueprint."))
	FOpenMobileNativeStepCountQueryHandle Request;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Query for this native step count query result."))
	FOpenMobileNativeStepCountQuery Query;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Outcome details for the operation, including any failure and correction."))
	FOpenMobileSensorOperationResult Operation;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Sample for this native step count query result."))
	FOpenMobileStepsSensorSample Sample;
};

UCLASS()
class OPENMOBILESENSORS_API UOpenMobileNativeStepCountLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Make Native Step Query for Last Duration", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors historical steps recent duration date time", ToolTip = "Builds a historical step query ending at the current UTC time. Duration must be positive and fit after the Unix epoch."))
	static bool MakeNativeStepQueryForLastDuration(
		FTimespan Duration,
		FOpenMobileNativeStepCountQuery& OutQuery);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Activity", meta = (DisplayName = "Make Native Step Query Between Dates", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors historical steps dates UTC range", ToolTip = "Builds a historical step query from two UTC dates without manual Unix-time arithmetic. Start is inclusive and End is exclusive."))
	static bool MakeNativeStepQueryBetweenDates(
		FDateTime StartInclusiveUtc,
		FDateTime EndExclusiveUtc,
		FOpenMobileNativeStepCountQuery& OutQuery);
};
