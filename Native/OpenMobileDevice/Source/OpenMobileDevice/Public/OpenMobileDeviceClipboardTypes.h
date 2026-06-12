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

UENUM(BlueprintType)
enum class EOpenMobileClipboardOperationState : uint8
{
	Unknown,
	Succeeded,
	Empty,
	TypeUnavailable,
	Unavailable,
	Unsupported,
	Denied,
	InvalidValue,
	TooLarge,
	Failed
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileClipboardWriteRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	EOpenMobileClipboardContentType ContentType =
		EOpenMobileClipboardContentType::Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	FString Value;
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

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileClipboardOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileClipboardOperationState State =
		EOpenMobileClipboardOperationState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileClipboardContent Content;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileError Error;

	bool IsSuccessful() const
	{
		return State == EOpenMobileClipboardOperationState::Succeeded
			|| State == EOpenMobileClipboardOperationState::Empty;
	}
};
