#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceAndroidPackageTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileAndroidPackageCheckState : uint8
{
	Unknown,
	Installed,
	Disabled,
	NotFoundOrNotVisible,
	NotDeclared,
	Unsupported,
	InvalidRequest,
	Failed
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileAndroidPackageCheckRequest
{
	GENERATED_BODY()

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Device",
		meta = (ToolTip = "One exact Android application ID declared in OpenMobile Device settings. Wildcards and arbitrary package queries are rejected.")
	)
	FString PackageName;
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileAndroidPackageCheckResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileAndroidPackageCheckState State =
		EOpenMobileAndroidPackageCheckState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileError Error;
};
