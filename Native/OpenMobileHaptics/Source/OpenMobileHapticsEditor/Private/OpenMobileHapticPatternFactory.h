#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "OpenMobileHapticPatternFactory.generated.h"

UCLASS()
class UOpenMobileHapticPatternFactory final : public UFactory
{
	GENERATED_BODY()

public:
	/** Registers this factory for new portable pattern assets inside the editor. */
	UOpenMobileHapticPatternFactory();

	/** Creates an empty editable pattern with Unreal ownership and flags supplied by the content browser. */
	virtual UObject* FactoryCreateNew(
		UClass* InClass,
		UObject* InParent,
		FName InName,
		EObjectFlags Flags,
		UObject* Context,
		FFeedbackContext* Warn
	) override;
};
