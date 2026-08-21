#pragma once

#include "CoreMinimal.h"
#include "OpenMobileMediaTypes.generated.h"

class UTexture2D;

/** Metadata read from the original image before creating a display-ready texture. */
USTRUCT(BlueprintType)
struct OPENMOBILEMEDIA_API FOpenMobileMediaMetadata
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	FString FileName;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	FString MimeType;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	int64 FileSizeBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	int32 Width = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	int32 Height = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	FString DateTaken;

	/** Original EXIF orientation (1-8), or 0 when absent. */
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	int32 ExifOrientation = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	FString CameraMake;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	FString CameraModel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	FString LensModel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	float Aperture = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	float ExposureTimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	int32 Iso = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	float FocalLengthMm = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	bool bHasLocation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	double Latitude = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	double Longitude = 0.0;

	/** Provider payload for fields not yet represented by the stable API. */
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	FString RawMetadataJson;
};

USTRUCT(BlueprintType)
struct OPENMOBILEMEDIA_API FOpenMobileMediaPickResult
{
	GENERATED_BODY()

	/** Transient texture. Keep it in a UPROPERTY or assign it to a UImage brush. */
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	TObjectPtr<UTexture2D> Texture = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Media")
	FOpenMobileMediaMetadata Metadata;
};
