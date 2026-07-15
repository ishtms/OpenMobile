#include "OpenMobileSensorsIOSBackend.h"

#include "OpenMobileAsync.h"
#include "OpenMobileSensorAccuracyMapper.h"
#include "OpenMobileSensorCoordinates.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorScreenRotationService.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "OpenMobileSensorTimestamp.h"
#include "OpenMobileSensorUnits.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsIOSBridge.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsIOSBackendPrivate
{
	struct FSupportedSensor
	{
		EOpenMobileSensorType Type;
		const TCHAR* NativeIdentifier;
	};

	TArray<FSupportedSensor> GetSupportedSensors(
		const FOpenMobileSensorsIOSAvailability& Availability
	)
	{
		TArray<FSupportedSensor> Sensors;
		auto Add = [&Sensors](
			bool bAvailable,
			EOpenMobileSensorType Type,
			const TCHAR* NativeIdentifier
		)
		{
			if (bAvailable)
			{
				Sensors.Add({Type, NativeIdentifier});
			}
		};
		Add(Availability.bAccelerometer, EOpenMobileSensorType::Accelerometer,
			TEXT("IOS-Accelerometer"));
		Add(Availability.bGyroscope, EOpenMobileSensorType::Gyroscope,
			TEXT("IOS-Gyroscope"));
		Add(Availability.bMagnetometer,
			EOpenMobileSensorType::MagnetometerUncalibrated,
			TEXT("IOS-MagnetometerUncalibrated"));
		Add(Availability.bDeviceMotion && Availability.bMagnetometer,
			EOpenMobileSensorType::Magnetometer,
			TEXT("IOS-Magnetometer"));
		Add(Availability.bDeviceMotion, EOpenMobileSensorType::Gravity,
			TEXT("IOS-Gravity"));
		Add(Availability.bDeviceMotion,
			EOpenMobileSensorType::LinearAcceleration,
			TEXT("IOS-LinearAcceleration"));
		Add(Availability.bDeviceMotion, EOpenMobileSensorType::Attitude,
			TEXT("IOS-Attitude"));
		Add(Availability.bDeviceMotion
				&& Availability.bMagneticNorthReference,
			EOpenMobileSensorType::MagneticHeading,
			TEXT("IOS-MagneticHeading"));
		Add(Availability.bRelativeAltitude,
			EOpenMobileSensorType::BarometricPressure,
			TEXT("IOS-BarometricPressure"));
		Add(Availability.bRelativeAltitude,
			EOpenMobileSensorType::RelativeAltitude,
			TEXT("IOS-RelativeAltitude"));
		Add(Availability.bAbsoluteAltitude,
			EOpenMobileSensorType::AbsoluteAltitude,
			TEXT("IOS-AbsoluteAltitude"));
		Add(Availability.bProximityApiSupported,
			EOpenMobileSensorType::Proximity,
			TEXT("IOS-ProximityState"));
		Add(Availability.bStepCounting,
			EOpenMobileSensorType::StepCounter,
			TEXT("IOS-Pedometer"));
		Add(Availability.bStepCounting,
			EOpenMobileSensorType::StepDetector,
			TEXT("IOS-PedometerDelta"));
		Add(Availability.bMotionActivity,
			EOpenMobileSensorType::MotionActivity,
			TEXT("IOS-MotionActivity"));
		return Sensors;
	}

	FOpenMobileSensorCapability MakeStepCounterCapability(
		const FOpenMobileSensorsIOSAvailability& Availability
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::StepCounter;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(
				EOpenMobileSensorType::StepCounter
			);
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.bSupportsNativeBatching = false;
		Capability.BackgroundSupport =
			EOpenMobileSensorBackgroundSupport::Suspended;
		Capability.RequiredPermission =
			FOpenMobileSensorPermissions::GetPermissionName(
				EOpenMobileSensorPermission::MotionActivity
			);
		if (!Availability.bPedometerApiSupported)
		{
			Capability.Availability.State =
				EOpenMobileCapabilityState::NotSupported;
			Capability.Availability.Detail =
				TEXT("Native step counting requires CMPedometer.");
			return Capability;
		}
		if (!Availability.bStepCounting)
		{
			Capability.Availability.State =
				EOpenMobileCapabilityState::Unavailable;
			Capability.Availability.Detail =
				TEXT("This Apple device does not provide step counting.");
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::MissingHardware;
			return Capability;
		}
		switch (Availability.PedometerAuthorizationStatus)
		{
		case EOpenMobilePermissionStatus::Granted:
			Capability.Availability.State =
				EOpenMobileCapabilityState::Available;
			Capability.Availability.Detail =
				TEXT("Available from Core Motion pedometer data.");
			break;
		case EOpenMobilePermissionStatus::Denied:
		case EOpenMobilePermissionStatus::PermanentlyDenied:
			Capability.Availability.State = EOpenMobileCapabilityState::Denied;
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::Permission;
			Capability.Availability.Detail =
				TEXT("Motion access was denied for pedometer data.");
			break;
		case EOpenMobilePermissionStatus::Restricted:
			Capability.Availability.State =
				EOpenMobileCapabilityState::Restricted;
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::Permission;
			Capability.Availability.Detail =
				TEXT("System policy restricts pedometer data.");
			break;
		case EOpenMobilePermissionStatus::NotDetermined:
		default:
			Capability.Availability.State =
				EOpenMobileCapabilityState::PermissionRequired;
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::Permission;
			Capability.Availability.Detail =
				TEXT("Motion access has not been decided for pedometer data.");
			break;
		}
		return Capability;
	}

	FOpenMobileSensorCapability MakeStepDetectorCapability(
		const FOpenMobileSensorsIOSAvailability& Availability
	)
	{
		FOpenMobileSensorCapability Capability =
			MakeStepCounterCapability(Availability);
		Capability.Sensor.Type = EOpenMobileSensorType::StepDetector;
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(
				EOpenMobileSensorType::StepDetector
			);
		if (Capability.Availability.State ==
			EOpenMobileCapabilityState::Available)
		{
			Capability.Availability.Detail =
				TEXT("Available from Core Motion pedometer deltas.");
		}
		return Capability;
	}

	FOpenMobileSensorCapability MakeMotionActivityCapability(
		const FOpenMobileSensorsIOSAvailability& Availability
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::MotionActivity;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(
				EOpenMobileSensorType::MotionActivity
			);
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.bSupportsNativeBatching = false;
		Capability.BackgroundSupport =
			EOpenMobileSensorBackgroundSupport::Suspended;
		Capability.RequiredPermission =
			FOpenMobileSensorPermissions::GetPermissionName(
				EOpenMobileSensorPermission::MotionActivity
			);
		if (!Availability.bMotionActivityApiSupported)
		{
			Capability.Availability.State =
				EOpenMobileCapabilityState::NotSupported;
			Capability.Availability.Detail =
				TEXT("Motion classification requires Core Motion activity APIs.");
			return Capability;
		}
		if (!Availability.bMotionActivity)
		{
			Capability.Availability.State =
				EOpenMobileCapabilityState::Unavailable;
			Capability.Availability.Detail =
				TEXT("This Apple device does not provide motion activity.");
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::MissingHardware;
			return Capability;
		}
		switch (Availability.MotionActivityAuthorizationStatus)
		{
		case EOpenMobilePermissionStatus::Granted:
			Capability.Availability.State =
				EOpenMobileCapabilityState::Available;
			Capability.Availability.Detail =
				TEXT("Available from Core Motion activity classification.");
			break;
		case EOpenMobilePermissionStatus::Denied:
		case EOpenMobilePermissionStatus::PermanentlyDenied:
			Capability.Availability.State = EOpenMobileCapabilityState::Denied;
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::Permission;
			Capability.Availability.Detail =
				TEXT("Motion access was denied for activity classification.");
			break;
		case EOpenMobilePermissionStatus::Restricted:
			Capability.Availability.State =
				EOpenMobileCapabilityState::Restricted;
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::Permission;
			Capability.Availability.Detail =
				TEXT("System policy restricts activity classification.");
			break;
		case EOpenMobilePermissionStatus::NotDetermined:
		default:
			Capability.Availability.State =
				EOpenMobileCapabilityState::PermissionRequired;
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::Permission;
			Capability.Availability.Detail =
				TEXT("Motion access has not been decided for activity classification.");
			break;
		}
		return Capability;
	}

	FOpenMobileSensorCapability MakeAbsoluteAltitudeCapability(
		const FOpenMobileSensorsIOSAvailability& Availability
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::AbsoluteAltitude;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(
				EOpenMobileSensorType::AbsoluteAltitude
			);
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.bSupportsNativeBatching = false;
		Capability.BackgroundSupport =
			EOpenMobileSensorBackgroundSupport::Suspended;
		if (!Availability.bAbsoluteAltitudeApiSupported)
		{
			Capability.Availability.State =
				EOpenMobileCapabilityState::NotSupported;
			Capability.Availability.Detail =
				TEXT("Native absolute altitude requires iOS 15 or later.");
			return Capability;
		}
		if (!Availability.bAbsoluteAltitude)
		{
			Capability.Availability.State =
				EOpenMobileCapabilityState::Unavailable;
			Capability.Availability.Detail =
				TEXT("This device does not provide native absolute altitude.");
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::MissingHardware;
			return Capability;
		}
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Availability.Detail =
			TEXT("Available from Core Motion on iOS 15 or later.");
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 1.0;
		return Capability;
	}

	FOpenMobileSensorCapability MakeUnsupportedAmbientLightCapability()
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::AmbientLight;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(
				EOpenMobileSensorType::AmbientLight
			);
		Capability.Availability.State =
			EOpenMobileCapabilityState::NotSupported;
		Capability.Availability.Detail =
			TEXT("Ordinary iOS applications have no public ambient-light stream.");
		Capability.BackgroundSupport =
			EOpenMobileSensorBackgroundSupport::Unsupported;
		return Capability;
	}

	FOpenMobileAttitudeReferenceFrameCapability MakeReferenceCapability(
		EOpenMobileAttitudeReferenceFrame ReferenceFrame,
		EOpenMobileCapabilityState State,
		const TCHAR* Detail
	)
	{
		FOpenMobileAttitudeReferenceFrameCapability Capability;
		Capability.ReferenceFrame = ReferenceFrame;
		Capability.Availability.Name = TEXT("AttitudeReference");
		Capability.Availability.State = State;
		Capability.Availability.Detail = Detail;
		return Capability;
	}

	void PopulateAttitudeReferenceCapabilities(
		FOpenMobileSensorCapability& Capability,
		const FOpenMobileSensorsIOSAvailability& Availability
	)
	{
		FOpenMobileAttitudeReferenceFrameCapability Game =
			MakeReferenceCapability(
				EOpenMobileAttitudeReferenceFrame::GameRelative,
				EOpenMobileCapabilityState::Available,
				TEXT("Uses Core Motion's arbitrary vertical frame.")
			);
		Game.bMayUseFallback = true;
		Game.bExpectedToDrift = true;
		FOpenMobileAttitudeReferenceFrameCapability Arbitrary =
			MakeReferenceCapability(
				EOpenMobileAttitudeReferenceFrame::ArbitraryVertical,
				EOpenMobileCapabilityState::Available,
				TEXT("Uses Core Motion's arbitrary vertical frame.")
			);
		Arbitrary.bExpectedToDrift = true;
		FOpenMobileAttitudeReferenceFrameCapability Magnetic =
			MakeReferenceCapability(
				EOpenMobileAttitudeReferenceFrame::MagneticNorth,
				Availability.bMagneticNorthReference
					? EOpenMobileCapabilityState::Available
					: EOpenMobileCapabilityState::NotSupported,
				TEXT("Uses Core Motion's magnetic north frame.")
			);
		Magnetic.bHeadingDependent = true;
		Magnetic.bCalibrationRequired = true;
		FOpenMobileAttitudeReferenceFrameCapability TrueNorth =
			MakeReferenceCapability(
				EOpenMobileAttitudeReferenceFrame::TrueNorth,
				EOpenMobileCapabilityState::NotSupported,
				TEXT("True north requires caller-owned location input.")
			);
		TrueNorth.bHeadingDependent = true;
		TrueNorth.bLocationDependent = true;
		TrueNorth.bCalibrationRequired = true;
		Capability.AttitudeReferenceFrames = {
			MoveTemp(Game),
			MoveTemp(Arbitrary),
			MoveTemp(Magnetic),
			MoveTemp(TrueNorth)
		};
	}

	void ApplyAttitudeReferenceState(
		FOpenMobileSensorPhysicalStreamRequest& Request
	)
	{
		if (Request.Sensor.Type != EOpenMobileSensorType::Attitude)
		{
			return;
		}
		FOpenMobileAttitudeReferenceState& State =
			Request.AttitudeReferenceState;
		State = {};
		State.RequestedReferenceFrame = Request.AttitudeReferenceFrame;
		State.AppliedReferenceFrame =
			Request.AttitudeReferenceFrame ==
				EOpenMobileAttitudeReferenceFrame::MagneticNorth
			? EOpenMobileAttitudeReferenceFrame::MagneticNorth
			: EOpenMobileAttitudeReferenceFrame::ArbitraryVertical;
		State.bFallbackApplied = State.RequestedReferenceFrame !=
			State.AppliedReferenceFrame;
		State.bHeadingDependent = State.AppliedReferenceFrame ==
			EOpenMobileAttitudeReferenceFrame::MagneticNorth;
		State.bLocationDependent = State.AppliedReferenceFrame ==
			EOpenMobileAttitudeReferenceFrame::TrueNorth;
		State.bCalibrationRequired = State.bHeadingDependent;
		State.bExpectedToDrift = State.AppliedReferenceFrame ==
			EOpenMobileAttitudeReferenceFrame::ArbitraryVertical;
	}

	FString FailureCode(EOpenMobileSensorsIOSBridgeFailure Failure)
	{
		switch (Failure)
		{
		case EOpenMobileSensorsIOSBridgeFailure::InvalidArgument:
			return TEXT("InvalidArgument");
		case EOpenMobileSensorsIOSBridgeFailure::SensorUnavailable:
			return TEXT("SensorUnavailable");
		case EOpenMobileSensorsIOSBridgeFailure::
			ServiceTemporarilyUnavailable:
			return TEXT("ServiceTemporarilyUnavailable");
		case EOpenMobileSensorsIOSBridgeFailure::ReferenceFrameUnavailable:
			return TEXT("ReferenceFrameUnavailable");
		case EOpenMobileSensorsIOSBridgeFailure::PermissionDenied:
			return TEXT("PermissionDenied");
		case EOpenMobileSensorsIOSBridgeFailure::PermissionRestricted:
			return TEXT("PermissionRestricted");
		case EOpenMobileSensorsIOSBridgeFailure::MissingUsageDescription:
			return TEXT("MissingUsageDescription");
		case EOpenMobileSensorsIOSBridgeFailure::ManagerError:
			return TEXT("ManagerError");
		case EOpenMobileSensorsIOSBridgeFailure::Paused:
			return TEXT("Paused");
		case EOpenMobileSensorsIOSBridgeFailure::ShuttingDown:
			return TEXT("ShuttingDown");
		case EOpenMobileSensorsIOSBridgeFailure::StreamMissing:
			return TEXT("StreamMissing");
		case EOpenMobileSensorsIOSBridgeFailure::None:
		default:
			return {};
		}
	}

	void ApplyBridgeRate(
		const FOpenMobileSensorsIOSBridgeResult& Result,
		FOpenMobileSensorPhysicalStreamRequest& Request
	)
	{
		Request.bNativeBatchingApplied = false;
		if (Result.AppliedFrequencyHz <= 0.0)
		{
			return;
		}
		if (!FMath::IsNearlyEqual(
			Request.RequestedFrequencyHz,
			Result.AppliedFrequencyHz))
		{
			Request.AppliedRateAdjustmentReason =
				EOpenMobileSensorRateAdjustmentReason::BackendLimit;
		}
		Request.RequestedFrequencyHz = Result.AppliedFrequencyHz;
	}
}

