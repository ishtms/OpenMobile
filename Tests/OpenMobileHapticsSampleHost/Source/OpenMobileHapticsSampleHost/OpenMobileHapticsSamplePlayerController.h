#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OpenMobileHapticsSamplePlayerController.generated.h"

class UOpenMobileHapticsSampleWidget;

UCLASS()
class OPENMOBILEHAPTICSSAMPLEHOST_API
AOpenMobileHapticsSamplePlayerController final : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileHapticsSampleWidget> HapticsWidget;
};
