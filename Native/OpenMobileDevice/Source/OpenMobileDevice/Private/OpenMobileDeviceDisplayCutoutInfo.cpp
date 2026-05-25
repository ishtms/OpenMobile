#include "OpenMobileDeviceDisplayCutoutInfo.h"

namespace OpenMobileDeviceDisplayCutoutInfoPrivate
{
	bool Equivalent(
		const FOpenMobileDeviceRect& Left,
		const FOpenMobileDeviceRect& Right
	)
	{
		return FMath::IsNearlyEqual(Left.Left, Right.Left, 0.01f)
			&& FMath::IsNearlyEqual(Left.Top, Right.Top, 0.01f)
			&& FMath::IsNearlyEqual(Left.Right, Right.Right, 0.01f)
			&& FMath::IsNearlyEqual(Left.Bottom, Right.Bottom, 0.01f);
	}

	bool IsFinite(const FOpenMobileDeviceRect& Rect)
	{
		return FMath::IsFinite(Rect.Left)
			&& FMath::IsFinite(Rect.Top)
			&& FMath::IsFinite(Rect.Right)
			&& FMath::IsFinite(Rect.Bottom);
	}

	bool ApplyWaterfallInsets(
		FOpenMobileWindowDisplaySnapshot& Snapshot,
		const FOpenMobileDeviceDisplayCutoutEvidence& Evidence,
		float NativeUnitsPerLogicalUnit
	)
	{
		if (!Evidence.NativeWaterfallInsets.IsSet())
		{
			return true;
		}
		const FOpenMobileDeviceInsetValues& Native =
			Evidence.NativeWaterfallInsets.GetValue();
		FOpenMobileDeviceInsetValues Logical{
			Native.Left / NativeUnitsPerLogicalUnit,
			Native.Top / NativeUnitsPerLogicalUnit,
			Native.Right / NativeUnitsPerLogicalUnit,
			Native.Bottom / NativeUnitsPerLogicalUnit
		};
		const bool bValid = FMath::IsFinite(Logical.Left)
			&& FMath::IsFinite(Logical.Top)
			&& FMath::IsFinite(Logical.Right)
			&& FMath::IsFinite(Logical.Bottom)
			&& Logical.Left >= 0.0f
			&& Logical.Top >= 0.0f
			&& Logical.Right >= 0.0f
			&& Logical.Bottom >= 0.0f
			&& (!Snapshot.bLogicalWindowSizeAvailable
				|| (Logical.Left + Logical.Right
						< Snapshot.LogicalWindowSize.X
					&& Logical.Top + Logical.Bottom
						< Snapshot.LogicalWindowSize.Y));
		if (!bValid)
		{
			return false;
		}
		Snapshot.WaterfallInsets.bIsAvailable = true;
		Snapshot.WaterfallInsets.Left = Logical.Left;
		Snapshot.WaterfallInsets.Top = Logical.Top;
		Snapshot.WaterfallInsets.Right = Logical.Right;
		Snapshot.WaterfallInsets.Bottom = Logical.Bottom;
		return true;
	}
}

