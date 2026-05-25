#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceDisplayTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileWindowOrientation : uint8
{
	Unknown,
	Portrait,
	PortraitUpsideDown,
	LandscapeLeft,
	LandscapeRight
};

UENUM(BlueprintType)
enum class EOpenMobileWindowMode : uint8
{
	Unknown,
	FullScreen,
	Split,
	Floating,
	Freeform
};

UENUM(BlueprintType)
enum class EOpenMobileFoldablePosture : uint8
{
	Unknown,
	Flat,
	HalfOpened,
	Tabletop,
	Book
};

UENUM(BlueprintType)
enum class EOpenMobileHdrType : uint8
{
	Unknown,
	HDR10,
	HDR10Plus,
	HLG,
	DolbyVision,
	Other
};

UENUM(BlueprintType)
enum class EOpenMobileSystemAppearance : uint8
{
	Unknown,
	Light,
	Dark
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileDeviceRect
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	float Left = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	float Top = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	float Right = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	float Bottom = 0.0f;

	bool operator==(const FOpenMobileDeviceRect& Other) const
	{
		return Left == Other.Left
			&& Top == Other.Top
			&& Right == Other.Right
			&& Bottom == Other.Bottom;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileDeviceInsets
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bIsAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	float Left = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	float Top = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	float Right = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	float Bottom = 0.0f;

	bool operator==(const FOpenMobileDeviceInsets& Other) const
	{
		return bIsAvailable == Other.bIsAvailable
			&& Left == Other.Left
			&& Top == Other.Top
			&& Right == Other.Right
			&& Bottom == Other.Bottom;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileDisplayRefreshMode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FIntPoint PixelSize = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	TArray<float> RefreshRatesHz;

	bool operator==(const FOpenMobileDisplayRefreshMode& Other) const
	{
		return PixelSize == Other.PixelSize
			&& RefreshRatesHz == Other.RefreshRatesHz;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileWindowDisplaySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bLogicalWindowSizeAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FVector2D LogicalWindowSize = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bDrawablePixelSizeAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FIntPoint DrawablePixelSize = FIntPoint::ZeroValue;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalFloat ScaleFactor;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalFloat DensityDpi;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString CurrentScreenIdentifier;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bIsWindowed;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalFloat CurrentRefreshRateHz;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalFloat MaximumRefreshRateHz;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bSupportedRefreshRatesAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	TArray<float> SupportedRefreshRatesHz;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bVariableRefreshRateSupported;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bSupportedRefreshModesAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	TArray<FOpenMobileDisplayRefreshMode> SupportedRefreshModes;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceInsets SafeAreaInsets;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceInsets SystemBarInsets;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceInsets HomeIndicatorInsets;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceInsets SystemGestureInsets;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bDisplayCutoutsAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	TArray<FOpenMobileDeviceRect> DisplayCutouts;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceInsets WaterfallInsets;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bDisplayCutoutDataMalformed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	int32 MalformedDisplayCutoutCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileWindowOrientation Orientation = EOpenMobileWindowOrientation::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileWindowMode WindowMode = EOpenMobileWindowMode::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileFoldablePosture FoldablePosture = EOpenMobileFoldablePosture::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bHingeBoundsAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceRect HingeBounds;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bFoldSeparatesContent;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bHdrAvailable;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bSupportedHdrTypesAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	TArray<EOpenMobileHdrType> SupportedHdrTypes;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bWideColorAvailable;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bHdrOutputActive;

	bool operator==(const FOpenMobileWindowDisplaySnapshot& Other) const
	{
		return Metadata == Other.Metadata
			&& bLogicalWindowSizeAvailable == Other.bLogicalWindowSizeAvailable
			&& LogicalWindowSize == Other.LogicalWindowSize
			&& bDrawablePixelSizeAvailable == Other.bDrawablePixelSizeAvailable
			&& DrawablePixelSize == Other.DrawablePixelSize
			&& ScaleFactor == Other.ScaleFactor
			&& DensityDpi == Other.DensityDpi
			&& CurrentScreenIdentifier == Other.CurrentScreenIdentifier
			&& bIsWindowed == Other.bIsWindowed
			&& CurrentRefreshRateHz == Other.CurrentRefreshRateHz
			&& MaximumRefreshRateHz == Other.MaximumRefreshRateHz
			&& bSupportedRefreshRatesAvailable == Other.bSupportedRefreshRatesAvailable
			&& SupportedRefreshRatesHz == Other.SupportedRefreshRatesHz
			&& bVariableRefreshRateSupported == Other.bVariableRefreshRateSupported
			&& bSupportedRefreshModesAvailable == Other.bSupportedRefreshModesAvailable
			&& SupportedRefreshModes == Other.SupportedRefreshModes
			&& SafeAreaInsets == Other.SafeAreaInsets
			&& SystemBarInsets == Other.SystemBarInsets
			&& HomeIndicatorInsets == Other.HomeIndicatorInsets
			&& SystemGestureInsets == Other.SystemGestureInsets
			&& bDisplayCutoutsAvailable == Other.bDisplayCutoutsAvailable
			&& DisplayCutouts == Other.DisplayCutouts
			&& WaterfallInsets == Other.WaterfallInsets
			&& bDisplayCutoutDataMalformed == Other.bDisplayCutoutDataMalformed
			&& MalformedDisplayCutoutCount
				== Other.MalformedDisplayCutoutCount
			&& Orientation == Other.Orientation
			&& WindowMode == Other.WindowMode
			&& FoldablePosture == Other.FoldablePosture
			&& bHingeBoundsAvailable == Other.bHingeBoundsAvailable
			&& HingeBounds == Other.HingeBounds
			&& bFoldSeparatesContent == Other.bFoldSeparatesContent
			&& bHdrAvailable == Other.bHdrAvailable
			&& bSupportedHdrTypesAvailable == Other.bSupportedHdrTypesAvailable
			&& SupportedHdrTypes == Other.SupportedHdrTypes
			&& bWideColorAvailable == Other.bWideColorAvailable
			&& bHdrOutputActive == Other.bHdrOutputActive;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileAppearanceSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileSystemAppearance Appearance = EOpenMobileSystemAppearance::Unknown;

	bool operator==(const FOpenMobileAppearanceSnapshot& Other) const
	{
		return Metadata == Other.Metadata && Appearance == Other.Appearance;
	}
};
