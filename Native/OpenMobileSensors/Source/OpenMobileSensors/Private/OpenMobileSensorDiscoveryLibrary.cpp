#include "OpenMobileSensorDiscoveryLibrary.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenMobileSensorsSubsystem.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorDiscoveryPrivate
{
	template <typename EnumType>
	bool IsValidOptionEnum(EnumType Value)
	{
		return StaticEnum<EnumType>()->IsValidEnumValue(
			static_cast<int64>(Value));
	}

	bool IsFiniteInRange(double Value, double Minimum, double Maximum)
	{
		return FMath::IsFinite(Value)
			&& Value >= Minimum
			&& Value <= Maximum;
	}

	bool AreFilterOptionsValid(
		const FOpenMobileSensorFilterOptions& Options)
	{
		return (!Options.bEnableLowPass
				|| IsFiniteInRange(
					Options.LowPassTimeConstantSeconds, 0.0001, 60.0))
			&& (!Options.bEnableHighPass
				|| IsFiniteInRange(
					Options.HighPassTimeConstantSeconds, 0.0001, 60.0))
			&& (!Options.bEnableExponentialSmoothing
				|| IsFiniteInRange(
					Options.SmoothingTimeConstantSeconds, 0.0001, 60.0))
			&& FMath::IsFinite(Options.DeadZone)
			&& Options.DeadZone >= 0.0;
	}

	bool AreShakeOptionsValid(
		const FOpenMobileShakeDetectionOptions& Options)
	{
		return IsFiniteInRange(
				Options.StrengthThresholdMetresPerSecondSquared,
				0.1, 1000.0)
			&& Options.MinimumImpulses >= 1
			&& Options.MinimumImpulses <= 32
			&& IsFiniteInRange(Options.DurationWindowSeconds, 0.01, 10.0)
			&& IsFiniteInRange(Options.QuietResetSeconds, 0.0, 5.0)
			&& IsFiniteInRange(Options.CooldownSeconds, 0.0, 60.0);
	}

	void AddOptionIssue(
		TArray<FOpenMobileSensorOptionIssue>& Issues,
		FName Field,
		EOpenMobileSensorOptionIssueSeverity Severity,
		const TCHAR* Message,
		const TCHAR* Correction)
	{
		FOpenMobileSensorOptionIssue& Issue = Issues.AddDefaulted_GetRef();
		Issue.Field = Field;
		Issue.Severity = Severity;
		Issue.Message = FText::FromString(Message);
		Issue.Correction = FText::FromString(Correction);
	}

	void AddApplicabilityIssues(
		EOpenMobileSensorType Sensor,
		const FOpenMobileSensorStreamOptions& Options,
		TArray<FOpenMobileSensorOptionIssue>& Issues)
	{
		const bool bCustomRate =
			Options.RatePreset == EOpenMobileSensorRatePreset::Custom;
		const bool bEventDelivery =
			Options.DeliveryMode == EOpenMobileSensorDeliveryMode::EventBatches;
		const bool bBufferedDelivery =
			Options.DeliveryMode == EOpenMobileSensorDeliveryMode::Buffered;
		const EOpenMobileSensorSampleFamily Family =
			FOpenMobileSensorTypes::GetSampleFamily(Sensor);
		const auto WarningOrError = [](bool bApplies)
		{
			return bApplies
				? EOpenMobileSensorOptionIssueSeverity::Error
				: EOpenMobileSensorOptionIssueSeverity::Warning;
		};
		if (!IsFiniteInRange(Options.CustomFrequencyHz, 1.0, 1000.0))
		{
			AddOptionIssue(Issues, TEXT("CustomFrequencyHz"),
				WarningOrError(bCustomRate),
				TEXT("Custom Frequency must be between 1 and 1000 Hz."),
				bCustomRate
					? TEXT("Choose a frequency from 1 to 1000 Hz.")
					: TEXT("This field is ignored by the selected preset and will use a safe value."));
		}
		if (!IsFiniteInRange(
			Options.MaximumDeliveryLatencySeconds, 0.0, 10.0))
		{
			AddOptionIssue(Issues, TEXT("MaximumDeliveryLatencySeconds"),
				WarningOrError(bCustomRate),
				TEXT("Maximum Delivery Latency must be between 0 and 10 seconds."),
				bCustomRate
					? TEXT("Choose a latency from 0 to 10 seconds.")
					: TEXT("This field is ignored by the selected preset and will use a safe value."));
		}
		if (!IsFiniteInRange(
			Options.MaximumCallbackFrequencyHz, 1.0, 120.0))
		{
			const bool bApplies = bCustomRate && bEventDelivery;
			AddOptionIssue(Issues, TEXT("MaximumCallbackFrequencyHz"),
				WarningOrError(bApplies),
				TEXT("Maximum Callback Frequency must be between 1 and 120 Hz."),
				bApplies
					? TEXT("Choose an event callback frequency from 1 to 120 Hz.")
					: TEXT("This field is ignored by the selected rate or delivery mode and will use a safe value."));
		}
		if (Options.BufferCapacitySamples < 1
			|| Options.BufferCapacitySamples > 4096)
		{
			const bool bApplies = bEventDelivery || bBufferedDelivery;
			AddOptionIssue(Issues, TEXT("BufferCapacitySamples"),
				WarningOrError(bApplies),
				TEXT("Buffer Capacity must be between 1 and 4096 samples."),
				bApplies
					? TEXT("Choose a capacity from 1 to 4096 samples.")
					: TEXT("Polling does not use this field, so a safe capacity will be used."));
		}
		if (!IsValidOptionEnum(Options.OverflowPolicy))
		{
			const bool bApplies = bEventDelivery || bBufferedDelivery;
			AddOptionIssue(Issues, TEXT("OverflowPolicy"),
				WarningOrError(bApplies),
				TEXT("Overflow Policy is not a recognized value."),
				bApplies
					? TEXT("Choose Drop Oldest or Reject Newest.")
					: TEXT("Polling does not use this field, so Drop Oldest will be used."));
		}
		const bool bActivity = Sensor == EOpenMobileSensorType::MotionActivity
			|| Sensor == EOpenMobileSensorType::ActivityTransition;
		if (!IsValidOptionEnum(Options.MinimumActivityConfidence))
		{
			AddOptionIssue(Issues, TEXT("MinimumActivityConfidence"),
				WarningOrError(bActivity),
				TEXT("Minimum Activity Confidence is not a recognized value."),
				bActivity
					? TEXT("Choose Unknown, Low, Medium, or High.")
					: TEXT("This sensor ignores activity confidence and will use Unknown."));
		}
		if (!IsFiniteInRange(
			Options.MinimumActivityStableDurationSeconds, 0.0, 3600.0))
		{
			AddOptionIssue(Issues,
				TEXT("MinimumActivityStableDurationSeconds"),
				WarningOrError(bActivity),
				TEXT("Activity Stable Duration must be between 0 and 3600 seconds."),
				bActivity
					? TEXT("Choose a duration from 0 to 3600 seconds.")
					: TEXT("This sensor ignores activity stability and will use zero."));
		}
		const bool bFilterable = Family == EOpenMobileSensorSampleFamily::Vector
			|| Family == EOpenMobileSensorSampleFamily::Heading;
		if (!AreFilterOptionsValid(Options.Filters))
		{
			AddOptionIssue(Issues, TEXT("Filters"),
				WarningOrError(bFilterable),
				TEXT("One or more enabled filter values are outside their valid range."),
				bFilterable
					? TEXT("Use positive time constants up to 60 seconds and a nonnegative dead zone.")
					: TEXT("This sensor ignores vector filters and will use safe defaults."));
		}
		if (!AreShakeOptionsValid(Options.ShakeDetection))
		{
			const bool bShake = Sensor == EOpenMobileSensorType::Shake;
			AddOptionIssue(Issues, TEXT("ShakeDetection"),
				WarningOrError(bShake),
				TEXT("One or more shake-detection values are outside their valid range."),
				bShake
					? TEXT("Use the documented threshold, impulse, duration, reset, and cooldown ranges.")
					: TEXT("This sensor ignores shake detection and will use safe defaults."));
		}
		const int32 AllowedRepresentations =
			static_cast<int32>(EOpenMobileAttitudeRepresentation::Quaternion)
			| static_cast<int32>(EOpenMobileAttitudeRepresentation::EulerAngles)
			| static_cast<int32>(EOpenMobileAttitudeRepresentation::RotationMatrix);
		const bool bAttitude = Sensor == EOpenMobileSensorType::Attitude;
		if (!IsValidOptionEnum(Options.AttitudeReferenceFrame))
		{
			AddOptionIssue(Issues, TEXT("AttitudeReferenceFrame"),
				WarningOrError(bAttitude),
				TEXT("Attitude Reference Frame is not a recognized value."),
				bAttitude
					? TEXT("Choose a supported attitude reference frame.")
					: TEXT("This sensor ignores attitude reference and will use Game Relative."));
		}
		if (Options.AttitudeRepresentations == 0
			|| (Options.AttitudeRepresentations & ~AllowedRepresentations) != 0)
		{
			AddOptionIssue(Issues, TEXT("AttitudeRepresentations"),
				WarningOrError(bAttitude),
				TEXT("Attitude Representations must contain at least one recognized output."),
				bAttitude
					? TEXT("Select Quaternion, Euler Angles, or Rotation Matrix.")
					: TEXT("This sensor ignores attitude output selection and will use Quaternion."));
		}
		const bool bScalarEvent = bEventDelivery
			&& Family == EOpenMobileSensorSampleFamily::Scalar;
		if (!FMath::IsFinite(Options.MinimumScalarEventChange)
			|| Options.MinimumScalarEventChange < 0.0)
		{
			AddOptionIssue(Issues, TEXT("MinimumScalarEventChange"),
				WarningOrError(bScalarEvent),
				TEXT("Minimum Scalar Event Change must be finite and nonnegative."),
				bScalarEvent
					? TEXT("Choose zero or a positive change threshold.")
					: TEXT("This request ignores the scalar event threshold and will use zero."));
		}
	}

	const FOpenMobileSensorCapability* FindCapability(
		const FOpenMobileSensorCapabilitySnapshot& Snapshot,
		EOpenMobileSensorType Sensor,
		FName InstanceId = NAME_None)
	{
		const FOpenMobileSensorCapability* First = nullptr;
		for (const FOpenMobileSensorCapability& Capability : Snapshot.Sensors)
		{
			if (Capability.Sensor.Type != Sensor)
			{
				continue;
			}
			if (!InstanceId.IsNone())
			{
				if (Capability.Sensor.InstanceId == InstanceId)
				{
					return &Capability;
				}
				continue;
			}
			if (!First)
			{
				First = &Capability;
			}
			if (Capability.Sensor.InstanceId == TEXT("Default"))
			{
				return &Capability;
			}
		}
		return First;
	}

	FText GetRequiredAction(const FOpenMobileSensorCapability& Capability)
	{
		switch (Capability.Availability.State)
		{
		case EOpenMobileCapabilityState::Available:
			return FText::GetEmpty();
		case EOpenMobileCapabilityState::PermissionRequired:
			return FText::Format(
				NSLOCTEXT("OpenMobileSensorDiscovery", "RequestPermission", "Request {0} from a user action, then retry."),
				FText::FromString(Capability.RequiredPermission.ToString()));
		case EOpenMobileCapabilityState::Denied:
			return NSLOCTEXT("OpenMobileSensorDiscovery", "PermissionDenied", "Respect the decision or direct the user to system settings when appropriate.");
		case EOpenMobileCapabilityState::Restricted:
			return NSLOCTEXT("OpenMobileSensorDiscovery", "PermissionRestricted", "Use a feature path that does not require the restricted permission.");
		case EOpenMobileCapabilityState::TemporarilyUnavailable:
			return NSLOCTEXT("OpenMobileSensorDiscovery", "TemporarilyUnavailable", "Wait for lifecycle or backend recovery, then check availability again.");
		case EOpenMobileCapabilityState::NotConfigured:
			return NSLOCTEXT("OpenMobileSensorDiscovery", "NotConfigured", "Enable the required Sensors project setting and rebuild the application.");
		case EOpenMobileCapabilityState::NotSupported:
			return NSLOCTEXT("OpenMobileSensorDiscovery", "NotSupported", "Use another sensor reported as available on this device.");
		case EOpenMobileCapabilityState::Unavailable:
		default:
			break;
		}

		switch (Capability.ActiveRestriction)
		{
		case EOpenMobileSensorRestriction::MissingInput:
			return NSLOCTEXT("OpenMobileSensorDiscovery", "MissingInput", "Provide the required input or enable an available fallback.");
		case EOpenMobileSensorRestriction::Background:
			return NSLOCTEXT("OpenMobileSensorDiscovery", "BackgroundRestricted", "Resume in the foreground or choose a supported lifecycle policy.");
		case EOpenMobileSensorRestriction::Calibration:
			return NSLOCTEXT("OpenMobileSensorDiscovery", "CalibrationRequired", "Complete sensor calibration before retrying.");
		case EOpenMobileSensorRestriction::RateLimited:
			return NSLOCTEXT("OpenMobileSensorDiscovery", "RateLimited", "Request a lower rate or retry after the reported limit clears.");
		case EOpenMobileSensorRestriction::Configuration:
			return NSLOCTEXT("OpenMobileSensorDiscovery", "ConfigurationRestricted", "Update the Sensors project settings and rebuild the application.");
		case EOpenMobileSensorRestriction::MissingHardware:
		default:
			return NSLOCTEXT("OpenMobileSensorDiscovery", "Unavailable", "Use another sensor reported as available on this device.");
		}
	}

	FText Text(const TCHAR* Value)
	{
		return FText::FromString(Value);
	}

	FOpenMobileSensorDisplayInfo MakeDisplayInfo(
		const TCHAR* Name,
		const TCHAR* Unit,
		const TCHAR* Description,
		EOpenMobileSensorSampleFamily Family)
	{
		FOpenMobileSensorDisplayInfo Info;
		Info.DisplayName = Text(Name);
		Info.Unit = Text(Unit);
		Info.Description = Text(Description);
		Info.SampleFamily = Family;
		return Info;
	}
}

