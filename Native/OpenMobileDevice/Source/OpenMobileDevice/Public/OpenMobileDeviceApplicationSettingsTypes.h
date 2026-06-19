#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceApplicationSettingsTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileApplicationSettingsOpenState : uint8
{
	Unknown,
	Accepted,
	Unsupported,
	NoPresenter,
	NativeFailure
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileApplicationSettingsOpenResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileApplicationSettingsOpenState State =
		EOpenMobileApplicationSettingsOpenState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileError Error;

	bool IsAccepted() const
	{
		return State == EOpenMobileApplicationSettingsOpenState::Accepted;
	}
};
