#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileAdFormat : uint8
{
	Banner UMETA(DisplayName = "320x50 Fixed Banner"),
	Interstitial,
	Rewarded,
	RewardedInterstitial,
	AppOpen,
	NativeDisplay UMETA(DisplayName = "Native"),
	AnchoredAdaptiveBanner UMETA(DisplayName = "Anchored Adaptive Banner"),
	MediumRectangle UMETA(DisplayName = "300x250 MREC")
};

UENUM(BlueprintType)
enum class EOpenMobileAdsPlatform : uint8
{
	Android,
	IOS UMETA(DisplayName = "iOS"),
	Unsupported
};

UENUM(BlueprintType)
enum class EOpenMobileAdPlacementState : uint8
{
	Disabled,
	Idle,
	Loading,
	Ready,
	Showing,
	Destroying,
	Failed,
	Hiding,
	Hidden
};

UENUM(BlueprintType)
enum class EOpenMobileAdsServiceState : uint8
{
	Uninitialized,
	Initializing,
	Ready,
	Failed,
	ShuttingDown
};

OPENMOBILEADS_API EOpenMobileAdsPlatform OpenMobileAdsGetCurrentPlatform();