UOpenMobileSensorsSubsystem*
UOpenMobileSensorDiscoveryLibrary::GetOpenMobileSensorsSubsystem(
	const UObject* WorldContextObject)
{
	if (!GEngine || !WorldContextObject)
	{
		return nullptr;
	}
	UWorld* World = GEngine->GetWorldFromContextObject(
		WorldContextObject,
		EGetWorldErrorMode::ReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UOpenMobileSensorsSubsystem>()
		: nullptr;
}

bool UOpenMobileSensorDiscoveryLibrary::IsSensorAvailable(
	const UObject* WorldContextObject,
	EOpenMobileSensorType Sensor)
{
	FOpenMobileSensorCapability Capability;
	return GetSensorAvailability(WorldContextObject, Sensor, Capability)
		&& Capability.Availability.IsAvailable();
}

bool UOpenMobileSensorDiscoveryLibrary::GetSensorAvailability(
	const UObject* WorldContextObject,
	EOpenMobileSensorType Sensor,
	FOpenMobileSensorCapability& OutCapability,
	FName InstanceId)
{
	OutCapability = {};
	const UOpenMobileSensorsSubsystem* Subsystem =
		GetOpenMobileSensorsSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		return false;
	}
	const FOpenMobileSensorCapability* Capability =
		OpenMobileSensorDiscoveryPrivate::FindCapability(
			Subsystem->GetCapabilitySnapshotNative(),
			Sensor,
			InstanceId);
	if (!Capability)
	{
		return false;
	}
	OutCapability = *Capability;
	return true;
}

