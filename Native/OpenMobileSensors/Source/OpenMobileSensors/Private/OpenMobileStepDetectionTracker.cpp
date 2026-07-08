#include "OpenMobileStepDetectionTracker.h"

void FOpenMobileStepDetectionTracker::Initialize(
	const FGuid& InSessionIdentifier
)
{
	SessionIdentifier = InSessionIdentifier;
	PedometerOriginIdentifier.Invalidate();
	SessionCount = 0;
	LastPedometerTotal = 0;
	LastDirectTimestampSeconds = 0.0;
	LastPedometerQueryEnd = 0.0;
	bCountSaturated = false;
	bHasDirectTimestamp = false;
	bHasPedometerState = false;
}

bool FOpenMobileStepDetectionTracker::Process(
	const FOpenMobileStepsSensorSample& Input,
	FOpenMobileStepsSensorSample& OutEvent
)
{
	if (!SessionIdentifier.IsValid()
		|| Input.Header.Sensor.Type != EOpenMobileSensorType::StepDetector
		|| !Input.Header.bValid
		|| !FMath::IsFinite(Input.Header.TimestampSeconds)
		|| Input.Header.TimestampSeconds < 0.0)
	{
		return false;
	}

	int64 Delta = 0;
	if (Input.DetectionSource ==
		EOpenMobileStepDetectionSource::AndroidStepDetector)
	{
		Delta = Input.DetectedStepDelta;
		if (Delta <= 0
			|| (bHasDirectTimestamp
				&& Input.Header.TimestampSeconds <=
					LastDirectTimestampSeconds))
		{
			return false;
		}
		LastDirectTimestampSeconds = Input.Header.TimestampSeconds;
		bHasDirectTimestamp = true;
	}
	else if (Input.DetectionSource ==
		EOpenMobileStepDetectionSource::IOSPedometerDelta)
	{
		if (!Input.bHasNativeTotal
			|| Input.NativeTotal < 0
			|| !Input.OriginIdentifier.IsValid()
			|| !Input.bHasQueryInterval
			|| !FMath::IsFinite(Input.QueryStartUnixTimeSeconds)
			|| !FMath::IsFinite(Input.QueryEndUnixTimeSeconds)
			|| Input.QueryStartUnixTimeSeconds < 0.0
			|| Input.QueryEndUnixTimeSeconds <
				Input.QueryStartUnixTimeSeconds)
		{
			return false;
		}

		if (!bHasPedometerState)
		{
			Delta = Input.NativeTotal;
		}
		else if (PedometerOriginIdentifier == Input.OriginIdentifier)
		{
			if (Input.NativeTotal <= LastPedometerTotal)
			{
				LastPedometerTotal = Input.NativeTotal;
				LastPedometerQueryEnd = FMath::Max(
					LastPedometerQueryEnd,
					Input.QueryEndUnixTimeSeconds
				);
				return false;
			}
			Delta = Input.NativeTotal - LastPedometerTotal;
		}
		else
		{
			const bool bOverlapsPreviousInterval =
				Input.QueryStartUnixTimeSeconds < LastPedometerQueryEnd;
			Delta = bOverlapsPreviousInterval ? 0 : Input.NativeTotal;
		}

		PedometerOriginIdentifier = Input.OriginIdentifier;
		LastPedometerTotal = Input.NativeTotal;
		LastPedometerQueryEnd = Input.QueryEndUnixTimeSeconds;
		bHasPedometerState = true;
		if (Delta <= 0)
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	bool bSaturatedByThisEvent = false;
	SessionCount = SaturatingAdd(
		SessionCount,
		Delta,
		bSaturatedByThisEvent
	);
	bCountSaturated |= bSaturatedByThisEvent;
	OutEvent = Input;
	OutEvent.Count = SessionCount;
	OutEvent.Origin = EOpenMobileStepCountOrigin::Session;
	OutEvent.OriginIdentifier = SessionIdentifier;
	OutEvent.DetectedStepDelta = Delta;
	OutEvent.bCountSaturated = bCountSaturated;
	if (Input.DetectionSource ==
		EOpenMobileStepDetectionSource::IOSPedometerDelta)
	{
		const int32 OverlayFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::Mock
		) | static_cast<int32>(EOpenMobileSensorSourceFlags::Replay);
		OutEvent.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::PluginDerived
		) | (Input.Header.SourceFlags & OverlayFlags);
	}
	return true;
}

int64 FOpenMobileStepDetectionTracker::SaturatingAdd(
	int64 Left,
	int64 Right,
	bool& bOutSaturated
)
{
	const int64 Maximum = TNumericLimits<int64>::Max();
	bOutSaturated = Right > Maximum - Left;
	return bOutSaturated ? Maximum : Left + Right;
}
