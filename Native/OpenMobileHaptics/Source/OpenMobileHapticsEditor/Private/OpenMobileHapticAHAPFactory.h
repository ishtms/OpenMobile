#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "OpenMobileHapticAHAPFactory.generated.h"

UCLASS()
class UOpenMobileHapticAHAPFactory final : public UFactory
{
	GENERATED_BODY()

public:
	/** Configures this factory for file import only, ordinary pattern creation belongs to the separate asset factory. */
	UOpenMobileHapticAHAPFactory(
		const FObjectInitializer& ObjectInitializer
	);

	/** Accepts only AHAP filenames here, content validation still runs during import. */
	virtual bool FactoryCanImport(const FString& Filename) override;
	/** Parses and normalizes AHAP before creating the asset, a failed import leaves no partly populated UObject. */
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