void FOpenMobileDeviceDisplayCutoutInfo::Apply(
	FOpenMobileWindowDisplaySnapshot& Snapshot,
	const FOpenMobileDeviceDisplayCutoutEvidence& Evidence
)
{
	using namespace OpenMobileDeviceDisplayCutoutInfoPrivate;
	Snapshot.bDisplayCutoutsAvailable = Evidence.bCutoutsAvailable;
	Snapshot.DisplayCutouts.Reset();
	Snapshot.WaterfallInsets = {};
	Snapshot.bDisplayCutoutDataMalformed = false;
	Snapshot.MalformedDisplayCutoutCount = 0;

	const bool bScaleValid = Evidence.NativeUnitsPerLogicalUnit.IsSet()
		&& FMath::IsFinite(Evidence.NativeUnitsPerLogicalUnit.GetValue())
		&& Evidence.NativeUnitsPerLogicalUnit.GetValue() > 0.0f;
	if (!Evidence.NativeCutouts.IsEmpty()
		&& (!Evidence.bCutoutsAvailable
			|| !Evidence.NativeWindowOrigin.IsSet()
			|| !Snapshot.bLogicalWindowSizeAvailable
			|| !bScaleValid
			|| !FMath::IsFinite(Evidence.NativeWindowOrigin->X)
			|| !FMath::IsFinite(Evidence.NativeWindowOrigin->Y)))
	{
		Snapshot.bDisplayCutoutDataMalformed = true;
		Snapshot.MalformedDisplayCutoutCount = Evidence.NativeCutouts.Num();
	}
	else if (!Evidence.NativeCutouts.IsEmpty())
	{
		const FVector2D Origin = Evidence.NativeWindowOrigin.GetValue();
		const float Scale = Evidence.NativeUnitsPerLogicalUnit.GetValue();
		for (const FOpenMobileDeviceRect& Native : Evidence.NativeCutouts)
		{
			bool bMalformed = !IsFinite(Native)
				|| Native.Right <= Native.Left
				|| Native.Bottom <= Native.Top;
			if (bMalformed)
			{
				++Snapshot.MalformedDisplayCutoutCount;
				continue;
			}
			FOpenMobileDeviceRect Logical;
			Logical.Left = (Native.Left - Origin.X) / Scale;
			Logical.Top = (Native.Top - Origin.Y) / Scale;
			Logical.Right = (Native.Right - Origin.X) / Scale;
			Logical.Bottom = (Native.Bottom - Origin.Y) / Scale;
			if (!IsFinite(Logical))
			{
				++Snapshot.MalformedDisplayCutoutCount;
				continue;
			}
			FOpenMobileDeviceRect Clipped;
			Clipped.Left = FMath::Clamp(
				Logical.Left,
				0.0f,
				static_cast<float>(Snapshot.LogicalWindowSize.X)
			);
			Clipped.Top = FMath::Clamp(
				Logical.Top,
				0.0f,
				static_cast<float>(Snapshot.LogicalWindowSize.Y)
			);
			Clipped.Right = FMath::Clamp(
				Logical.Right,
				0.0f,
				static_cast<float>(Snapshot.LogicalWindowSize.X)
			);
			Clipped.Bottom = FMath::Clamp(
				Logical.Bottom,
				0.0f,
				static_cast<float>(Snapshot.LogicalWindowSize.Y)
			);
			bMalformed = !(Clipped == Logical);
			if (Clipped.Right <= Clipped.Left
				|| Clipped.Bottom <= Clipped.Top)
			{
				bMalformed = true;
			}
			if (bMalformed)
			{
				++Snapshot.MalformedDisplayCutoutCount;
			}
			if (Clipped.Right <= Clipped.Left
				|| Clipped.Bottom <= Clipped.Top)
			{
				continue;
			}
			if (!Snapshot.DisplayCutouts.ContainsByPredicate(
				[&Clipped](const FOpenMobileDeviceRect& Existing)
				{
					return Equivalent(Existing, Clipped);
				}
			))
			{
				Snapshot.DisplayCutouts.Add(Clipped);
			}
		}
		Snapshot.DisplayCutouts.Sort([](
			const FOpenMobileDeviceRect& Left,
			const FOpenMobileDeviceRect& Right
		)
		{
			return Left.Top != Right.Top
				? Left.Top < Right.Top
				: Left.Left != Right.Left
					? Left.Left < Right.Left
					: Left.Bottom != Right.Bottom
						? Left.Bottom < Right.Bottom
						: Left.Right < Right.Right;
		});
	}

	if (Evidence.NativeWaterfallInsets.IsSet())
	{
		if (!bScaleValid || !ApplyWaterfallInsets(
			Snapshot,
			Evidence,
			Evidence.NativeUnitsPerLogicalUnit.GetValue()
		))
		{
			Snapshot.bDisplayCutoutDataMalformed = true;
		}
	}
	Snapshot.bDisplayCutoutDataMalformed |=
		Snapshot.MalformedDisplayCutoutCount > 0;
}
