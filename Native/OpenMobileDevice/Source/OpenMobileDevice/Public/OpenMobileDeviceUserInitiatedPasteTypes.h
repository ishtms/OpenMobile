#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceClipboardTypes.h"
#include "OpenMobileDeviceUserInitiatedPasteTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileUserInitiatedPasteState : uint8
{
	Unknown,
	Success,
	Cancelled,
	Denied,
	Failed
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileUserInitiatedPasteRequest
{
	GENERATED_BODY()

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Device",
		meta = (ToolTip = "Portable clipboard value type to request. Text and Url are supported.")
	)
	EOpenMobileClipboardContentType ContentType =
		EOpenMobileClipboardContentType::Text;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Device",
		meta = (ToolTip = "Set true only while handling a current player paste action. Requests without this confirmation fail before native work.")
	)
	bool bCallerConfirmsUserInitiated = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileUserInitiatedPasteResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileUserInitiatedPasteState State =
		EOpenMobileUserInitiatedPasteState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileClipboardContent Content;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileError Error;
};