bool UOpenMobileSensorDiscoveryLibrary::GetSensorAvailabilityInfo(
	const UObject* WorldContextObject,
	EOpenMobileSensorType Sensor,
	FOpenMobileSensorAvailabilityInfo& OutAvailability,
	FName InstanceId)
{
	OutAvailability = {};
	OutAvailability.Sensor.Type = Sensor;
	OutAvailability.Sensor.InstanceId = InstanceId;
	FOpenMobileSensorCapability Capability;
	if (!GetSensorAvailability(
		WorldContextObject, Sensor, Capability, InstanceId))
	{
		OutAvailability.RequiredAction = NSLOCTEXT(
			"OpenMobileSensorDiscovery",
			"SensorNotDiscovered",
			"Use another sensor reported as available on this device.");
		return false;
	}
	OutAvailability.Sensor = Capability.Sensor;
	OutAvailability.bAvailable = Capability.Availability.IsAvailable();
	OutAvailability.State = Capability.Availability.State;
	OutAvailability.Source = Capability.Source;
	OutAvailability.Restriction = Capability.ActiveRestriction;
	OutAvailability.RequiredPermission = Capability.RequiredPermission;
	OutAvailability.MinimumFrequencyHz = Capability.MinimumFrequencyHz;
	OutAvailability.MaximumFrequencyHz = Capability.MaximumFrequencyHz;
	OutAvailability.Detail = FText::FromString(Capability.Availability.Detail);
	OutAvailability.RequiredAction =
		OpenMobileSensorDiscoveryPrivate::GetRequiredAction(Capability);
	return true;
}

