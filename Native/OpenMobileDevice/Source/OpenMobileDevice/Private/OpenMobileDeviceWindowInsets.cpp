#include "OpenMobileDeviceWindowInsets.h"

namespace OpenMobileDeviceWindowInsetsPrivate
{
	FOpenMobileDeviceInsets Normalize(
		const TOptional<FOpenMobileDeviceInsetValues>& Evidence,
		const FOpenMobileWindowDisplaySnapshot& Snapshot
	)
	{
		FOpenMobileDeviceInsets Insets;
		if (!Evidence.IsSet())
		{
			return Insets;
		}
		const FOpenMobileDeviceInsetValues& Values = Evidence.GetValue();
		if (!FMath::IsFinite(Values.Left)
			|| !FMath::IsFinite(Values.Top)
			|| !FMath::IsFinite(Values.Right)
			|| !FMath::IsFinite(Values.Bottom)
			|| Values.Left < 0.0f
			|| Values.Top < 0.0f
			|| Values.Right < 0.0f
			|| Values.Bottom < 0.0f)
		{
			return Insets;
		}
		if (Snapshot.bLogicalWindowSizeAvailable
			&& (Values.Left + Values.Right >= Snapshot.LogicalWindowSize.X
				|| Values.Top + Values.Bottom >= Snapshot.LogicalWindowSize.Y))
		{
			return Insets;
		}
		Insets.bIsAvailable = true;
		Insets.Left = Values.Left;
		Insets.Top = Values.Top;
		Insets.Right = Values.Right;
		Insets.Bottom = Values.Bottom;
		return Insets;
	}
}

void FOpenMobileDeviceWindowInsets::Apply(
	FOpenMobileWindowDisplaySnapshot& Snapshot,
	const FOpenMobileDeviceWindowInsetsEvidence& Evidence
)
{
	using namespace OpenMobileDeviceWindowInsetsPrivate;
	Snapshot.SafeAreaInsets = Normalize(Evidence.SafeArea, Snapshot);
	Snapshot.SystemBarInsets = Normalize(Evidence.SystemBars, Snapshot);
	Snapshot.HomeIndicatorInsets = Normalize(Evidence.HomeIndicator, Snapshot);
	Snapshot.SystemGestureInsets = Normalize(Evidence.SystemGestures, Snapshot);
	Snapshot.bUsableWindowBoundsAvailable = false;
	Snapshot.UsableWindowBounds = {};
	if (Snapshot.bLogicalWindowSizeAvailable
		&& Snapshot.SafeAreaInsets.bIsAvailable)
	{
		Snapshot.bUsableWindowBoundsAvailable = true;
		Snapshot.UsableWindowBounds.Left = Snapshot.SafeAreaInsets.Left;
		Snapshot.UsableWindowBounds.Top = Snapshot.SafeAreaInsets.Top;
		Snapshot.UsableWindowBounds.Right =
			Snapshot.LogicalWindowSize.X - Snapshot.SafeAreaInsets.Right;
		Snapshot.UsableWindowBounds.Bottom =
			Snapshot.LogicalWindowSize.Y - Snapshot.SafeAreaInsets.Bottom;
	}
}
