#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OpenMobileSensorsSamplePlayerController.generated.h"

class UOpenMobileSensorsDemoWidget;

UCLASS()
class OPENMOBILESENSORSSAMPLEHOST_API AOpenMobileSensorsSamplePlayerController final
	: public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileSensorsDemoWidget> SensorsWidget;
};
