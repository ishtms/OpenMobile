#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceClipboardTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileClipboardContentType : uint8
{
	Unknown,
	Empty,
	Text,
	Url
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileClipboardContent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bContentTypesAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	TArray<EOpenMobileClipboardContentType> ContentTypes;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString Text;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString Url;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bReadWasUserInitiated = false;

	bool operator==(const FOpenMobileClipboardContent& Other) const
	{
		return Metadata == Other.Metadata
			&& bContentTypesAvailable == Other.bContentTypesAvailable
			&& ContentTypes == Other.ContentTypes
			&& Text == Other.Text
			&& Url == Other.Url
			&& bReadWasUserInitiated == Other.bReadWasUserInitiated;
	}
};