void UOpenMobileSensorDiscoveryLibrary::BranchSensorAvailability(
	const UObject* WorldContextObject,
	EOpenMobileSensorType Sensor,
	FOpenMobileSensorAvailabilityInfo& OutAvailability,
	EOpenMobileSensorAvailabilityBranch& Branch,
	FName InstanceId)
{
	const bool bFound = GetSensorAvailabilityInfo(
		WorldContextObject, Sensor, OutAvailability, InstanceId);
	if (!bFound)
	{
		Branch = EOpenMobileSensorAvailabilityBranch::Unavailable;
		return;
	}
	switch (OutAvailability.State)
	{
	case EOpenMobileCapabilityState::Available:
		Branch = EOpenMobileSensorAvailabilityBranch::Available;
		return;
	case EOpenMobileCapabilityState::PermissionRequired:
		Branch = EOpenMobileSensorAvailabilityBranch::PermissionRequired;
		return;
	case EOpenMobileCapabilityState::TemporarilyUnavailable:
		Branch =
			EOpenMobileSensorAvailabilityBranch::TemporarilyUnavailable;
		return;
	default:
		Branch = EOpenMobileSensorAvailabilityBranch::Unavailable;
		return;
	}
}

TArray<FOpenMobileSensorIdentifier>
UOpenMobileSensorDiscoveryLibrary::GetAvailableSensors(
	const UObject* WorldContextObject)
{
	TArray<FOpenMobileSensorIdentifier> Result;
	const UOpenMobileSensorsSubsystem* Subsystem =
		GetOpenMobileSensorsSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		return Result;
	}
	for (const FOpenMobileSensorCapability& Capability
		: Subsystem->GetCapabilitySnapshotNative().Sensors)
	{
		if (Capability.Availability.IsAvailable())
		{
			Result.Add(Capability.Sensor);
		}
	}
	return Result;
}

