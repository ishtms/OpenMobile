#include "OpenMobileSensorsSettings.h"

namespace OpenMobileSensorsSettingsPrivate
{
	bool IsFiniteInRange(double Value, double Minimum, double Maximum)
	{
		return FMath::IsFinite(Value)
			&& Value >= Minimum
			&& Value <= Maximum;
	}

	void ValidatePreset(
		const TCHAR* Name,
		const FOpenMobileSensorRatePresetSettings& Preset,
		TArray<FString>& OutErrors
	)
	{
		if (!IsFiniteInRange(Preset.RequestedFrequencyHz, 1.0, 1000.0))
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s RequestedFrequencyHz must be between 1 and 1000 Hz."),
				Name
			));
		}
		if (!IsFiniteInRange(
			Preset.MaximumDeliveryLatencySeconds,
			0.0,
			10.0
		))
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s MaximumDeliveryLatencySeconds must be between 0 and 10 seconds."),
				Name
			));
		}
		if (!IsFiniteInRange(
			Preset.MaximumCallbackFrequencyHz,
			1.0,
			120.0
		))
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s MaximumCallbackFrequencyHz must be between 1 and 120 Hz."),
				Name
			));
		}
		if (!StaticEnum<EOpenMobileSensorPowerIntent>()->IsValidEnumValue(
			static_cast<int64>(Preset.PowerIntent)
		))
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s PowerIntent is invalid."),
				Name
			));
		}
	}
}

