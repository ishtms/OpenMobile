#include "OpenMobileDeviceFoldableInfo.h"

namespace OpenMobileDeviceFoldableInfoPrivate
{
	EOpenMobileFoldablePosture NormalizePosture(
		EOpenMobileDeviceNativeFoldState State,
		EOpenMobileDeviceNativeFoldOrientation Orientation
	)
	{
		if (State == EOpenMobileDeviceNativeFoldState::Flat)
		{
			return EOpenMobileFoldablePosture::Flat;
		}
		if (State != EOpenMobileDeviceNativeFoldState::HalfOpened)
		{
			return EOpenMobileFoldablePosture::Unknown;
		}
		switch (Orientation)
		{
		case EOpenMobileDeviceNativeFoldOrientation::Horizontal:
			return EOpenMobileFoldablePosture::Tabletop;
		case EOpenMobileDeviceNativeFoldOrientation::Vertical:
			return EOpenMobileFoldablePosture::Book;
		case EOpenMobileDeviceNativeFoldOrientation::Unknown:
			return EOpenMobileFoldablePosture::HalfOpened;
		}
		return EOpenMobileFoldablePosture::Unknown;
	}

	bool NormalizeBounds(
		const FOpenMobileDeviceFoldableEvidence& Evidence,
		const FOpenMobileWindowDisplaySnapshot& Snapshot,
		FOpenMobileDeviceRect& OutBounds
	)
	{
		if (!Evidence.NativeBounds.IsSet()
			|| !Snapshot.bLogicalWindowSizeAvailable
			|| !FMath::IsFinite(Evidence.NativeUnitsPerLogicalUnit)
			|| Evidence.NativeUnitsPerLogicalUnit <= 0.0f)
		{
			return false;
		}
		const FOpenMobileDeviceRect& Native = Evidence.NativeBounds.GetValue();
		if (!FMath::IsFinite(Native.Left)
			|| !FMath::IsFinite(Native.Top)
			|| !FMath::IsFinite(Native.Right)
			|| !FMath::IsFinite(Native.Bottom)
			|| Native.Right < Native.Left
			|| Native.Bottom < Native.Top
			|| (Native.Right == Native.Left
				&& Native.Bottom == Native.Top))
		{
			return false;
		}
		const float Scale = Evidence.NativeUnitsPerLogicalUnit;
		FOpenMobileDeviceRect Logical{
			Native.Left / Scale,
			Native.Top / Scale,
			Native.Right / Scale,
			Native.Bottom / Scale
		};
		const float Width = static_cast<float>(Snapshot.LogicalWindowSize.X);
		const float Height = static_cast<float>(Snapshot.LogicalWindowSize.Y);
		if (Logical.Right < 0.0f || Logical.Bottom < 0.0f
			|| Logical.Left > Width || Logical.Top > Height)
		{
			return false;
		}
		Logical.Left = FMath::Clamp(Logical.Left, 0.0f, Width);
		Logical.Top = FMath::Clamp(Logical.Top, 0.0f, Height);
		Logical.Right = FMath::Clamp(Logical.Right, 0.0f, Width);
		Logical.Bottom = FMath::Clamp(Logical.Bottom, 0.0f, Height);
		if (Logical.Right < Logical.Left || Logical.Bottom < Logical.Top
			|| (Logical.Right == Logical.Left
				&& Logical.Bottom == Logical.Top))
		{
			return false;
		}
		OutBounds = Logical;
		return true;
	}
}

void FOpenMobileDeviceFoldableInfo::Apply(
	FOpenMobileWindowDisplaySnapshot& Snapshot,
	const FOpenMobileDeviceFoldableEvidence& Evidence
)
{
	using namespace OpenMobileDeviceFoldableInfoPrivate;
	Snapshot.FoldablePosture = EOpenMobileFoldablePosture::Unknown;
	Snapshot.bHingeBoundsAvailable = false;
	Snapshot.HingeBounds = {};
	Snapshot.bFoldSeparatesContent = {};
	if (!Evidence.bFeatureAvailable)
	{
		return;
	}
	Snapshot.FoldablePosture = NormalizePosture(
		Evidence.State,
		Evidence.Orientation
	);
	if (Evidence.bSeparating.IsSet())
	{
		Snapshot.bFoldSeparatesContent =
			FOpenMobileDeviceOptionalBool::MakeAvailable(
				Evidence.bSeparating.GetValue()
			);
	}
	Snapshot.bHingeBoundsAvailable = NormalizeBounds(
		Evidence,
		Snapshot,
		Snapshot.HingeBounds
	);
}