FOpenMobileSensorsIOSBackend::FOpenMobileSensorsIOSBackend() = default;

FOpenMobileSensorsIOSBackend::~FOpenMobileSensorsIOSBackend()
{
	BeginShutdown();
}

FName FOpenMobileSensorsIOSBackend::GetBackendName() const
{
	return TEXT("IOS");
}

FName FOpenMobileSensorsIOSBackend::GetProviderName() const
{
	return TEXT("IOSSensorsPermissions");
}

bool FOpenMobileSensorsIOSBackend::SupportsPermission(FName Permission) const
{
	return Permission == FOpenMobileSensorPermissions::GetPermissionName(
		EOpenMobileSensorPermission::MotionActivity
	);
}

FOpenMobilePermissionResult FOpenMobileSensorsIOSBackend::GetStatus(
	FName Permission
) const
{
	FOpenMobilePermissionResult Result;
	Result.Permission = Permission;
	if (!SupportsPermission(Permission))
	{
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("The iOS Sensors provider does not own this permission.")
		);
		return Result;
	}
	if (bShuttingDown.Load())
	{
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The iOS Sensors provider is shutting down.")
		);
		return Result;
	}
	Result = FOpenMobileSensorsIOSBridge::GetMotionActivityPermissionStatus();
	Result.Permission = Permission;
	return Result;
}

