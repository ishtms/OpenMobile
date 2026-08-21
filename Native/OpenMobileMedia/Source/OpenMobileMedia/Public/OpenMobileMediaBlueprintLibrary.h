#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileMediaTypes.h"
#include "OpenMobileMediaBlueprintLibrary.generated.h"

UCLASS()
class OPENMOBILEMEDIA_API UOpenMobileMediaBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Open Mobile|Media")
	static bool IsPhotoPickerSupported();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Media")
	static FText FormatPhotoMetadata(const FOpenMobileMediaMetadata& Metadata);
};