bool UOpenMobileSensorDiscoveryLibrary::GetPreferredSensor(
	const UObject* WorldContextObject,
	EOpenMobileSensorType Sensor,
	FOpenMobileSensorIdentifier& OutSensor)
{
	OutSensor = {};
	FOpenMobileSensorCapability Capability;
	if (!GetSensorAvailability(WorldContextObject, Sensor, Capability))
	{
		return false;
	}
	OutSensor = Capability.Sensor;
	return true;
}

bool UOpenMobileSensorDiscoveryLibrary::GetSensorMetadata(
	const UObject* WorldContextObject,
	EOpenMobileSensorType Sensor,
	FOpenMobileSensorMetadata& OutMetadata)
{
	OutMetadata = {};
	const UOpenMobileSensorsSubsystem* Subsystem =
		GetOpenMobileSensorsSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		return false;
	}
	const FOpenMobileSensorMetadata* First = nullptr;
	for (const FOpenMobileSensorMetadata& Metadata
		: Subsystem->GetMetadataNative())
	{
		if (Metadata.Sensor.Type != Sensor)
		{
			continue;
		}
		if (!First)
		{
			First = &Metadata;
		}
		if (Metadata.bPreferred
			|| Metadata.Sensor.InstanceId == TEXT("Default"))
		{
			OutMetadata = Metadata;
			return true;
		}
	}
	if (!First)
	{
		return false;
	}
	OutMetadata = *First;
	return true;
}

