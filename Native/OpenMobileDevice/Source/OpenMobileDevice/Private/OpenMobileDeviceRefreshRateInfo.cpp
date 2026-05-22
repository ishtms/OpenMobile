#include "OpenMobileDeviceRefreshRateInfo.h"

namespace OpenMobileDeviceRefreshRateInfoPrivate
{
	bool IsValidRate(float Rate)
	{
		return FMath::IsFinite(Rate) && Rate > 0.0f && Rate <= 1000.0f;
	}

	void AddUniqueRate(TArray<float>& Rates, float Rate)
	{
		if (!IsValidRate(Rate))
		{
			return;
		}
		for (const float Existing : Rates)
		{
			if (FMath::IsNearlyEqual(Existing, Rate, 0.01f))
			{
				return;
			}
		}
		Rates.Add(Rate);
	}
}

void FOpenMobileDeviceRefreshRateInfo::Apply(
	FOpenMobileWindowDisplaySnapshot& Snapshot,
	const FOpenMobileDeviceRefreshRateEvidence& Evidence
)
{
	using namespace OpenMobileDeviceRefreshRateInfoPrivate;
	Snapshot.CurrentRefreshRateHz = {};
	Snapshot.MaximumRefreshRateHz = {};
	Snapshot.bSupportedRefreshRatesAvailable = false;
	Snapshot.SupportedRefreshRatesHz.Reset();
	Snapshot.bVariableRefreshRateSupported = {};
	Snapshot.bSupportedRefreshModesAvailable = false;
	Snapshot.SupportedRefreshModes.Reset();
	if (Evidence.CurrentRefreshRateHz.IsSet()
		&& IsValidRate(Evidence.CurrentRefreshRateHz.GetValue()))
	{
		Snapshot.CurrentRefreshRateHz =
			FOpenMobileDeviceOptionalFloat::MakeAvailable(
				Evidence.CurrentRefreshRateHz.GetValue()
			);
	}
	if (Evidence.bVariableRefreshRateSupported.IsSet())
	{
		Snapshot.bVariableRefreshRateSupported =
			FOpenMobileDeviceOptionalBool::MakeAvailable(
				Evidence.bVariableRefreshRateSupported.GetValue()
			);
	}

	Snapshot.bSupportedRefreshModesAvailable =
		Evidence.bSupportedModesAvailable;
	Snapshot.bSupportedRefreshRatesAvailable =
		Evidence.bSupportedModesAvailable;
	float DerivedMaximum = 0.0f;
	for (const FOpenMobileDeviceRefreshModeEvidence& NativeMode :
		Evidence.SupportedModes)
	{
		if (NativeMode.PixelSize.X <= 0 || NativeMode.PixelSize.Y <= 0)
		{
			continue;
		}
		FOpenMobileDisplayRefreshMode* Mode =
			Snapshot.SupportedRefreshModes.FindByPredicate(
				[&NativeMode](const FOpenMobileDisplayRefreshMode& Existing)
				{
					return Existing.PixelSize == NativeMode.PixelSize;
				}
			);
		if (!Mode)
		{
			FOpenMobileDisplayRefreshMode NewMode;
			NewMode.PixelSize = NativeMode.PixelSize;
			Mode = &Snapshot.SupportedRefreshModes.Add_GetRef(
				MoveTemp(NewMode)
			);
		}
		for (const float Rate : NativeMode.RefreshRatesHz)
		{
			AddUniqueRate(Mode->RefreshRatesHz, Rate);
			AddUniqueRate(Snapshot.SupportedRefreshRatesHz, Rate);
			if (IsValidRate(Rate))
			{
				DerivedMaximum = FMath::Max(DerivedMaximum, Rate);
			}
		}
	}
	Snapshot.SupportedRefreshModes.RemoveAll(
		[](const FOpenMobileDisplayRefreshMode& Mode)
		{
			return Mode.RefreshRatesHz.IsEmpty();
		}
	);
	for (FOpenMobileDisplayRefreshMode& Mode :
		Snapshot.SupportedRefreshModes)
	{
		Mode.RefreshRatesHz.Sort();
	}
	Snapshot.SupportedRefreshModes.Sort(
		[](const FOpenMobileDisplayRefreshMode& Left,
			const FOpenMobileDisplayRefreshMode& Right)
		{
			return Left.PixelSize.X == Right.PixelSize.X
				? Left.PixelSize.Y < Right.PixelSize.Y
				: Left.PixelSize.X < Right.PixelSize.X;
		}
	);
	Snapshot.SupportedRefreshRatesHz.Sort();

	float Maximum = DerivedMaximum;
	if (Evidence.MaximumRefreshRateHz.IsSet()
		&& IsValidRate(Evidence.MaximumRefreshRateHz.GetValue()))
	{
		Maximum = FMath::Max(
			Maximum,
			Evidence.MaximumRefreshRateHz.GetValue()
		);
	}
	if (Maximum > 0.0f)
	{
		Snapshot.MaximumRefreshRateHz =
			FOpenMobileDeviceOptionalFloat::MakeAvailable(Maximum);
	}
}
