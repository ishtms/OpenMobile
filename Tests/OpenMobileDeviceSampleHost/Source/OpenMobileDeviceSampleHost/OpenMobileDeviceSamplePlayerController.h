#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OpenMobileDeviceSamplePlayerController.generated.h"

class UOpenMobileDeviceDemoWidget;

UCLASS()
class OPENMOBILEDEVICESAMPLEHOST_API AOpenMobileDeviceSamplePlayerController final
	: public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileDeviceDemoWidget> DeviceWidget;
};
