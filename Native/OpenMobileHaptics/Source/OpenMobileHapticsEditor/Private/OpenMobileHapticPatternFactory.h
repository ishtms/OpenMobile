#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "OpenMobileHapticPatternFactory.generated.h"

UCLASS()
class UOpenMobileHapticPatternFactory final : public UFactory
{
	GENERATED_BODY()

public:
	UOpenMobileHapticPatternFactory();

	virtual UObject* FactoryCreateNew(
		UClass* InClass,
		UObject* InParent,
		FName InName,
		EObjectFlags Flags,
		UObject* Context,
		FFeedbackContext* Warn
	) override;
};