bool FOpenMobileSensorsIOSBackend::RequestPermission(
	FName Permission,
	const FGuid& RequestIdentifier,
	FOpenMobileNativePermissionCompletion&& Completion,
	FOpenMobileError& OutError
)
{
	if (!SupportsPermission(Permission))
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("The iOS Sensors provider does not own this permission.")
		);
		return false;
	}
	if (bShuttingDown.Load())
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The iOS Sensors provider is shutting down.")
		);
		return false;
	}
	return GetBridge().RequestMotionActivityPermission(
		RequestIdentifier,
		MoveTemp(Completion),
		OutError
	);
}

void FOpenMobileSensorsIOSBackend::CancelRequest(
	const FGuid& RequestIdentifier
)
{
	if (Bridge)
	{
		Bridge->CancelMotionActivityPermission(RequestIdentifier);
	}
}

double FOpenMobileSensorsIOSBackend::ConvertCoreMotionTimestampSeconds(
	double TimestampSeconds)
{
	return FOpenMobileSensorTimestampConverter::FromIOSCoreMotionSeconds(
		TimestampSeconds);
}

bool FOpenMobileSensorsIOSBackend::
CaptureApplicationWindowRotationFromMainThread(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorScreenRotation Rotation,
	double TimestampSeconds,
	bool bNaturalOrientationLandscape)
{
	return FOpenMobileSensorsScreenRotationService::
		CaptureApplicationWindowRotation(
			OwnerIdentifier,
			Rotation,
			TimestampSeconds,
			bNaturalOrientationLandscape);
}

