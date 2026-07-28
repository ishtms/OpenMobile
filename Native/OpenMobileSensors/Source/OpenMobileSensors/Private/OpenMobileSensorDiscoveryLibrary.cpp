#include "OpenMobileSensorDiscoveryLibrary.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OpenMobileSensorsSubsystem.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorDiscoveryPrivate
{
	const FOpenMobileSensorCapability* FindPreferredCapability(
		const FOpenMobileSensorCapabilitySnapshot& Snapshot,
		EOpenMobileSensorType Sensor)
	{
		const FOpenMobileSensorCapability* First = nullptr;
		for (const FOpenMobileSensorCapability& Capability : Snapshot.Sensors)
		{
			if (Capability.Sensor.Type != Sensor)
			{
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
	FOpenMobileSensorCapability& OutCapability)
{
	OutCapability = {};
	const UOpenMobileSensorsSubsystem* Subsystem =
		GetOpenMobileSensorsSubsystem(WorldContextObject);
	if (!Subsystem)
	{
		return false;
	}
	const FOpenMobileSensorCapability* Capability =
		OpenMobileSensorDiscoveryPrivate::FindPreferredCapability(
			Subsystem->GetCapabilitySnapshotNative(),
			Sensor);
	if (!Capability)
	{
		return false;
	}
	OutCapability = *Capability;
	return true;
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
	FOpenMobileSensorStreamOptions Requested =
		GetDefault<UOpenMobileSensorsSettings>()->DefaultStreamOptions;
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
