#include "OpenMobileDeviceWindowMetrics.h"

FOpenMobileWindowDisplaySnapshot FOpenMobileDeviceWindowMetrics::Build(
	const FOpenMobileDeviceWindowMetricsEvidence& Evidence
)
{
	FOpenMobileWindowDisplaySnapshot Snapshot;
	if (Evidence.LogicalWindowSize.IsSet())
	{
		const FVector2D Size = Evidence.LogicalWindowSize.GetValue();
		if (FMath::IsFinite(Size.X) && FMath::IsFinite(Size.Y)
			&& Size.X > 0.0 && Size.Y > 0.0)
		{
			Snapshot.bLogicalWindowSizeAvailable = true;
			Snapshot.LogicalWindowSize = Size;
		}
	}
	if (Evidence.DrawablePixelSize.IsSet())
	{
		const FIntPoint Size = Evidence.DrawablePixelSize.GetValue();
		if (Size.X > 0 && Size.Y > 0)
		{
			Snapshot.bDrawablePixelSizeAvailable = true;
			Snapshot.DrawablePixelSize = Size;
		}
	}
	if (Evidence.ScaleFactor.IsSet()
		&& FMath::IsFinite(Evidence.ScaleFactor.GetValue())
		&& Evidence.ScaleFactor.GetValue() > 0.0f)
	{
		Snapshot.ScaleFactor = FOpenMobileDeviceOptionalFloat::MakeAvailable(
			Evidence.ScaleFactor.GetValue()
		);
	}
	if (Evidence.DensityDpi.IsSet()
		&& FMath::IsFinite(Evidence.DensityDpi.GetValue())
		&& Evidence.DensityDpi.GetValue() > 0.0f)
	{
		Snapshot.DensityDpi = FOpenMobileDeviceOptionalFloat::MakeAvailable(
			Evidence.DensityDpi.GetValue()
		);
	}
	if (Evidence.ScreenIdentifier.IsSet())
	{
		FString Identifier = Evidence.ScreenIdentifier.GetValue();
		Identifier.TrimStartAndEndInline();
		if (!Identifier.IsEmpty())
		{
			Snapshot.CurrentScreenIdentifier =
				FOpenMobileDeviceOptionalString::MakeAvailable(
					MoveTemp(Identifier)
				);
		}
	}
	if (Evidence.bIsWindowed.IsSet())
	{
		Snapshot.bIsWindowed = FOpenMobileDeviceOptionalBool::MakeAvailable(
			Evidence.bIsWindowed.GetValue()
		);
	}
	return Snapshot;
}
