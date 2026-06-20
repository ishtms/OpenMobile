#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceAccessibilityTypes.generated.h"

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileAccessibilitySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalFloat PreferredTextScale;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString ContentSizeCategory;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bReducedAnimationPreferred;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString ReducedAnimationPlatformDetail;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bScreenReaderActive;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bTouchExplorationActive;

	bool operator==(const FOpenMobileAccessibilitySnapshot& Other) const
	{
		return Metadata == Other.Metadata
			&& PreferredTextScale == Other.PreferredTextScale
			&& ContentSizeCategory == Other.ContentSizeCategory
			&& bReducedAnimationPreferred == Other.bReducedAnimationPreferred
			&& ReducedAnimationPlatformDetail
				== Other.ReducedAnimationPlatformDetail
			&& bScreenReaderActive == Other.bScreenReaderActive
			&& bTouchExplorationActive == Other.bTouchExplorationActive;
	}
};