FOpenMobileCapability FOpenMobileSensorsIOSBackend::GetBackendCapability() const
{
	FOpenMobileCapability Capability;
	Capability.Name = IOpenMobileSensorsBackend::GetModularFeatureName();
	if (bShuttingDown.Load())
	{
		Capability.State = EOpenMobileCapabilityState::TemporarilyUnavailable;
		Capability.Detail = TEXT("The iOS Sensors backend is shutting down.");
		return Capability;
	}
	const EOpenMobileSensorsIOSBridgeFailure Failure =
		static_cast<EOpenMobileSensorsIOSBridgeFailure>(
			LastBridgeFailure.Load());
	if (Failure == EOpenMobileSensorsIOSBridgeFailure::None)
	{
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.Detail = TEXT("The iOS Sensors backend is registered.");
	}
	else if (Failure == EOpenMobileSensorsIOSBridgeFailure::Paused)
	{
		Capability.State = EOpenMobileCapabilityState::TemporarilyUnavailable;
		Capability.Detail = TEXT("Core Motion is paused while the app is inactive.");
	}
	else if (Failure == EOpenMobileSensorsIOSBridgeFailure::
		ServiceTemporarilyUnavailable)
	{
		Capability.State = EOpenMobileCapabilityState::TemporarilyUnavailable;
		Capability.Detail = TEXT("Core Motion is temporarily unavailable.");
	}
	else
	{
		Capability.State = EOpenMobileCapabilityState::Unavailable;
		Capability.Detail = TEXT("Core Motion is unavailable.");
	}
	return Capability;
}

