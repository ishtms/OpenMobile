#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileSensors.h"
#include "OpenMobileSensorsBlueprintExamples.generated.h"

UCLASS()
class OPENMOBILESENSORSSAMPLEHOST_API UOpenMobileSensorsBlueprintExamples final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "OpenMobile Sensors Sample")
	static FOpenMobileSensorSubscriptionRequest MakeLowRateRequest(
		EOpenMobileSensorType SensorType,
		EOpenMobileSensorDeliveryMode DeliveryMode =
			EOpenMobileSensorDeliveryMode::LatestValue,
		EOpenMobileSensorCoordinateSpace CoordinateSpace =
			EOpenMobileSensorCoordinateSpace::DeviceFixed
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile Sensors Sample")
	static FVector2D ComputeTiltSteering(
		const FVector& AccelerationMetresPerSecondSquared
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile Sensors Sample")
	static FVector2D ComputeGyroAimDelta(
		const FVector& AngularVelocityRadiansPerSecond,
		double DeltaSeconds
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile Sensors Sample")
	static FString DescribeSampleHeader(
		const FOpenMobileSensorSampleHeader& Header,
		double SampleAgeSeconds
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile Sensors Sample")
	static FText GetAxisConvention();

	UFUNCTION(BlueprintPure, Category = "OpenMobile Sensors Sample")
	static FText GetBlueprintRecipes();
};