FOpenMobileSensorDisplayInfo
UOpenMobileSensorDiscoveryLibrary::GetSensorDisplayInfo(
	EOpenMobileSensorType Sensor)
{
	using namespace OpenMobileSensorDiscoveryPrivate;
	const EOpenMobileSensorSampleFamily Family =
		FOpenMobileSensorTypes::GetSampleFamily(Sensor);
	switch (Sensor)
	{
	case EOpenMobileSensorType::Accelerometer:
	case EOpenMobileSensorType::AccelerometerUncalibrated:
		return MakeDisplayInfo(TEXT("Accelerometer"), TEXT("m/s2"),
			TEXT("Device acceleration including gravity."), Family);
	case EOpenMobileSensorType::Gyroscope:
	case EOpenMobileSensorType::GyroscopeUncalibrated:
		return MakeDisplayInfo(TEXT("Gyroscope"), TEXT("rad/s"),
			TEXT("Angular velocity around device axes."), Family);
	case EOpenMobileSensorType::Magnetometer:
	case EOpenMobileSensorType::MagnetometerUncalibrated:
		return MakeDisplayInfo(TEXT("Magnetometer"), TEXT("uT"),
			TEXT("Magnetic field around device axes."), Family);
	case EOpenMobileSensorType::Gravity:
		return MakeDisplayInfo(TEXT("Gravity"), TEXT("m/s2"),
			TEXT("Estimated gravity vector."), Family);
	case EOpenMobileSensorType::LinearAcceleration:
		return MakeDisplayInfo(TEXT("Linear Acceleration"), TEXT("m/s2"),
			TEXT("Device acceleration with gravity removed."), Family);
	case EOpenMobileSensorType::Attitude:
		return MakeDisplayInfo(TEXT("Attitude"), TEXT("rotation"),
			TEXT("Normalized device pose."), Family);
	case EOpenMobileSensorType::MagneticHeading:
		return MakeDisplayInfo(TEXT("Magnetic Heading"), TEXT("degrees"),
			TEXT("Clockwise bearing from magnetic north."), Family);
	case EOpenMobileSensorType::TrueHeading:
		return MakeDisplayInfo(TEXT("True Heading"), TEXT("degrees"),
			TEXT("Clockwise bearing from geographic north."), Family);
	case EOpenMobileSensorType::BarometricPressure:
		return MakeDisplayInfo(TEXT("Pressure"), TEXT("hPa"),
			TEXT("Atmospheric pressure."), Family);
	case EOpenMobileSensorType::RelativeAltitude:
		return MakeDisplayInfo(TEXT("Relative Altitude"), TEXT("m"),
			TEXT("Altitude change from a session baseline."), Family);
	case EOpenMobileSensorType::AbsoluteAltitude:
		return MakeDisplayInfo(TEXT("Absolute Altitude"), TEXT("m"),
			TEXT("Platform-provided absolute altitude."), Family);
	case EOpenMobileSensorType::AmbientLight:
		return MakeDisplayInfo(TEXT("Ambient Light"), TEXT("lux"),
			TEXT("Ambient illuminance."), Family);
	case EOpenMobileSensorType::Proximity:
		return MakeDisplayInfo(TEXT("Proximity"), TEXT("near"),
			TEXT("Near state with optional distance."), Family);
	case EOpenMobileSensorType::StepCounter:
		return MakeDisplayInfo(TEXT("Step Count"), TEXT("steps"),
			TEXT("Resettable session step total."), Family);
	case EOpenMobileSensorType::StepDetector:
		return MakeDisplayInfo(TEXT("Step Events"), TEXT("steps"),
			TEXT("Newly detected step deltas."), Family);
	case EOpenMobileSensorType::Pedometer:
		return MakeDisplayInfo(TEXT("Pedometer"), TEXT("steps"),
			TEXT("Step total with optional movement metrics."), Family);
	case EOpenMobileSensorType::MotionActivity:
		return MakeDisplayInfo(TEXT("Motion Activity"), TEXT("activity"),
			TEXT("Walking, running, cycling, driving, or stationary."), Family);
	case EOpenMobileSensorType::ActivityTransition:
		return MakeDisplayInfo(TEXT("Activity Transition"), TEXT("transition"),
			TEXT("Activity start and stop transitions."), Family);
	case EOpenMobileSensorType::PhysicalOrientation:
		return MakeDisplayInfo(TEXT("Physical Orientation"), TEXT("orientation"),
			TEXT("Portrait, landscape, face up, or face down."), Family);
	case EOpenMobileSensorType::Shake:
		return MakeDisplayInfo(TEXT("Shake"), TEXT("m/s2"),
			TEXT("Detected shake strength and duration."), Family);
	default:
		return MakeDisplayInfo(TEXT("Unknown Sensor"), TEXT(""),
			TEXT("Unknown sensor type."), Family);
	}
}