TArray<FOpenMobileSensorCapability>
FOpenMobileSensorsIOSBackend::GetSensorCapabilities() const
{
	using namespace OpenMobileSensorsIOSBackendPrivate;
	TArray<FOpenMobileSensorCapability> Capabilities;
	const FOpenMobileSensorsIOSAvailability Availability = QueryAvailability();
	for (const FSupportedSensor& Supported :
		GetSupportedSensors(Availability))
	{
		if (Supported.Type == EOpenMobileSensorType::AbsoluteAltitude
			|| Supported.Type == EOpenMobileSensorType::StepCounter
			|| Supported.Type == EOpenMobileSensorType::StepDetector
			|| Supported.Type == EOpenMobileSensorType::MotionActivity)
		{
			continue;
		}
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Supported.Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(Supported.Type);
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Availability.Detail = Supported.Type ==
			EOpenMobileSensorType::Proximity
			? TEXT("UIKit proximity monitoring is available; hardware is confirmed when streaming starts.")
			: TEXT("Available from Core Motion.");
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.bSupportsNativeBatching = false;
		Capability.BackgroundSupport =
			EOpenMobileSensorBackgroundSupport::Suspended;
		if (Supported.Type ==
			EOpenMobileSensorType::BarometricPressure)
		{
			Capability.MinimumFrequencyHz = 1.0;
			Capability.MaximumFrequencyHz = 1.0;
		}
		if (Supported.Type == EOpenMobileSensorType::Attitude)
		{
			PopulateAttitudeReferenceCapabilities(Capability, Availability);
		}
		Capabilities.Add(MoveTemp(Capability));
	}
	Capabilities.Add(MakeAbsoluteAltitudeCapability(Availability));
	Capabilities.Add(MakeStepCounterCapability(Availability));
	Capabilities.Add(MakeStepDetectorCapability(Availability));
	Capabilities.Add(MakeMotionActivityCapability(Availability));
	Capabilities.Add(MakeUnsupportedAmbientLightCapability());
	return Capabilities;
}