bool UOpenMobileSensorsSettings::Validate(
	TArray<FString>& OutErrors,
	bool bShipping
) const
{
	using namespace OpenMobileSensorsSettingsPrivate;
	OutErrors.Reset();
	ValidatePreset(TEXT("UIPreset"), UIPreset, OutErrors);
	ValidatePreset(TEXT("GamePreset"), GamePreset, OutErrors);
	ValidatePreset(TEXT("FastPreset"), FastPreset, OutErrors);
	if (!StaticEnum<EOpenMobileSensorRatePreset>()->IsValidEnumValue(
		static_cast<int64>(DefaultStreamOptions.RatePreset)
	))
	{
		OutErrors.Add(TEXT("Default sensor rate preset is invalid."));
	}
	if (!IsFiniteInRange(
		DefaultStreamOptions.CustomFrequencyHz,
		1.0,
		1000.0
	))
	{
		OutErrors.Add(TEXT("Default CustomFrequencyHz must be between 1 and 1000 Hz."));
	}
	if (!IsFiniteInRange(
		DefaultStreamOptions.MaximumCallbackFrequencyHz,
		1.0,
		120.0
	))
	{
		OutErrors.Add(TEXT("Default MaximumCallbackFrequencyHz must be between 1 and 120 Hz."));
	}
	if (!IsFiniteInRange(
		DefaultStreamOptions.MaximumDeliveryLatencySeconds,
		0.0,
		10.0
	))
	{
		OutErrors.Add(TEXT("Default MaximumDeliveryLatencySeconds must be between 0 and 10 seconds."));
	}
	if (DefaultStreamOptions.BufferCapacitySamples < 1
		|| DefaultStreamOptions.BufferCapacitySamples > 4096)
	{
		OutErrors.Add(TEXT("Default BufferCapacitySamples must be between 1 and 4096."));
	}
	if (!FMath::IsFinite(DefaultStreamOptions.MinimumScalarEventChange)
		|| DefaultStreamOptions.MinimumScalarEventChange < 0.0)
	{
		OutErrors.Add(TEXT("Default MinimumScalarEventChange must be finite and nonnegative."));
	}
	if ((DefaultStreamOptions.RatePreset == EOpenMobileSensorRatePreset::Fast
		|| DefaultStreamOptions.CustomFrequencyHz > 200.0
		|| DefaultStreamOptions.bAllowHighSamplingRate)
		&& !bAllowHighSamplingRate)
	{
		OutErrors.Add(TEXT("High-rate default streaming requires the project high-rate opt-in."));
	}
	if (DefaultStreamOptions.LifecyclePolicy ==
		EOpenMobileSensorLifecyclePolicy::ContinueWhenSupported
		&& !bAllowBackgroundSensorDelivery)
	{
		OutErrors.Add(TEXT("Background continuation requires the project background-delivery opt-in."));
	}
	if (!IsFiniteInRange(
		PhysicalOrientationFaceAngleDegrees,
		5.0,
		40.0
	))
	{
		OutErrors.Add(TEXT("PhysicalOrientationFaceAngleDegrees must be between 5 and 40 degrees."));
	}
	if (!IsFiniteInRange(
		PhysicalOrientationHysteresisDegrees,
		0.0,
		15.0
	))
	{
		OutErrors.Add(TEXT("PhysicalOrientationHysteresisDegrees must be between 0 and 15 degrees."));
	}
	if (!IsFiniteInRange(
		PhysicalOrientationTransitionDebounceSeconds,
		0.0,
		2.0
	))
	{
		OutErrors.Add(TEXT("PhysicalOrientationTransitionDebounceSeconds must be between 0 and 2 seconds."));
	}
	if (DefaultStreamOptions.Filters.bEnableLowPass
		&& !IsFiniteInRange(
			DefaultStreamOptions.Filters.LowPassTimeConstantSeconds,
			0.0001,
			60.0
		))
	{
		OutErrors.Add(TEXT("Low-pass filter time constant must be finite and positive."));
	}
	if (DefaultStreamOptions.Filters.bEnableHighPass
		&& !IsFiniteInRange(
			DefaultStreamOptions.Filters.HighPassTimeConstantSeconds,
			0.0001,
			60.0
		))
	{
		OutErrors.Add(TEXT("High-pass filter time constant must be finite and positive."));
	}
	if (DefaultStreamOptions.Filters.bEnableExponentialSmoothing
		&& !IsFiniteInRange(
			DefaultStreamOptions.Filters.SmoothingTimeConstantSeconds,
			0.0001,
			60.0
		))
	{
		OutErrors.Add(TEXT("Smoothing time constant must be finite and positive."));
	}
	if (!FMath::IsFinite(DefaultStreamOptions.Filters.DeadZone)
		|| DefaultStreamOptions.Filters.DeadZone < 0.0)
	{
		OutErrors.Add(TEXT("Filter dead zone must be finite and nonnegative."));
	}
	if (!IsFiniteInRange(MaximumRecordingDurationSeconds, 1.0, 86400.0))
	{
		OutErrors.Add(TEXT("MaximumRecordingDurationSeconds must be between 1 and 86400 seconds."));
	}
	if (MaximumRecordingBytes < 1024ll * 1024
		|| MaximumRecordingBytes > 256ll * 1024 * 1024)
	{
		OutErrors.Add(TEXT("MaximumRecordingBytes must be between 1 MiB and 256 MiB."));
	}
	if (MaximumRecordingBufferedBatches < 1
		|| MaximumRecordingBufferedBatches > 4096)
	{
		OutErrors.Add(TEXT("Maximum buffered recording batches must be between 1 and 4096."));
	}
	if (bShipping
		&& bAllowSensitiveLocationContextInDevelopmentRecordings)
	{
		OutErrors.Add(TEXT("Sensitive location recording must be disabled in Shipping builds."));
	}
	if (IOSMotionUsageDescription.TrimStartAndEnd().IsEmpty())
	{
		OutErrors.Add(TEXT("IOSMotionUsageDescription is required for the iOS sensor backend."));
	}
	if (bEnablePermissionSensitiveSensors
		&& AndroidActivityRecognitionRationale.TrimStartAndEnd().IsEmpty())
	{
		OutErrors.Add(TEXT("AndroidActivityRecognitionRationale is required for permission-sensitive sensors."));
	}
	if (!StaticEnum<EOpenMobileSensorsDevelopmentInputMode>()->IsValidEnumValue(
		static_cast<int64>(DevelopmentInputMode)
	))
	{
		OutErrors.Add(TEXT("DevelopmentInputMode is invalid."));
	}
	if (DevelopmentInputMode == EOpenMobileSensorsDevelopmentInputMode::Replay
		&& DevelopmentReplayFile.TrimStartAndEnd().IsEmpty())
	{
		OutErrors.Add(TEXT("DevelopmentReplayFile is required when replay input is selected."));
	}
	if (bShipping
		&& DevelopmentInputMode !=
			EOpenMobileSensorsDevelopmentInputMode::Disabled)
	{
		OutErrors.Add(TEXT("DevelopmentInputMode must be disabled in Shipping builds."));
	}
	return OutErrors.IsEmpty();
}
