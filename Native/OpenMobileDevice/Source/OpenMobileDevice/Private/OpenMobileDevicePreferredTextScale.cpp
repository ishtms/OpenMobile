#include "OpenMobileDevicePreferredTextScale.h"

namespace
{
	bool IsValidRelativeScale(float Scale)
	{
		return FMath::IsFinite(Scale) && Scale > 0.0f;
	}
}

FOpenMobileAccessibilitySnapshot
FOpenMobileDevicePreferredTextScale::FromAndroidFontScale(float FontScale)
{
	FOpenMobileAccessibilitySnapshot Snapshot;
	if (IsValidRelativeScale(FontScale))
	{
		Snapshot.PreferredTextScale =
			FOpenMobileDeviceOptionalFloat::MakeAvailable(FontScale);
	}
	return Snapshot;
}

FOpenMobileAccessibilitySnapshot
FOpenMobileDevicePreferredTextScale::FromIOSContentSizeCategory(
	FString ContentSizeCategory,
	float RelativeScale
)
{
	FOpenMobileAccessibilitySnapshot Snapshot;
	if (ContentSizeCategory.IsEmpty())
	{
		return Snapshot;
	}
	Snapshot.ContentSizeCategory =
		FOpenMobileDeviceOptionalString::MakeAvailable(
			MoveTemp(ContentSizeCategory)
		);
	if (IsValidRelativeScale(RelativeScale))
	{
		Snapshot.PreferredTextScale =
			FOpenMobileDeviceOptionalFloat::MakeAvailable(RelativeScale);
	}
	return Snapshot;
}
