#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "OpenMobileHapticAHAPFactory.generated.h"

UCLASS()
class UOpenMobileHapticAHAPFactory final : public UFactory
{
	GENERATED_BODY()

public:
	UOpenMobileHapticAHAPFactory(
		const FObjectInitializer& ObjectInitializer
	);

	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateFile(
		UClass* InClass,
		UObject* InParent,
		FName InName,
		EObjectFlags Flags,
		const FString& Filename,
		const TCHAR* Parms,
		FFeedbackContext* Warn,
		bool& bOutOperationCanceled
	) override;
};