FOpenMobileSensorAccessRequirement
UOpenMobileSensorDiscoveryLibrary::GetRequiredAccessForSensor(
	const UObject* WorldContextObject,
	EOpenMobileSensorType Sensor)
{
	FOpenMobileSensorAccessRequirement Result;
	if (Sensor == EOpenMobileSensorType::TrueHeading)
	{
		Result.Requirement =
			EOpenMobileSensorAccessRequirement::ExternalPrerequisite;
		Result.AccessName = TEXT("Location");
		Result.Explanation = FText::FromString(
			TEXT("True heading requires a recent, accurate location."));
		Result.Correction = FText::FromString(
			TEXT("Supply location through the owning OpenMobile location provider."));
		return Result;
	}
	FOpenMobileSensorCapability Capability;
	if (!GetSensorAvailability(WorldContextObject, Sensor, Capability)
		|| Capability.RequiredPermission.IsNone())
	{
		Result.Explanation = FText::FromString(
			TEXT("No sensor permission is required."));
		return Result;
	}
	Result.Requirement = EOpenMobileSensorAccessRequirement::SensorPermission;
	Result.AccessName = Capability.RequiredPermission;
	Result.Permission = Capability.RequiredPermission == TEXT("ActivityRecognition")
		? EOpenMobileSensorPermission::ActivityRecognition
		: EOpenMobileSensorPermission::MotionActivity;
	const FOpenMobileSensorPermissionDescriptor Descriptor =
		FOpenMobileSensorPermissions::Describe(Result.Permission);
	Result.Explanation = FText::FromString(Descriptor.Explanation);
	Result.Correction = FText::FromString(
		TEXT("Request access from a user-initiated action before starting the listener."));
	return Result;
}

FOpenMobileSensorPermissionDescriptor
UOpenMobileSensorDiscoveryLibrary::DescribeSensorPermission(
	EOpenMobileSensorPermission Permission,
	EOpenMobilePermissionStatus Status)
{
	return FOpenMobileSensorPermissions::Describe(Permission, Status);
}

FOpenMobileSensorRatePresetSettings
UOpenMobileSensorDiscoveryLibrary::GetConfiguredSensorRatePreset(
	EOpenMobileSensorRatePreset Preset)
{
	const UOpenMobileSensorsSettings* Settings =
		GetDefault<UOpenMobileSensorsSettings>();
	switch (Preset)
	{
	case EOpenMobileSensorRatePreset::UI:
		return Settings->UIPreset;
	case EOpenMobileSensorRatePreset::Game:
		return Settings->GamePreset;
	case EOpenMobileSensorRatePreset::Fast:
		return Settings->FastPreset;
	case EOpenMobileSensorRatePreset::Custom:
	default:
	{
		FOpenMobileSensorRatePresetSettings Custom;
		Custom.RequestedFrequencyHz =
			Settings->DefaultStreamOptions.CustomFrequencyHz;
		Custom.MaximumDeliveryLatencySeconds =
			Settings->DefaultStreamOptions.MaximumDeliveryLatencySeconds;
		Custom.MaximumCallbackFrequencyHz =
			Settings->DefaultStreamOptions.MaximumCallbackFrequencyHz;
		return Custom;
	}
	}
}

