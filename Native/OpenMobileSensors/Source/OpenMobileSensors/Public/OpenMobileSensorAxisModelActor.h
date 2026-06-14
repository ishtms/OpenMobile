#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OpenMobileSensorAxisModelActor.generated.h"

class UArrowComponent;
class USceneComponent;

UCLASS(BlueprintType)
class OPENMOBILESENSORS_API AOpenMobileSensorAxisModelActor final
	: public AActor
{
	GENERATED_BODY()

public:
	AOpenMobileSensorAxisModelActor();

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Sensors|Validation")
	void SetSampleVectors(
		const FVector& Gravity,
		const FVector& AngularVelocity
	);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open Mobile|Sensors|Validation")
	TObjectPtr<UArrowComponent> XAxis;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open Mobile|Sensors|Validation")
	TObjectPtr<UArrowComponent> YAxis;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open Mobile|Sensors|Validation")
	TObjectPtr<UArrowComponent> ZAxis;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open Mobile|Sensors|Validation")
	TObjectPtr<UArrowComponent> GravityArrow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Open Mobile|Sensors|Validation")
	TObjectPtr<UArrowComponent> AngularVelocityArrow;

private:
	void SetOverlayVector(UArrowComponent& Arrow, const FVector& Value);

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;
};