TArray<FOpenMobileSensorBackendMetadata>
FOpenMobileSensorsIOSBackend::GetSensorMetadata() const
{
	using namespace OpenMobileSensorsIOSBackendPrivate;
	TArray<FOpenMobileSensorBackendMetadata> Metadata;
	for (const FSupportedSensor& Supported :
		GetSupportedSensors(QueryAvailability()))
	{
		FOpenMobileSensorBackendMetadata Entry;
		Entry.Metadata.Sensor.Type = Supported.Type;
		Entry.Metadata.Sensor.InstanceId = TEXT("Default");
		Entry.Metadata.bPreferred = true;
		Entry.Metadata.bReportingModeAvailable = false;
		Entry.Metadata.ReportingMode = EOpenMobileSensorReportingMode::Unknown;
		Entry.MeasurementUnit = EOpenMobileSensorMetadataUnit::Portable;
		Entry.IntervalUnit = EOpenMobileSensorMetadataTimeUnit::Seconds;
		Entry.NativeIdentifier = Supported.NativeIdentifier;
		Metadata.Add(MoveTemp(Entry));
	}
	return Metadata;
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsIOSBackend::StartSensorStream(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	FOpenMobileSensorPhysicalStreamRequest& InOutRequest)
{
	using namespace OpenMobileSensorsIOSBackendPrivate;
	if (bShuttingDown.Load())
	{
		return MapBridgeFailure(
			EOpenMobileSensorsIOSBridgeFailure::ShuttingDown);
	}
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const bool bTrackNativeSteps = InOutRequest.Sensor.Type ==
		EOpenMobileSensorType::StepCounter;
	if (bTrackNativeSteps)
	{
		FScopeLock Lock(&NativeStepCountersMutex);
		if (NativeStepCounters.Contains(Handle.Identifier))
		{
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::InvalidRequest
			);
		}
		NativeStepCounters.Add(
			Handle.Identifier,
			FOpenMobileNativeStepCounterTracker{}
		);
	}
	const FOpenMobileSensorsIOSBridgeResult Result = GetBridge().StartStream(
		Token,
		Handle,
		InOutRequest);
	if (!Result.IsSuccess())
	{
		if (bTrackNativeSteps)
		{
			FScopeLock Lock(&NativeStepCountersMutex);
			NativeStepCounters.Remove(Handle.Identifier);
		}
		LastBridgeFailure.Store(static_cast<uint8>(Result.Failure));
		return MapBridgeFailure(
			Result.Failure,
			Result.NativeDomain,
			Result.NativeCode.IsEmpty()
				? FailureCode(Result.Failure)
				: Result.NativeCode);
	}
	LastBridgeFailure.Store(
		static_cast<uint8>(EOpenMobileSensorsIOSBridgeFailure::None));
	ApplyBridgeRate(Result, InOutRequest);
	ApplyAttitudeReferenceState(InOutRequest);
	return {EOpenMobileSensorResultCode::Success};
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsIOSBackend::ReconfigureSensorStream(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	FOpenMobileSensorPhysicalStreamRequest& InOutRequest)
{
	using namespace OpenMobileSensorsIOSBackendPrivate;
	if (!Bridge)
	{
		return MapBridgeFailure(
			EOpenMobileSensorsIOSBridgeFailure::StreamMissing);
	}
	const FOpenMobileSensorsIOSBridgeResult Result =
		Bridge->ReconfigureStream(Handle, InOutRequest);
	if (!Result.IsSuccess())
	{
		LastBridgeFailure.Store(static_cast<uint8>(Result.Failure));
		return MapBridgeFailure(
			Result.Failure,
			Result.NativeDomain,
			Result.NativeCode.IsEmpty()
				? FailureCode(Result.Failure)
				: Result.NativeCode);
	}
	LastBridgeFailure.Store(
		static_cast<uint8>(EOpenMobileSensorsIOSBridgeFailure::None));
	ApplyBridgeRate(Result, InOutRequest);
	ApplyAttitudeReferenceState(InOutRequest);
	return {EOpenMobileSensorResultCode::Success};
}

void FOpenMobileSensorsIOSBackend::StopSensorStream(
	const FOpenMobileSensorBackendStreamHandle& Handle)
{
	{
		FScopeLock Lock(&NativeStepCountersMutex);
		NativeStepCounters.Remove(Handle.Identifier);
	}
	if (Bridge)
	{
		Bridge->StopStream(Handle);
	}
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsIOSBackend::FlushSensorStream(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FGuid& RequestId,
	FOnOpenMobileSensorBackendFlushComplete&& Completion)
{
	static_cast<void>(Handle);
	static_cast<void>(RequestId);
	static_cast<void>(Completion);
	return FOpenMobileSensorsErrorMapper::Map(
		EOpenMobileSensorFailureReason::UnsupportedOperation,
		TEXT("CoreMotion"),
		TEXT("NoNativeFifo"));
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsIOSBackend::QueryNativeStepCount(
	const FGuid& RequestId,
	const FOpenMobileNativeStepCountQuery& Query,
	FOnOpenMobileNativeStepCountBackendQueryComplete&& Completion)
{
	using namespace OpenMobileSensorsIOSBackendPrivate;
	if (bShuttingDown.Load())
	{
		return MapBridgeFailure(
			EOpenMobileSensorsIOSBridgeFailure::ShuttingDown);
	}
	const FOpenMobileSensorsIOSBridgeResult Result =
		GetBridge().QueryNativeStepCount(
			RequestId,
			Query,
			MoveTemp(Completion));
	if (!Result.IsSuccess())
	{
		LastBridgeFailure.Store(static_cast<uint8>(Result.Failure));
		return MapBridgeFailure(
			Result.Failure,
			Result.NativeDomain,
			Result.NativeCode.IsEmpty()
				? FailureCode(Result.Failure)
				: Result.NativeCode);
	}
	LastBridgeFailure.Store(
		static_cast<uint8>(EOpenMobileSensorsIOSBridgeFailure::None));
	return {EOpenMobileSensorResultCode::Accepted};
}

bool FOpenMobileSensorsIOSBackend::CancelNativeStepCountQuery(
	const FGuid& RequestId)
{
	return Bridge && Bridge->CancelNativeStepCountQuery(RequestId);
}

bool FOpenMobileSensorsIOSBackend::PublishVectorBatchFromMotionQueue(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileVectorSensorBatch& Batch)
{
	if (bShuttingDown.Load())
	{
		return false;
	}
	FOpenMobileVectorSensorBatch NormalizedBatch = Batch;
	for (FOpenMobileVectorSensorSample& Sample : NormalizedBatch.Samples)
	{
		if (Sample.Header.SourceFlags == 0)
		{
			Sample.Header.SourceFlags =
				FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(
					Sample.Header.Sensor.Type);
		}
		FOpenMobileSensorUnitConverter::NormalizeVectorSample(
			EOpenMobileSensorNativePlatform::IOS,
			Sample);
		FOpenMobileSensorCoordinateConverter::ConvertVectorSample(
			EOpenMobileSensorNativePlatform::IOS,
			Sample);
	}
	return FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token,
		Handle,
		NormalizedBatch);
}

bool FOpenMobileSensorsIOSBackend::PublishAttitudeBatchFromMotionQueue(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileAttitudeSensorBatch& Batch)
{
	if (bShuttingDown.Load())
	{
		return false;
	}
	FOpenMobileAttitudeSensorBatch NormalizedBatch = Batch;
	for (FOpenMobileAttitudeSensorSample& Sample : NormalizedBatch.Samples)
	{
		if (Sample.Header.SourceFlags == 0)
		{
			Sample.Header.SourceFlags =
				FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(
					Sample.Header.Sensor.Type);
		}
		FOpenMobileSensorUnitConverter::NormalizeAttitudeSample(
			EOpenMobileSensorNativePlatform::IOS,
			Sample);
		FOpenMobileSensorCoordinateConverter::ConvertAttitudeSample(
			EOpenMobileSensorNativePlatform::IOS,
			Sample);
	}
	return FOpenMobileSensorsSampleService::PublishAttitudeBatchFromBackend(
		Token,
		Handle,
		NormalizedBatch);
}

bool FOpenMobileSensorsIOSBackend::PublishScalarBatchFromMotionQueue(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileScalarSensorBatch& Batch)
{
	if (bShuttingDown.Load())
	{
		return false;
	}
	FOpenMobileScalarSensorBatch NormalizedBatch = Batch;
	for (FOpenMobileScalarSensorSample& Sample : NormalizedBatch.Samples)
	{
		if (Sample.Header.SourceFlags == 0)
		{
			Sample.Header.SourceFlags =
				FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(
					Sample.Header.Sensor.Type);
		}
		FOpenMobileSensorUnitConverter::NormalizeScalarSample(
			EOpenMobileSensorNativePlatform::IOS,
			Sample);
	}
	return FOpenMobileSensorsSampleService::PublishScalarBatchFromBackend(
		Token,
		Handle,
		NormalizedBatch);
}

bool FOpenMobileSensorsIOSBackend::PublishHeadingBatchFromMotionQueue(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileHeadingSensorBatch& Batch)
{
	if (bShuttingDown.Load())
	{
		return false;
	}
	FOpenMobileHeadingSensorBatch NormalizedBatch = Batch;
	for (FOpenMobileHeadingSensorSample& Sample : NormalizedBatch.Samples)
	{
		if (Sample.Header.SourceFlags == 0)
		{
			Sample.Header.SourceFlags =
				FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(
					Sample.Header.Sensor.Type);
		}
		FOpenMobileSensorUnitConverter::NormalizeHeadingSample(
			EOpenMobileSensorNativePlatform::IOS,
			Sample);
	}
	return FOpenMobileSensorsSampleService::PublishHeadingBatchFromBackend(
		Token,
		Handle,
		NormalizedBatch);
}

bool FOpenMobileSensorsIOSBackend::PublishProximityBatchFromProximityQueue(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileProximitySensorBatch& Batch)
{
	if (bShuttingDown.Load())
	{
		return false;
	}
	FOpenMobileProximitySensorBatch NormalizedBatch = Batch;
	for (FOpenMobileProximitySensorSample& Sample : NormalizedBatch.Samples)
	{
		if (Sample.Header.SourceFlags == 0)
		{
			Sample.Header.SourceFlags =
				FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(
					Sample.Header.Sensor.Type);
		}
		FOpenMobileSensorUnitConverter::NormalizeProximitySample(
			EOpenMobileSensorNativePlatform::IOS,
			Sample);
	}
	OpenMobile::DispatchToGameThread(
		[Token, Handle, Batch = MoveTemp(NormalizedBatch)]() mutable
		{
			FOpenMobileSensorsSampleService::
				PublishProximityBatchFromBackend(Token, Handle, Batch);
		}
	);
	return true;
}

bool FOpenMobileSensorsIOSBackend::PublishStepsBatchFromPedometerQueue(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileStepsSensorBatch& Batch)
{
	if (bShuttingDown.Load())
	{
		return false;
	}
	FOpenMobileStepsSensorBatch NormalizedBatch = Batch;
	{
		FScopeLock Lock(&NativeStepCountersMutex);
		FOpenMobileNativeStepCounterTracker* Tracker =
			NativeStepCounters.Find(Handle.Identifier);
		for (FOpenMobileStepsSensorSample& Sample : NormalizedBatch.Samples)
		{
			if (Sample.Header.SourceFlags == 0)
			{
				Sample.Header.SourceFlags =
					FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(
						Sample.Header.Sensor.Type);
			}
			if (Sample.Header.Sensor.Type ==
				EOpenMobileSensorType::StepCounter)
			{
				if (!Tracker)
				{
					return false;
				}
				Sample.Header.bValid &= Tracker->Apply(Sample);
			}
			else if (Sample.Header.Sensor.Type ==
				EOpenMobileSensorType::StepDetector)
			{
				Sample.bHasNativeTotal = true;
				Sample.NativeTotal = Sample.Count;
				Sample.DetectionSource =
					EOpenMobileStepDetectionSource::IOSPedometerDelta;
				Sample.DetectionQuality = EOpenMobileStepDetectionQuality::
					InferredFromPedometerDelta;
			}
			else
			{
				return false;
			}
			FOpenMobileSensorUnitConverter::NormalizeStepsSample(
				EOpenMobileSensorNativePlatform::IOS,
				Sample);
		}
	}
	return FOpenMobileSensorsSampleService::PublishStepsBatchFromBackend(
		Token,
		Handle,
		NormalizedBatch);
}

bool FOpenMobileSensorsIOSBackend::PublishActivityBatchFromMotionQueue(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileActivitySensorBatch& Batch)
{
	if (bShuttingDown.Load())
	{
		return false;
	}
	FOpenMobileActivitySensorBatch NormalizedBatch;
	NormalizedBatch.Samples.Reserve(Batch.Samples.Num());
	for (FOpenMobileActivitySensorSample Sample : Batch.Samples)
	{
		if (Sample.Header.SourceFlags == 0)
		{
			Sample.Header.SourceFlags =
				FOpenMobileSensorSourcePolicy::GetIOSNativeSourceFlags(
					Sample.Header.Sensor.Type
				);
		}
		FOpenMobileSensorUnitConverter::NormalizeActivitySample(
			EOpenMobileSensorNativePlatform::IOS,
			Sample
		);
		NormalizedBatch.Samples.Add(MoveTemp(Sample));
	}
	return NormalizedBatch.Samples.IsEmpty()
		|| FOpenMobileSensorsSampleService::PublishActivityBatchFromBackend(
			Token,
			Handle,
			NormalizedBatch
		);
}

bool FOpenMobileSensorsIOSBackend::
PublishMagneticFieldAccuracyFromMotionQueue(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileSensorIdentifier& Sensor,
	int32 NativeAccuracy,
	double TimestampSeconds)
{
	return !bShuttingDown.Load()
		&& FOpenMobileSensorsSampleService::PublishAccuracyFromBackend(
			Token,
			Handle,
			FOpenMobileSensorAccuracyMapper::FromIOSMagneticFieldAccuracy(
				Sensor,
				NativeAccuracy,
				TimestampSeconds));
}

bool FOpenMobileSensorsIOSBackend::PublishHeadingAccuracyFromLocationCallback(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileSensorIdentifier& Sensor,
	double AccuracyDegrees,
	bool bCalibrationRequired,
	double TimestampSeconds)
{
	return !bShuttingDown.Load()
		&& FOpenMobileSensorsSampleService::PublishAccuracyFromBackend(
			Token,
			Handle,
			FOpenMobileSensorAccuracyMapper::FromIOSHeadingAccuracy(
				Sensor,
				AccuracyDegrees,
				bCalibrationRequired,
				TimestampSeconds));
}

void FOpenMobileSensorsIOSBackend::FailPhysicalStreamFromBackend(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	EOpenMobileSensorsIOSBridgeFailure Failure,
	FString NativeDomain,
	FString NativeCode)
{
	{
		FScopeLock Lock(&NativeStepCountersMutex);
		NativeStepCounters.Remove(Handle.Identifier);
	}
	LastBridgeFailure.Store(static_cast<uint8>(Failure));
	FOpenMobileSensorOperationResult Result = MapBridgeFailure(
		Failure,
		MoveTemp(NativeDomain),
		MoveTemp(NativeCode));
	OpenMobile::DispatchToGameThread(
		[Token, Handle, Result = MoveTemp(Result)]() mutable
		{
			FOpenMobileSensorsSubscriptionService::FailPhysicalStreamFromBackend(
				Token,
				Handle,
				Result);
		});
}

void FOpenMobileSensorsIOSBackend::BeginShutdown()
{
	if (bShuttingDown.Exchange(true))
	{
		return;
	}
	{
		FScopeLock Lock(&NativeStepCountersMutex);
		NativeStepCounters.Reset();
	}
	if (Bridge)
	{
		Bridge->Shutdown();
		Bridge.Reset();
	}
}

FOpenMobileSensorsIOSBridge& FOpenMobileSensorsIOSBackend::GetBridge() const
{
	if (!Bridge)
	{
		Bridge = MakeUnique<FOpenMobileSensorsIOSBridge>(
			const_cast<FOpenMobileSensorsIOSBackend&>(*this));
	}
	return *Bridge;
}

FOpenMobileSensorsIOSAvailability
FOpenMobileSensorsIOSBackend::QueryAvailability() const
{
	return Bridge
		? Bridge->QueryAvailability()
		: FOpenMobileSensorsIOSBridge::QuerySystemAvailability();
}

FOpenMobileSensorOperationResult FOpenMobileSensorsIOSBackend::MapBridgeFailure(
	EOpenMobileSensorsIOSBridgeFailure Failure,
	FString NativeDomain,
	FString NativeCode) const
{
	EOpenMobileSensorFailureReason Reason =
		EOpenMobileSensorFailureReason::OperationalFailure;
	switch (Failure)
	{
	case EOpenMobileSensorsIOSBridgeFailure::InvalidArgument:
		Reason = EOpenMobileSensorFailureReason::InvalidRequest;
		break;
	case EOpenMobileSensorsIOSBridgeFailure::SensorUnavailable:
		Reason = EOpenMobileSensorFailureReason::MissingHardware;
		break;
	case EOpenMobileSensorsIOSBridgeFailure::
		ServiceTemporarilyUnavailable:
		Reason = EOpenMobileSensorFailureReason::TemporarilyUnavailable;
		break;
	case EOpenMobileSensorsIOSBridgeFailure::ReferenceFrameUnavailable:
		Reason = EOpenMobileSensorFailureReason::InvalidReferenceFrame;
		break;
	case EOpenMobileSensorsIOSBridgeFailure::PermissionDenied:
		Reason = EOpenMobileSensorFailureReason::PermissionDenied;
		break;
	case EOpenMobileSensorsIOSBridgeFailure::PermissionRestricted:
		Reason = EOpenMobileSensorFailureReason::PermissionRestricted;
		break;
	case EOpenMobileSensorsIOSBridgeFailure::MissingUsageDescription:
		Reason = EOpenMobileSensorFailureReason::ConfigurationBlocked;
		break;
	case EOpenMobileSensorsIOSBridgeFailure::Paused:
	case EOpenMobileSensorsIOSBridgeFailure::ShuttingDown:
		Reason = EOpenMobileSensorFailureReason::TemporarilyUnavailable;
		break;
	case EOpenMobileSensorsIOSBridgeFailure::StreamMissing:
		Reason = EOpenMobileSensorFailureReason::InvalidHandle;
		break;
	case EOpenMobileSensorsIOSBridgeFailure::ManagerError:
	case EOpenMobileSensorsIOSBridgeFailure::None:
	default:
		break;
	}
	return FOpenMobileSensorsErrorMapper::Map(
		Reason,
		NativeDomain.IsEmpty() ? TEXT("CoreMotion") : MoveTemp(NativeDomain),
		NativeCode.IsEmpty()
			? OpenMobileSensorsIOSBackendPrivate::FailureCode(Failure)
			: MoveTemp(NativeCode));
}