FOpenMobileSensorStreamOptions
UOpenMobileSensorDiscoveryLibrary::GetRecommendedSensorOptions(
	EOpenMobileSensorType Sensor)
{
	return MakeRecommendedSensorOptions(
		Sensor, EOpenMobileSensorUseCase::Interface);
}

FOpenMobileSensorStreamOptions
UOpenMobileSensorDiscoveryLibrary::MakeRecommendedSensorOptions(
	EOpenMobileSensorType Sensor,
	EOpenMobileSensorUseCase UseCase)
{
	FOpenMobileSensorStreamOptions Requested =
		GetDefault<UOpenMobileSensorsSettings>()->DefaultStreamOptions;
	switch (UseCase)
	{
	case EOpenMobileSensorUseCase::Interface:
		Requested.RatePreset = EOpenMobileSensorRatePreset::UI;
		break;
	case EOpenMobileSensorUseCase::HighResponsiveness:
		Requested.RatePreset = EOpenMobileSensorRatePreset::Fast;
		break;
	case EOpenMobileSensorUseCase::Gameplay:
	default:
		Requested.RatePreset = EOpenMobileSensorRatePreset::Game;
		break;
	}
	Requested.DeliveryMode = EOpenMobileSensorDeliveryMode::EventBatches;
	FOpenMobileSensorStreamOptions Applied;
	FOpenMobileSensorRateResolution Resolution;
	return PreviewSensorStreamOptions(
		Sensor, Requested, Applied, Resolution)
		? Applied
		: Requested;
}

bool UOpenMobileSensorDiscoveryLibrary::PreviewSensorStreamOptions(
	EOpenMobileSensorType Sensor,
	const FOpenMobileSensorStreamOptions& RequestedOptions,
	FOpenMobileSensorStreamOptions& OutAppliedOptions,
	FOpenMobileSensorRateResolution& OutRateResolution)
{
	FOpenMobileSensorIdentifier Identifier;
	Identifier.Type = Sensor;
	Identifier.InstanceId = TEXT("Default");
	return FOpenMobileSensorsSubscriptionService::PreviewOptions(
		Identifier,
		RequestedOptions,
		OutAppliedOptions,
		OutRateResolution);
}

void UOpenMobileSensorDiscoveryLibrary::ValidateSensorOptions(
	EOpenMobileSensorType Sensor,
	const FOpenMobileSensorStreamOptions& RequestedOptions,
	EOpenMobileSensorOptionsValidationOutcome& Outcome,
	FOpenMobileSensorStreamOptions& OutAppliedOptions,
	FOpenMobileSensorRateResolution& OutRateResolution,
	TArray<FOpenMobileSensorOptionIssue>& OutIssues)
{
	OutAppliedOptions = RequestedOptions;
	OutRateResolution = {};
	OutIssues.Reset();
	OpenMobileSensorDiscoveryPrivate::AddApplicabilityIssues(
		Sensor, RequestedOptions, OutIssues);
	const bool bValid = PreviewSensorStreamOptions(
		Sensor,
		RequestedOptions,
		OutAppliedOptions,
		OutRateResolution);
	if (!bValid)
	{
		Outcome = EOpenMobileSensorOptionsValidationOutcome::Invalid;
		const bool bHasError = OutIssues.ContainsByPredicate(
			[](const FOpenMobileSensorOptionIssue& Issue)
			{
				return Issue.Severity ==
					EOpenMobileSensorOptionIssueSeverity::Error;
			});
		if (!bHasError)
		{
			OpenMobileSensorDiscoveryPrivate::AddOptionIssue(
				OutIssues,
				TEXT("Options"),
				EOpenMobileSensorOptionIssueSeverity::Error,
				TEXT("One or more active sensor options are invalid."),
				TEXT("Check the sensor, enum selections, rate, latency, and enabled feature values."));
		}
		return;
	}
	Outcome = OutIssues.IsEmpty()
		? EOpenMobileSensorOptionsValidationOutcome::Valid
		: EOpenMobileSensorOptionsValidationOutcome::Adjusted;
}
