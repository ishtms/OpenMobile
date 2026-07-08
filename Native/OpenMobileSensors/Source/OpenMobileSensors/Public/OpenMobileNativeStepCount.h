#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileNativeStepCount.generated.h"

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileNativeStepCountQuery
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (ToolTip = "Inclusive query start in Unix time seconds."))
	double StartUnixTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Sensors", meta = (ToolTip = "Exclusive query end in Unix time seconds."))
	double EndUnixTimeSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileNativeStepCountQueryResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileNativeStepCountQuery Query;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileSensorOperationResult Operation;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	FOpenMobileStepsSensorSample Sample;
};
