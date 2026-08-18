#include "OpenMobileSensorsAndroidBackend.h"

#include "Containers/Ticker.h"
#include "Features/IModularFeatures.h"
#include "OpenMobileAsync.h"
#include "OpenMobileMotionActivityProviderResolver.h"
#include "OpenMobileSensorAccuracyMapper.h"
#include "OpenMobileSensorCoordinates.h"
#include "OpenMobileSensorHeading.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorScreenRotationService.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "OpenMobileSensorTimestamp.h"
#include "OpenMobileSensorUnits.h"
#include "OpenMobileSensorValidity.h"
#include "OpenMobileSensorsAndroidBridge.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsMetadataService.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsAndroidBackendPrivate
{
	constexpr double MicrosecondsPerSecond = 1000000.0;

	FString FailureCode(EOpenMobileSensorsAndroidBridgeFailure Failure)
	{
		switch (Failure)
		{
		case EOpenMobileSensorsAndroidBridgeFailure::ActivityUnavailable:
			return TEXT("ActivityUnavailable");
		case EOpenMobileSensorsAndroidBridgeFailure::BridgeClassMissing:
			return TEXT("BridgeClassMissing");
		case EOpenMobileSensorsAndroidBridgeFailure::BridgeMethodMissing:
			return TEXT("BridgeMethodMissing");
		case EOpenMobileSensorsAndroidBridgeFailure::BridgeCreateFailed:
			return TEXT("BridgeCreateFailed");
		case EOpenMobileSensorsAndroidBridgeFailure::JavaException:
			return TEXT("JavaException");
		case EOpenMobileSensorsAndroidBridgeFailure::InvalidPayload:
			return TEXT("InvalidPayload");
		case EOpenMobileSensorsAndroidBridgeFailure::InvalidArgument:
			return TEXT("InvalidArgument");
		case EOpenMobileSensorsAndroidBridgeFailure::SensorMissing:
			return TEXT("SensorMissing");
		case EOpenMobileSensorsAndroidBridgeFailure::PermissionDenied:
			return TEXT("PermissionDenied");
		case EOpenMobileSensorsAndroidBridgeFailure::RegisterFailed:
			return TEXT("RegisterFailed");
		case EOpenMobileSensorsAndroidBridgeFailure::StreamMissing:
			return TEXT("StreamMissing");
		case EOpenMobileSensorsAndroidBridgeFailure::FlushFailed:
			return TEXT("FlushFailed");
		case EOpenMobileSensorsAndroidBridgeFailure::Paused:
			return TEXT("Paused");
		case EOpenMobileSensorsAndroidBridgeFailure::ShuttingDown:
			return TEXT("ShuttingDown");
		case EOpenMobileSensorsAndroidBridgeFailure::Timeout:
			return TEXT("Timeout");
		case EOpenMobileSensorsAndroidBridgeFailure::None:
		default:
			return {};
		}
	}

	EOpenMobileSensorReportingMode MapReportingMode(int32 NativeMode)
	{
		switch (NativeMode)
		{
		case 0:
			return EOpenMobileSensorReportingMode::Continuous;
		case 1:
			return EOpenMobileSensorReportingMode::OnChange;
		case 2:
			return EOpenMobileSensorReportingMode::OneShot;
		case 3:
			return EOpenMobileSensorReportingMode::SpecialTrigger;
		default:
			return EOpenMobileSensorReportingMode::Unknown;
		}
	}

	int32 AttitudePreference(
		int32 NativeType,
		EOpenMobileAttitudeReferenceFrame ReferenceFrame
	)
	{
		switch (ReferenceFrame)
		{
		case EOpenMobileAttitudeReferenceFrame::GameRelative:
		case EOpenMobileAttitudeReferenceFrame::ArbitraryVertical:
			return NativeType == 15 ? 0 : NativeType == 11 ? 1 : 2;
		case EOpenMobileAttitudeReferenceFrame::MagneticNorth:
			return NativeType == 11 ? 0 : NativeType == 20 ? 1 : 100;
		case EOpenMobileAttitudeReferenceFrame::TrueNorth:
		default:
			return 100;
		}
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

	void ApplyActivityRecognitionPermission(
		FOpenMobileSensorCapability& Capability,
		const FOpenMobilePermissionResult& Permission
	)
	{
		const FName ActivityRecognition =
			FOpenMobileSensorPermissions::GetPermissionName(
				EOpenMobileSensorPermission::ActivityRecognition
			);
		if (Capability.RequiredPermission != ActivityRecognition
			|| Capability.Availability.State ==
				EOpenMobileCapabilityState::NotSupported
			|| Capability.ActiveRestriction ==
				EOpenMobileSensorRestriction::MissingHardware)
		{
			return;
		}
		if (Permission.Error.IsSet())
		{
			if (Permission.Error.Code == EOpenMobileErrorCode::NotConfigured)
			{
				Capability.Availability.State =
					EOpenMobileCapabilityState::NotConfigured;
				Capability.ActiveRestriction =
					EOpenMobileSensorRestriction::Configuration;
				Capability.Availability.Detail =
					TEXT("Activity recognition is not declared in the Android manifest.");
			}
			else
			{
				Capability.Availability.State =
					EOpenMobileCapabilityState::TemporarilyUnavailable;
				Capability.ActiveRestriction =
					EOpenMobileSensorRestriction::TemporarilyUnavailable;
				Capability.Availability.Detail =
					TEXT("Android activity-recognition status is unavailable.");
			}
			return;
		}
		switch (Permission.Status)
		{
		case EOpenMobilePermissionStatus::Granted:
			return;
		case EOpenMobilePermissionStatus::NotDetermined:
			Capability.Availability.State =
				EOpenMobileCapabilityState::PermissionRequired;
			Capability.Availability.Detail =
				TEXT("Activity-recognition permission has not been decided.");
			break;
		case EOpenMobilePermissionStatus::Denied:
		case EOpenMobilePermissionStatus::PermanentlyDenied:
			Capability.Availability.State = EOpenMobileCapabilityState::Denied;
			Capability.Availability.Detail =
				TEXT("Activity-recognition permission was denied.");
			break;
		case EOpenMobilePermissionStatus::Restricted:
			Capability.Availability.State =
				EOpenMobileCapabilityState::Restricted;
			Capability.Availability.Detail =
				TEXT("Activity recognition is restricted by system policy.");
			break;
		}
		Capability.ActiveRestriction =
			EOpenMobileSensorRestriction::Permission;
	}

	void PopulateAttitudeReferenceCapabilities(
		FOpenMobileSensorCapability& Capability,
		const TArray<FOpenMobileSensorsAndroidSensorDescriptor>& Descriptors
	)
	{
		const bool bHasGameRotationVector = Descriptors.ContainsByPredicate(
			[](const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor)
			{
				return Descriptor.Sensor.Type == EOpenMobileSensorType::Attitude
					&& Descriptor.NativeType == 15;
			}
		);
		const bool bHasMagneticRotationVector = Descriptors.ContainsByPredicate(
			[](const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor)
			{
				return Descriptor.Sensor.Type == EOpenMobileSensorType::Attitude
					&& (Descriptor.NativeType == 11
						|| Descriptor.NativeType == 20);
			}
		);
		const bool bHasAttitude =
			bHasGameRotationVector || bHasMagneticRotationVector;
		FOpenMobileAttitudeReferenceFrameCapability Game =
			MakeReferenceCapability(
				EOpenMobileAttitudeReferenceFrame::GameRelative,
				bHasAttitude
					? EOpenMobileCapabilityState::Available
					: EOpenMobileCapabilityState::NotSupported,
				TEXT("Uses the closest Android rotation-vector sensor.")
			);
		Game.bMayUseFallback = !bHasGameRotationVector;
		Game.bExpectedToDrift = bHasGameRotationVector;
		FOpenMobileAttitudeReferenceFrameCapability Arbitrary =
			MakeReferenceCapability(
				EOpenMobileAttitudeReferenceFrame::ArbitraryVertical,
				bHasAttitude
					? EOpenMobileCapabilityState::Available
					: EOpenMobileCapabilityState::NotSupported,
				TEXT("Uses a vertical Android rotation-vector frame.")
			);
		Arbitrary.bMayUseFallback = !bHasGameRotationVector;
		Arbitrary.bExpectedToDrift = bHasGameRotationVector;
		FOpenMobileAttitudeReferenceFrameCapability Magnetic =
			MakeReferenceCapability(
				EOpenMobileAttitudeReferenceFrame::MagneticNorth,
				bHasMagneticRotationVector
					? EOpenMobileCapabilityState::Available
					: EOpenMobileCapabilityState::NotSupported,
				TEXT("Requires an Android magnetic rotation-vector sensor.")
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
		FOpenMobileSensorPhysicalStreamRequest& Request,
		const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor
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
		State.AppliedReferenceFrame = Descriptor.NativeType == 15
			? EOpenMobileAttitudeReferenceFrame::GameRelative
			: EOpenMobileAttitudeReferenceFrame::MagneticNorth;
		State.bFallbackApplied = State.RequestedReferenceFrame !=
			State.AppliedReferenceFrame;
		State.bHeadingDependent = State.AppliedReferenceFrame ==
			EOpenMobileAttitudeReferenceFrame::MagneticNorth;
		State.bLocationDependent = State.AppliedReferenceFrame ==
			EOpenMobileAttitudeReferenceFrame::TrueNorth;
		State.bCalibrationRequired = State.bHeadingDependent;
		State.bExpectedToDrift = State.AppliedReferenceFrame ==
			EOpenMobileAttitudeReferenceFrame::GameRelative
			|| State.AppliedReferenceFrame ==
				EOpenMobileAttitudeReferenceFrame::ArbitraryVertical;
	}

	int32 DescriptorPreference(
		const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor,
		EOpenMobileAttitudeReferenceFrame ReferenceFrame
	)
	{
		int32 Score = Descriptor.bPreferred ? 0 : 10;
		Score += Descriptor.bWakeUp ? 2 : 0;
		if (Descriptor.Sensor.Type == EOpenMobileSensorType::Attitude)
		{
			Score += AttitudePreference(
				Descriptor.NativeType,
				ReferenceFrame
			) * 100;
		}
		else if (Descriptor.Sensor.Type ==
			EOpenMobileSensorType::MagneticHeading)
		{
			Score += Descriptor.NativeType == 11
				? 0
				: Descriptor.NativeType == 20 ? 100 : 10000;
		}
		return Score;
	}

	void AddMagneticHeadingDescriptors(
		TArray<FOpenMobileSensorsAndroidSensorDescriptor>& Descriptors
	)
	{
		TArray<FOpenMobileSensorsAndroidSensorDescriptor> HeadingDescriptors;
		for (const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor
			: Descriptors)
		{
			if (Descriptor.Sensor.Type != EOpenMobileSensorType::Attitude
				|| (Descriptor.NativeType != 11
					&& Descriptor.NativeType != 20))
			{
				continue;
			}
			FOpenMobileSensorsAndroidSensorDescriptor Heading = Descriptor;
			Heading.Sensor.Type = EOpenMobileSensorType::MagneticHeading;
			Heading.Sensor.InstanceId = FName(*FString::Printf(
				TEXT("%s-MagneticHeading"),
				*Descriptor.NativeIdentifier
			));
			Heading.NativeName = FString::Printf(
				TEXT("%s Magnetic Heading"),
				*Descriptor.NativeName
			);
			HeadingDescriptors.Add(MoveTemp(Heading));
		}
		Descriptors.Append(MoveTemp(HeadingDescriptors));
	}

	FOpenMobileSensorSampleHeader MakeHeader(
		const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor,
		int64 TimestampNanoseconds,
		bool bReset
	)
	{
		FOpenMobileSensorSampleHeader Header;
		Header.Sensor = Descriptor.Sensor;
		Header.TimestampSeconds =
			FOpenMobileSensorTimestampConverter::
				FromAndroidSensorEventNanoseconds(TimestampNanoseconds);
		Header.bStatefulProcessingReset = bReset;
		Header.bValid = true;
		Header.SourceFlags =
			FOpenMobileSensorSourcePolicy::GetAndroidNativeSourceFlags(
				Descriptor.Sensor.Type
			);
		return Header;
	}

	template <typename OptionalType, typename ValueType>
	void SetOptional(
		OptionalType& Optional,
		ValueType Value,
		bool bAvailable
	)
	{
		Optional.bAvailable = bAvailable;
		Optional.Value = bAvailable ? Value : ValueType{};
	}
}

FOpenMobileSensorsAndroidBackend::FOpenMobileSensorsAndroidBackend()
{
	MotionActivityProviderRegisteredHandle = IModularFeatures::Get()
		.OnModularFeatureRegistered().AddRaw(
			this,
			&FOpenMobileSensorsAndroidBackend::
				HandleMotionActivityProviderRegistered
		);
	MotionActivityProviderUnregisteredHandle = IModularFeatures::Get()
		.OnModularFeatureUnregistered().AddRaw(
			this,
			&FOpenMobileSensorsAndroidBackend::
				HandleMotionActivityProviderUnregistered
		);
}

void FOpenMobileSensorsAndroidBackend::
HandleMotionActivityProviderRegistered(
	const FName& FeatureName,
	IModularFeature* Feature
)
{
	if (FeatureName !=
		IOpenMobileMotionActivityProvider::GetModularFeatureName()
		|| !Feature)
	{
		return;
	}
	ExecuteOnGameThread(
		TEXT("OpenMobileMotionActivityProviderRegistered"),
		[]()
		{
			FOpenMobileSensorsCapabilityService::
				HandleBackendGenerationChanged();
		}
	);
}

FOpenMobileSensorsAndroidBackend::~FOpenMobileSensorsAndroidBackend()
{
	BeginShutdown();
}

FName FOpenMobileSensorsAndroidBackend::GetBackendName() const
{
	return TEXT("Android");
}

FName FOpenMobileSensorsAndroidBackend::GetProviderName() const
{
	return TEXT("AndroidSensorsPermissions");
}

bool FOpenMobileSensorsAndroidBackend::SupportsPermission(
	FName Permission
) const
{
	return Permission == FOpenMobileSensorPermissions::GetPermissionName(
		EOpenMobileSensorPermission::ActivityRecognition
	);
}

FOpenMobilePermissionResult FOpenMobileSensorsAndroidBackend::GetStatus(
	FName Permission
) const
{
	FOpenMobilePermissionResult Result;
	Result.Permission = Permission;
	if (!SupportsPermission(Permission))
	{
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("The Android Sensors provider does not own this permission.")
		);
		return Result;
	}
	if (bShuttingDown.Load())
	{
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Android Sensors provider is shutting down.")
		);
		return Result;
	}
	Result = FOpenMobileSensorsAndroidBridge::
		QueryActivityRecognitionPermissionStatus();
	Result.Permission = Permission;
	return Result;
}

bool FOpenMobileSensorsAndroidBackend::RequestPermission(
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
			TEXT("The Android Sensors provider does not own this permission.")
		);
		return false;
	}
	if (bShuttingDown.Load())
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("The Android Sensors provider is shutting down.")
		);
		return false;
	}
	return GetBridge().RequestActivityRecognitionPermission(
		RequestIdentifier,
		MoveTemp(Completion),
		OutError
	);
}

void FOpenMobileSensorsAndroidBackend::CancelRequest(
	const FGuid& RequestIdentifier
)
{
	if (Bridge)
	{
		Bridge->CancelPermissionRequest(RequestIdentifier);
	}
}

double FOpenMobileSensorsAndroidBackend::
ConvertSensorEventTimestampNanoseconds(int64 TimestampNanoseconds)
{
	return FOpenMobileSensorTimestampConverter::
		FromAndroidSensorEventNanoseconds(TimestampNanoseconds);
}

bool FOpenMobileSensorsAndroidBackend::
CaptureApplicationWindowRotationFromUIThread(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorScreenRotation Rotation,
	double TimestampSeconds,
	bool bNaturalOrientationLandscape
)
{
	return FOpenMobileSensorsScreenRotationService::
		CaptureApplicationWindowRotation(
			OwnerIdentifier,
			Rotation,
			TimestampSeconds,
			bNaturalOrientationLandscape
		);
}

FOpenMobileCapability
FOpenMobileSensorsAndroidBackend::GetBackendCapability() const
{
	FOpenMobileCapability Capability;
	Capability.Name = IOpenMobileSensorsBackend::GetModularFeatureName();
	if (bShuttingDown.Load())
	{
		Capability.State = EOpenMobileCapabilityState::TemporarilyUnavailable;
		Capability.Detail = TEXT("The Android Sensors backend is shutting down.");
		return Capability;
	}
	const EOpenMobileSensorsAndroidBridgeFailure Failure =
		static_cast<EOpenMobileSensorsAndroidBridgeFailure>(
			LastBridgeFailure.Load()
		);
	if (Failure == EOpenMobileSensorsAndroidBridgeFailure::None)
	{
		Capability.State = EOpenMobileCapabilityState::Available;
		Capability.Detail = TEXT("The Android Sensors backend is registered.");
	}
	else if (Failure ==
			EOpenMobileSensorsAndroidBridgeFailure::ActivityUnavailable
		|| Failure == EOpenMobileSensorsAndroidBridgeFailure::Paused
		|| Failure == EOpenMobileSensorsAndroidBridgeFailure::Timeout)
	{
		Capability.State = EOpenMobileCapabilityState::TemporarilyUnavailable;
		Capability.Detail = TEXT("Android sensor services are temporarily unavailable.");
	}
	else
	{
		Capability.State = EOpenMobileCapabilityState::Unavailable;
		Capability.Detail = TEXT("The packaged Android sensor bridge is unavailable.");
	}
	return Capability;
}

TArray<FOpenMobileSensorCapability>
FOpenMobileSensorsAndroidBackend::GetSensorCapabilities() const
{
	using namespace OpenMobileSensorsAndroidBackendPrivate;
	const FOpenMobilePermissionResult ActivityRecognition =
		FOpenMobileSensorsAndroidBridge::
			QueryActivityRecognitionPermissionStatus();
	TArray<FOpenMobileSensorsAndroidSensorDescriptor> Descriptors;
	if (!QuerySensorDescriptors(Descriptors))
	{
		TArray<FOpenMobileSensorCapability> UnavailableCapabilities = {
			FOpenMobileMotionActivityProviderResolver::GetCapability(),
			FOpenMobileMotionActivityProviderResolver::
				GetTransitionCapability()
		};
		for (FOpenMobileSensorCapability& Capability : UnavailableCapabilities)
		{
			ApplyActivityRecognitionPermission(
				Capability,
				ActivityRecognition
			);
		}
		return UnavailableCapabilities;
	}
	TMap<EOpenMobileSensorType, FOpenMobileSensorsAndroidSensorDescriptor>
		Preferred;
	for (const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor
		: Descriptors)
	{
		FOpenMobileSensorsAndroidSensorDescriptor* Existing =
			Preferred.Find(Descriptor.Sensor.Type);
		if (!Existing
			|| DescriptorPreference(
				Descriptor,
				EOpenMobileAttitudeReferenceFrame::GameRelative
			) < DescriptorPreference(
				*Existing,
				EOpenMobileAttitudeReferenceFrame::GameRelative
			))
		{
			Preferred.Add(Descriptor.Sensor.Type, Descriptor);
		}
	}
	TArray<FOpenMobileSensorCapability> Capabilities;
	Capabilities.Reserve(Preferred.Num() + 2);
	for (const TPair<EOpenMobileSensorType,
		FOpenMobileSensorsAndroidSensorDescriptor>& Pair : Preferred)
	{
		const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor = Pair.Value;
		FOpenMobileSensorCapability Capability;
		Capability.Sensor = Descriptor.Sensor;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(Descriptor.Sensor.Type);
		Capability.Availability.State =
			EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		if (Descriptor.NativeReportingMode == 0)
		{
			if (Descriptor.MaximumDelayMicroseconds > 0)
			{
				Capability.MinimumFrequencyHz = MicrosecondsPerSecond
					/ Descriptor.MaximumDelayMicroseconds;
			}
			if (Descriptor.MinimumDelayMicroseconds > 0)
			{
				Capability.MaximumFrequencyHz = MicrosecondsPerSecond
					/ Descriptor.MinimumDelayMicroseconds;
			}
		}
		Capability.bSupportsNativeBatching =
			Descriptor.FifoCapacitySamples > 0;
		Capability.BackgroundSupport =
			EOpenMobileSensorBackgroundSupport::Suspended;
		if (Descriptor.Sensor.Type == EOpenMobileSensorType::StepCounter
			|| Descriptor.Sensor.Type == EOpenMobileSensorType::StepDetector)
		{
			Capability.RequiredPermission =
				FOpenMobileSensorPermissions::GetPermissionName(
					EOpenMobileSensorPermission::ActivityRecognition
				);
		}
		if (Descriptor.Sensor.Type == EOpenMobileSensorType::Attitude)
		{
			PopulateAttitudeReferenceCapabilities(Capability, Descriptors);
		}
		Capabilities.Add(MoveTemp(Capability));
	}
	Capabilities.Add(
		FOpenMobileMotionActivityProviderResolver::GetCapability()
	);
	Capabilities.Add(
		FOpenMobileMotionActivityProviderResolver::GetTransitionCapability()
	);
	for (FOpenMobileSensorCapability& Capability : Capabilities)
	{
		ApplyActivityRecognitionPermission(
			Capability,
			ActivityRecognition
		);
	}
	Capabilities.Sort(
		[](const FOpenMobileSensorCapability& Left,
			const FOpenMobileSensorCapability& Right)
		{
			return static_cast<uint8>(Left.Sensor.Type)
				< static_cast<uint8>(Right.Sensor.Type);
		}
	);
	return Capabilities;
}

TArray<FOpenMobileSensorBackendMetadata>
FOpenMobileSensorsAndroidBackend::GetSensorMetadata() const
{
	using namespace OpenMobileSensorsAndroidBackendPrivate;
	TArray<FOpenMobileSensorsAndroidSensorDescriptor> Descriptors;
	if (!QuerySensorDescriptors(Descriptors))
	{
		return {};
	}
	TMap<EOpenMobileSensorType, FString> PreferredIdentifiers;
	for (const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor
		: Descriptors)
	{
		const FString* ExistingIdentifier =
			PreferredIdentifiers.Find(Descriptor.Sensor.Type);
		const FOpenMobileSensorsAndroidSensorDescriptor* Existing =
			ExistingIdentifier
				? Descriptors.FindByPredicate(
					[ExistingIdentifier](const auto& Candidate)
					{
						return Candidate.NativeIdentifier == *ExistingIdentifier;
					}
				)
				: nullptr;
		if (!Existing
			|| DescriptorPreference(
				Descriptor,
				EOpenMobileAttitudeReferenceFrame::GameRelative
			) < DescriptorPreference(
				*Existing,
				EOpenMobileAttitudeReferenceFrame::GameRelative
			))
		{
			PreferredIdentifiers.Add(
				Descriptor.Sensor.Type,
				Descriptor.NativeIdentifier
			);
		}
	}
	TArray<FOpenMobileSensorBackendMetadata> Metadata;
	Metadata.Reserve(Descriptors.Num());
	for (const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor
		: Descriptors)
	{
		FOpenMobileSensorBackendMetadata Entry;
		Entry.Metadata.Sensor = Descriptor.Sensor;
		Entry.Metadata.bPreferred =
			PreferredIdentifiers.FindRef(Descriptor.Sensor.Type)
				== Descriptor.NativeIdentifier;
		Entry.NativeIdentifier = Descriptor.NativeIdentifier;
		SetOptional(
			Entry.Metadata.Vendor,
			Descriptor.Vendor,
			!Descriptor.Vendor.IsEmpty()
		);
		SetOptional(
			Entry.Metadata.NativeName,
			Descriptor.NativeName,
			!Descriptor.NativeName.IsEmpty()
		);
		SetOptional(
			Entry.Metadata.Version,
			static_cast<int64>(Descriptor.Version),
			Descriptor.Version >= 0
		);
		SetOptional(
			Entry.Metadata.MaximumRange,
			Descriptor.MaximumRange,
			FMath::IsFinite(Descriptor.MaximumRange)
				&& Descriptor.MaximumRange >= 0.0
		);
		SetOptional(
			Entry.Metadata.Resolution,
			Descriptor.Resolution,
			FMath::IsFinite(Descriptor.Resolution)
				&& Descriptor.Resolution >= 0.0
		);
		// Android reports current in mA; power needs a voltage estimate.
		SetOptional(
			Entry.Metadata.MinimumIntervalSeconds,
			static_cast<double>(Descriptor.MinimumDelayMicroseconds),
			Descriptor.MinimumDelayMicroseconds > 0
		);
		SetOptional(
			Entry.Metadata.MaximumIntervalSeconds,
			static_cast<double>(Descriptor.MaximumDelayMicroseconds),
			Descriptor.MaximumDelayMicroseconds > 0
		);
		SetOptional(
			Entry.Metadata.FifoCapacitySamples,
			static_cast<int64>(Descriptor.FifoCapacitySamples),
			Descriptor.FifoCapacitySamples >= 0
		);
		Entry.Metadata.WakeUpBehavior.bAvailable = true;
		Entry.Metadata.WakeUpBehavior.bValue = Descriptor.bWakeUp;
		Entry.Metadata.ReportingMode = MapReportingMode(
			Descriptor.NativeReportingMode
		);
		Entry.Metadata.bReportingModeAvailable =
			Entry.Metadata.ReportingMode !=
				EOpenMobileSensorReportingMode::Unknown;
		Entry.MeasurementUnit = Descriptor.Sensor.Type ==
			EOpenMobileSensorType::Proximity
			? EOpenMobileSensorMetadataUnit::Centimeters
			: EOpenMobileSensorMetadataUnit::Portable;
		Entry.IntervalUnit =
			EOpenMobileSensorMetadataTimeUnit::Microseconds;
		Metadata.Add(MoveTemp(Entry));
	}
	return Metadata;
}

bool FOpenMobileSensorsAndroidBackend::
RequiresHighSamplingRateDeclaration() const
{
	return true;
}

bool FOpenMobileSensorsAndroidBackend::HasHighSamplingRateDeclaration() const
{
	return !bShuttingDown.Load()
		&& GetBridge().HasHighSamplingRateDeclaration();
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsAndroidBackend::StartMotionActivityProviderStream(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	FOpenMobileSensorPhysicalStreamRequest& InOutRequest
)
{
	IOpenMobileMotionActivityProvider* Provider =
		FOpenMobileMotionActivityProviderResolver::FindProvider();
	if (!Provider || !Handle.IsValid())
	{
		return FOpenMobileSensorsErrorMapper::Map(
			Provider
				? EOpenMobileSensorFailureReason::InvalidHandle
				: EOpenMobileSensorFailureReason::UnsupportedPlatform,
			TEXT("MotionActivityProvider"),
			Provider ? TEXT("InvalidHandle") : TEXT("ProviderAbsent")
		);
	}
	FOpenMobileMotionActivityProviderStreamHandle ProviderHandle;
	ProviderHandle.Identifier = Handle.Identifier;
	FOpenMobileMotionActivityProviderRequest ProviderRequest;
	ProviderRequest.Sensor = InOutRequest.Sensor;
	ProviderRequest.RequestedFrequencyHz = InOutRequest.RequestedFrequencyHz;
	ProviderRequest.MaximumDeliveryLatencySeconds =
		InOutRequest.MaximumDeliveryLatencySeconds;
	ProviderRequest.bLowLatency = InOutRequest.bLowLatency;
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FName ProviderName = Provider->GetProviderName();
	const FOpenMobileSensorIdentifier RequestedSensor = InOutRequest.Sensor;
	FOpenMobileMotionActivityProviderCallbacks Callbacks;
	Callbacks.OnBatch = FOnOpenMobileMotionActivityProviderBatch::CreateLambda(
		[Token, Handle, ProviderName, RequestedSensor](
			const FOpenMobileActivitySensorBatch& Batch
		)
		{
			FOpenMobileActivitySensorBatch NormalizedBatch;
			NormalizedBatch.Samples.Reserve(Batch.Samples.Num());
			for (FOpenMobileActivitySensorSample Sample : Batch.Samples)
			{
				if (Sample.Header.Sensor.Type != RequestedSensor.Type)
				{
					continue;
				}
				if (Sample.ActivityProvider.IsNone())
				{
					Sample.ActivityProvider = ProviderName;
				}
				if (RequestedSensor.Type ==
						EOpenMobileSensorType::ActivityTransition)
				{
					if (Sample.Transition ==
						EOpenMobileActivityTransition::None)
					{
						continue;
					}
					if (Sample.TransitionOrigin ==
						EOpenMobileActivityTransitionOrigin::Unknown)
					{
						Sample.TransitionOrigin =
							EOpenMobileActivityTransitionOrigin::Native;
					}
				}
				if (Sample.Header.SourceFlags == 0)
				{
					Sample.Header.SourceFlags = static_cast<int32>(
						EOpenMobileSensorSourceFlags::PluginDerived
					);
				}
				FOpenMobileSensorUnitConverter::NormalizeActivitySample(
					EOpenMobileSensorNativePlatform::Android,
					Sample
				);
				NormalizedBatch.Samples.Add(MoveTemp(Sample));
			}
			FOpenMobileSensorsSampleService::PublishActivityBatchFromBackend(
				Token,
				Handle,
				NormalizedBatch
			);
		}
	);
	Callbacks.OnFailure =
		FOnOpenMobileMotionActivityProviderFailure::CreateLambda(
			[Token, Handle](const FOpenMobileSensorOperationResult& Result)
			{
				OpenMobile::DispatchToGameThread(
					[Token, Handle, Result]()
					{
						FOpenMobileSensorsSubscriptionService::
							FailPhysicalStreamFromBackend(
								Token,
								Handle,
								Result
							);
					}
				);
			}
		);
	const FOpenMobileSensorOperationResult Result = Provider->StartStream(
		ProviderHandle,
		ProviderRequest,
		MoveTemp(Callbacks)
	);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	FActiveMotionActivityProviderStream Active;
	Active.Provider = Provider;
	Active.ProviderHandle = ProviderHandle;
	Active.BackendHandle = Handle;
	bool bDuplicate = false;
	{
		FScopeLock Lock(&MotionActivityProviderStreamsMutex);
		if (MotionActivityProviderStreams.Contains(Handle.Identifier))
		{
			bDuplicate = true;
		}
		else
		{
			MotionActivityProviderStreams.Add(
				Handle.Identifier,
				MoveTemp(Active)
			);
		}
	}
	if (bDuplicate)
	{
		Provider->StopStream(ProviderHandle);
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidRequest
		);
	}
	return Result;
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsAndroidBackend::ReconfigureMotionActivityProviderStream(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	FOpenMobileSensorPhysicalStreamRequest& InOutRequest
)
{
	FActiveMotionActivityProviderStream Active;
	{
		FScopeLock Lock(&MotionActivityProviderStreamsMutex);
		const FActiveMotionActivityProviderStream* Found =
			MotionActivityProviderStreams.Find(Handle.Identifier);
		if (!Found)
		{
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::InvalidHandle
			);
		}
		Active = *Found;
	}
	FOpenMobileMotionActivityProviderRequest ProviderRequest;
	ProviderRequest.Sensor = InOutRequest.Sensor;
	ProviderRequest.RequestedFrequencyHz = InOutRequest.RequestedFrequencyHz;
	ProviderRequest.MaximumDeliveryLatencySeconds =
		InOutRequest.MaximumDeliveryLatencySeconds;
	ProviderRequest.bLowLatency = InOutRequest.bLowLatency;
	return Active.Provider->ReconfigureStream(
		Active.ProviderHandle,
		ProviderRequest
	);
}

bool FOpenMobileSensorsAndroidBackend::StopMotionActivityProviderStream(
	const FOpenMobileSensorBackendStreamHandle& Handle
)
{
	FActiveMotionActivityProviderStream Active;
	{
		FScopeLock Lock(&MotionActivityProviderStreamsMutex);
		if (!MotionActivityProviderStreams.RemoveAndCopyValue(
			Handle.Identifier,
			Active
		))
		{
			return false;
		}
	}
	Active.Provider->StopStream(Active.ProviderHandle);
	return true;
}

void FOpenMobileSensorsAndroidBackend::
HandleMotionActivityProviderUnregistered(
	const FName& FeatureName,
	IModularFeature* Feature
)
{
	if (FeatureName !=
		IOpenMobileMotionActivityProvider::GetModularFeatureName())
	{
		return;
	}
	IOpenMobileMotionActivityProvider* Provider =
		static_cast<IOpenMobileMotionActivityProvider*>(Feature);
	TArray<FActiveMotionActivityProviderStream> Removed;
	{
		FScopeLock Lock(&MotionActivityProviderStreamsMutex);
		for (auto Iterator = MotionActivityProviderStreams.CreateIterator();
			Iterator;
			++Iterator)
		{
			if (Iterator.Value().Provider == Provider)
			{
				Removed.Add(Iterator.Value());
				Iterator.RemoveCurrent();
			}
		}
	}
	for (const FActiveMotionActivityProviderStream& Active : Removed)
	{
		Provider->StopStream(Active.ProviderHandle);
	}
	if (Removed.IsEmpty())
	{
		ExecuteOnGameThread(
			TEXT("OpenMobileMotionActivityProviderUnregistered"),
			[]()
			{
				FOpenMobileSensorsCapabilityService::
					HandleBackendGenerationChanged();
			}
		);
		return;
	}
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FOpenMobileSensorOperationResult Failure =
		FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable,
			Provider->GetProviderName().ToString(),
			TEXT("ProviderUnloaded")
		);
	ExecuteOnGameThread(
		TEXT("OpenMobileMotionActivityProviderUnload"),
		[Token, Removed = MoveTemp(Removed), Failure]()
		{
			for (const FActiveMotionActivityProviderStream& Active : Removed)
			{
				FOpenMobileSensorsSubscriptionService::
					FailPhysicalStreamFromBackend(
						Token,
						Active.BackendHandle,
						Failure
					);
			}
			FOpenMobileSensorsCapabilityService::
				HandleBackendGenerationChanged();
		}
	);
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsAndroidBackend::StartSensorStream(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	FOpenMobileSensorPhysicalStreamRequest& InOutRequest
)
{
	if (bShuttingDown.Load())
	{
		return MapBridgeFailure(
			EOpenMobileSensorsAndroidBridgeFailure::ShuttingDown
		);
	}
	if (InOutRequest.Sensor.Type == EOpenMobileSensorType::MotionActivity
		|| InOutRequest.Sensor.Type ==
			EOpenMobileSensorType::ActivityTransition)
	{
		return StartMotionActivityProviderStream(Handle, InOutRequest);
	}
	TArray<FOpenMobileSensorsAndroidSensorDescriptor> Descriptors;
	FOpenMobileSensorOperationResult Failure;
	if (!QuerySensorDescriptors(Descriptors, &Failure))
	{
		return Failure;
	}
	FOpenMobileSensorsAndroidSensorDescriptor Descriptor;
	if (!SelectDescriptor(
		InOutRequest,
		Descriptors,
		Descriptor,
		Failure
	))
	{
		return Failure;
	}
	int32 SamplingPeriodMicroseconds = 0;
	int32 MaximumReportLatencyMicroseconds = 0;
	ResolveNativeRequest(
		Descriptor,
		InOutRequest,
		SamplingPeriodMicroseconds,
		MaximumReportLatencyMicroseconds
	);
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const bool bTrackNativeSteps = Descriptor.Sensor.Type ==
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
	const FOpenMobileSensorsAndroidBridgeResult Result =
		GetBridge().StartStream(
			Token,
			Handle,
			Descriptor,
			SamplingPeriodMicroseconds,
			MaximumReportLatencyMicroseconds,
			InOutRequest.bLowLatency,
			InOutRequest.AttitudeReferenceFrame
		);
	if (!Result.IsSuccess() && bTrackNativeSteps)
	{
		FScopeLock Lock(&NativeStepCountersMutex);
		NativeStepCounters.Remove(Handle.Identifier);
	}
	if (Result.IsSuccess())
	{
		OpenMobileSensorsAndroidBackendPrivate::ApplyAttitudeReferenceState(
			InOutRequest,
			Descriptor
		);
	}
	return Result.IsSuccess()
		? FOpenMobileSensorOperationResult{
			EOpenMobileSensorResultCode::Success
		}
		: MapBridgeFailure(Result.Failure);
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsAndroidBackend::ReconfigureSensorStream(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	FOpenMobileSensorPhysicalStreamRequest& InOutRequest
)
{
	if (InOutRequest.Sensor.Type == EOpenMobileSensorType::MotionActivity
		|| InOutRequest.Sensor.Type ==
			EOpenMobileSensorType::ActivityTransition)
	{
		return ReconfigureMotionActivityProviderStream(Handle, InOutRequest);
	}
	FOpenMobileSensorsAndroidSensorDescriptor Descriptor;
	if (!GetBridge().GetActiveSensorDescriptor(Handle, Descriptor))
	{
		return MapBridgeFailure(
			EOpenMobileSensorsAndroidBridgeFailure::StreamMissing
		);
	}
	TArray<FOpenMobileSensorsAndroidSensorDescriptor> Descriptors;
	FOpenMobileSensorOperationResult Failure;
	if (!QuerySensorDescriptors(Descriptors, &Failure))
	{
		return Failure;
	}
	FOpenMobileSensorsAndroidSensorDescriptor RequestedDescriptor;
	if (!SelectDescriptor(
		InOutRequest,
		Descriptors,
		RequestedDescriptor,
		Failure
	))
	{
		return Failure;
	}
	if (RequestedDescriptor.NativeIdentifier != Descriptor.NativeIdentifier)
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		);
	}
	int32 SamplingPeriodMicroseconds = 0;
	int32 MaximumReportLatencyMicroseconds = 0;
	ResolveNativeRequest(
		Descriptor,
		InOutRequest,
		SamplingPeriodMicroseconds,
		MaximumReportLatencyMicroseconds
	);
	const FOpenMobileSensorsAndroidBridgeResult Result =
		GetBridge().ReconfigureStream(
			Handle,
			SamplingPeriodMicroseconds,
			MaximumReportLatencyMicroseconds,
			InOutRequest.bLowLatency,
			InOutRequest.AttitudeReferenceFrame
		);
	if (Result.IsSuccess())
	{
		OpenMobileSensorsAndroidBackendPrivate::ApplyAttitudeReferenceState(
			InOutRequest,
			RequestedDescriptor
		);
	}
	return Result.IsSuccess()
		? FOpenMobileSensorOperationResult{
			EOpenMobileSensorResultCode::Success
		}
		: MapBridgeFailure(Result.Failure);
}

void FOpenMobileSensorsAndroidBackend::StopSensorStream(
	const FOpenMobileSensorBackendStreamHandle& Handle
)
{
	if (StopMotionActivityProviderStream(Handle))
	{
		return;
	}
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
FOpenMobileSensorsAndroidBackend::FlushSensorStream(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FGuid& RequestId,
	FOnOpenMobileSensorBackendFlushComplete&& Completion
)
{
	{
		FScopeLock Lock(&MotionActivityProviderStreamsMutex);
		if (MotionActivityProviderStreams.Contains(Handle.Identifier))
		{
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::UnsupportedOperation,
				TEXT("MotionActivityProvider"),
				TEXT("FlushUnsupported")
			);
		}
	}
	const FOpenMobileSensorsAndroidBridgeResult Result =
		GetBridge().FlushStream(
			Handle,
			RequestId,
			MoveTemp(Completion)
		);
	return Result.IsSuccess()
		? FOpenMobileSensorOperationResult{
			EOpenMobileSensorResultCode::Accepted
		}
		: MapBridgeFailure(Result.Failure);
}

bool FOpenMobileSensorsAndroidBackend::PublishVectorBatchFromHandler(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileVectorSensorBatch& Batch
)
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
				FOpenMobileSensorSourcePolicy::GetAndroidNativeSourceFlags(
					Sample.Header.Sensor.Type
				);
		}
		FOpenMobileSensorUnitConverter::NormalizeVectorSample(
			EOpenMobileSensorNativePlatform::Android,
			Sample
		);
		FOpenMobileSensorCoordinateConverter::ConvertVectorSample(
			EOpenMobileSensorNativePlatform::Android,
			Sample
		);
	}
	return FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
		Token,
		Handle,
		NormalizedBatch
	);
}

bool FOpenMobileSensorsAndroidBackend::PublishAccuracyFromHandler(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileSensorIdentifier& Sensor,
	int32 NativeAccuracy,
	double TimestampSeconds
)
{
	if (bShuttingDown.Load())
	{
		return false;
	}
	return FOpenMobileSensorsSampleService::PublishAccuracyFromBackend(
		Token,
		Handle,
		FOpenMobileSensorAccuracyMapper::FromAndroidAccuracyCallback(
			Sensor,
			NativeAccuracy,
			TimestampSeconds
		)
	);
}

bool FOpenMobileSensorsAndroidBackend::PublishCompactBatchFromHandler(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor,
	int32 SampleCount,
	int32 ValuesPerSample,
	int32 ValueStride,
	TArray<int64>&& TimestampsNanoseconds,
	TArray<float>&& Values,
	bool bResetFirstSample,
	EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame
)
{
	using namespace OpenMobileSensorsAndroidBackendPrivate;
	if (bShuttingDown.Load()
		|| SampleCount <= 0
		|| SampleCount > 64
		|| ValuesPerSample <= 0
		|| ValuesPerSample > 6
		|| ValueStride < ValuesPerSample
		|| ValueStride > 6
		|| TimestampsNanoseconds.Num() != SampleCount
		|| Values.Num() != SampleCount * ValueStride)
	{
		return false;
	}
	auto ValueAt = [&Values, ValueStride](int32 Sample, int32 Field)
	{
		return static_cast<double>(Values[Sample * ValueStride + Field]);
	};
	const EOpenMobileSensorType Type = Descriptor.Sensor.Type;
	if (Type == EOpenMobileSensorType::Accelerometer
		|| Type == EOpenMobileSensorType::AccelerometerUncalibrated
		|| Type == EOpenMobileSensorType::Gyroscope
		|| Type == EOpenMobileSensorType::GyroscopeUncalibrated
		|| Type == EOpenMobileSensorType::Magnetometer
		|| Type == EOpenMobileSensorType::MagnetometerUncalibrated
		|| Type == EOpenMobileSensorType::Gravity
		|| Type == EOpenMobileSensorType::LinearAcceleration)
	{
		if (ValuesPerSample < 3)
		{
			return false;
		}
		FOpenMobileVectorSensorBatch Batch;
		Batch.Samples.Reserve(SampleCount);
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			FOpenMobileVectorSensorSample Sample;
			Sample.Header = MakeHeader(
				Descriptor,
				TimestampsNanoseconds[Index],
				bResetFirstSample && Index == 0
			);
			Sample.Value = FVector(
				ValueAt(Index, 0),
				ValueAt(Index, 1),
				ValueAt(Index, 2)
			);
			if (FMath::IsFinite(Descriptor.MaximumRange)
				&& Descriptor.MaximumRange > 0.0)
			{
				Sample.Header.bValid &=
					FOpenMobileSensorValidity::IsWithinMaximumRange(
						Sample.Value,
						Descriptor.MaximumRange
					);
			}
			Sample.bHasBias = ValuesPerSample >= 6
				&& (Type == EOpenMobileSensorType::AccelerometerUncalibrated
					|| Type == EOpenMobileSensorType::GyroscopeUncalibrated
					|| Type == EOpenMobileSensorType::MagnetometerUncalibrated);
			if (Sample.bHasBias)
			{
				Sample.Bias = FVector(
					ValueAt(Index, 3),
					ValueAt(Index, 4),
					ValueAt(Index, 5)
				);
			}
			Batch.Samples.Add(MoveTemp(Sample));
		}
		return PublishVectorBatchFromHandler(Token, Handle, Batch);
	}
	if (Type == EOpenMobileSensorType::Attitude)
	{
		if (ValuesPerSample < 3)
		{
			return false;
		}
		FOpenMobileAttitudeSensorBatch Batch;
		Batch.Samples.Reserve(SampleCount);
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			FOpenMobileAttitudeSensorSample Sample;
			Sample.Header = MakeHeader(
				Descriptor,
				TimestampsNanoseconds[Index],
				bResetFirstSample && Index == 0
			);
			const double X = ValueAt(Index, 0);
			const double Y = ValueAt(Index, 1);
			const double Z = ValueAt(Index, 2);
			const double W = ValuesPerSample >= 4
				? ValueAt(Index, 3)
				: FMath::Sqrt(FMath::Max(0.0, 1.0 - X * X - Y * Y - Z * Z));
			Sample.Quaternion = FQuat(X, Y, Z, W);
			Sample.ReferenceFrame = AttitudeReferenceFrame;
			FOpenMobileSensorUnitConverter::NormalizeAttitudeSample(
				EOpenMobileSensorNativePlatform::Android,
				Sample
			);
			FOpenMobileSensorCoordinateConverter::ConvertAttitudeSample(
				EOpenMobileSensorNativePlatform::Android,
				Sample
			);
			Batch.Samples.Add(MoveTemp(Sample));
		}
		return FOpenMobileSensorsSampleService::PublishAttitudeBatchFromBackend(
			Token,
			Handle,
			Batch
		);
	}
	if (Type == EOpenMobileSensorType::BarometricPressure
		|| Type == EOpenMobileSensorType::AmbientLight)
	{
		FOpenMobileScalarSensorBatch Batch;
		Batch.Samples.Reserve(SampleCount);
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			FOpenMobileScalarSensorSample Sample;
			Sample.Header = MakeHeader(
				Descriptor,
				TimestampsNanoseconds[Index],
				bResetFirstSample && Index == 0
			);
			Sample.Value = ValueAt(Index, 0);
			if (Type == EOpenMobileSensorType::AmbientLight
				&& FMath::IsFinite(Descriptor.MaximumRange)
				&& Descriptor.MaximumRange > 0.0)
			{
				Sample.Header.bValid &=
					FOpenMobileSensorValidity::IsWithinMaximumRange(
						Sample.Value,
						Descriptor.MaximumRange
					);
			}
			FOpenMobileSensorUnitConverter::NormalizeScalarSample(
				EOpenMobileSensorNativePlatform::Android,
				Sample
			);
			Batch.Samples.Add(MoveTemp(Sample));
		}
		return FOpenMobileSensorsSampleService::PublishScalarBatchFromBackend(
			Token,
			Handle,
			Batch
		);
	}
	if (Type == EOpenMobileSensorType::MagneticHeading)
	{
		if ((Descriptor.NativeType != 11 && Descriptor.NativeType != 20)
			|| ValuesPerSample < 3)
		{
			return false;
		}
		FOpenMobileHeadingSensorBatch Batch;
		Batch.Samples.Reserve(SampleCount);
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			FOpenMobileHeadingSensorSample Sample;
			Sample.Header = MakeHeader(
				Descriptor,
				TimestampsNanoseconds[Index],
				bResetFirstSample && Index == 0
			);
			const double X = ValueAt(Index, 0);
			const double Y = ValueAt(Index, 1);
			const double Z = ValueAt(Index, 2);
			const double W = ValuesPerSample >= 4
				? ValueAt(Index, 3)
				: FMath::Sqrt(FMath::Max(
					0.0,
					1.0 - X * X - Y * Y - Z * Z
				));
			Sample.Header.bValid &=
				FOpenMobileSensorHeading::FromAndroidRotationVector(
					FQuat(X, Y, Z, W),
					Sample.HeadingDegrees
				);
			Sample.Reference = EOpenMobileHeadingReference::MagneticNorth;
			Sample.bTiltCompensated = true;
			Sample.bHasAccuracyDegrees = ValuesPerSample >= 5
				&& FMath::IsFinite(ValueAt(Index, 4))
				&& ValueAt(Index, 4) >= 0.0;
			if (Sample.bHasAccuracyDegrees)
			{
				Sample.AccuracyDegrees = FMath::RadiansToDegrees(
					ValueAt(Index, 4)
				);
			}
			FOpenMobileSensorUnitConverter::NormalizeHeadingSample(
				EOpenMobileSensorNativePlatform::Android,
				Sample
			);
			Batch.Samples.Add(MoveTemp(Sample));
		}
		return FOpenMobileSensorsSampleService::PublishHeadingBatchFromBackend(
			Token,
			Handle,
			Batch
		);
	}
	if (Type == EOpenMobileSensorType::TrueHeading)
	{
		if (Descriptor.NativeType != 42 || ValuesPerSample < 1)
		{
			return false;
		}
		FOpenMobileHeadingSensorBatch Batch;
		Batch.Samples.Reserve(SampleCount);
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			FOpenMobileHeadingSensorSample Sample;
			Sample.Header = MakeHeader(
				Descriptor,
				TimestampsNanoseconds[Index],
				bResetFirstSample && Index == 0
			);
			Sample.HeadingDegrees = ValueAt(Index, 0);
			Sample.Reference = EOpenMobileHeadingReference::TrueNorth;
			Sample.bHasAccuracyDegrees = ValuesPerSample >= 2;
			if (Sample.bHasAccuracyDegrees)
			{
				Sample.AccuracyDegrees = ValueAt(Index, 1);
			}
			FOpenMobileSensorUnitConverter::NormalizeHeadingSample(
				EOpenMobileSensorNativePlatform::Android,
				Sample
			);
			Batch.Samples.Add(MoveTemp(Sample));
		}
		return FOpenMobileSensorsSampleService::PublishHeadingBatchFromBackend(
			Token,
			Handle,
			Batch
		);
	}
	if (Type == EOpenMobileSensorType::StepCounter
		|| Type == EOpenMobileSensorType::StepDetector)
	{
		FOpenMobileStepsSensorBatch Batch;
		Batch.Samples.Reserve(SampleCount);
		{
			FScopeLock Lock(&NativeStepCountersMutex);
			FOpenMobileNativeStepCounterTracker* StepCounterTracker =
				Type == EOpenMobileSensorType::StepCounter
				? NativeStepCounters.Find(Handle.Identifier)
				: nullptr;
			if (Type == EOpenMobileSensorType::StepCounter && !StepCounterTracker)
			{
				return false;
			}
			for (int32 Index = 0; Index < SampleCount; ++Index)
			{
				FOpenMobileStepsSensorSample Sample;
				Sample.Header = MakeHeader(
					Descriptor,
					TimestampsNanoseconds[Index],
					bResetFirstSample && Index == 0
				);
				const double Count = ValueAt(Index, 0);
				Sample.Header.bValid &=
					FOpenMobileNativeStepCounterTracker::TryConvertNativeTotal(
						Count,
						Sample.Count
					);
				if (Type == EOpenMobileSensorType::StepCounter)
				{
					Sample.Origin = EOpenMobileStepCountOrigin::DeviceBoot;
					FOpenMobileNativeStepCounterTracker& Tracker =
						*StepCounterTracker;
					Sample.Header.bValid &= Tracker.Apply(Sample);
				}
				else
				{
					Sample.Origin = EOpenMobileStepCountOrigin::Session;
					Sample.OriginIdentifier = Handle.Identifier;
					Sample.DetectedStepDelta = Sample.Count;
					Sample.DetectionSource =
						EOpenMobileStepDetectionSource::AndroidStepDetector;
					Sample.DetectionQuality = EOpenMobileStepDetectionQuality::
						DirectHardwareEvent;
					Sample.Header.bValid &= Sample.DetectedStepDelta > 0;
				}
				FOpenMobileSensorUnitConverter::NormalizeStepsSample(
					EOpenMobileSensorNativePlatform::Android,
					Sample
				);
				Batch.Samples.Add(MoveTemp(Sample));
			}
		}
		return FOpenMobileSensorsSampleService::PublishStepsBatchFromBackend(
			Token,
			Handle,
			Batch
		);
	}
	if (Type == EOpenMobileSensorType::Proximity)
	{
		FOpenMobileProximitySensorBatch Batch;
		Batch.Samples.Reserve(SampleCount);
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			FOpenMobileProximitySensorSample Sample;
			Sample.Header = MakeHeader(
				Descriptor,
				TimestampsNanoseconds[Index],
				bResetFirstSample && Index == 0
			);
			Sample.bHasDistanceMeters = true;
			Sample.DistanceMeters = ValueAt(Index, 0);
			Sample.bHasMaximumRangeMeters =
				FMath::IsFinite(Descriptor.MaximumRange)
				&& Descriptor.MaximumRange >= 0.0;
			Sample.MaximumRangeMeters = Descriptor.MaximumRange;
			FOpenMobileSensorUnitConverter::NormalizeProximitySample(
				EOpenMobileSensorNativePlatform::Android,
				Sample
			);
			Batch.Samples.Add(MoveTemp(Sample));
		}
		return FOpenMobileSensorsSampleService::PublishProximityBatchFromBackend(
			Token,
			Handle,
			Batch
		);
	}
	return false;
}

void FOpenMobileSensorsAndroidBackend::
HandlePhysicalStreamFailureFromHandler(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& Handle,
	EOpenMobileSensorsAndroidBridgeFailure Failure,
	FString NativeCode
)
{
	{
		FScopeLock Lock(&NativeStepCountersMutex);
		NativeStepCounters.Remove(Handle.Identifier);
	}
	const FOpenMobileSensorOperationResult Operation = MapBridgeFailure(
		Failure,
		MoveTemp(NativeCode)
	);
	OpenMobile::DispatchToGameThread(
		[Token, Handle, Operation]()
		{
			FOpenMobileSensorsSubscriptionService::
				FailPhysicalStreamFromBackend(Token, Handle, Operation);
			FOpenMobileSensorsCapabilityService::
				HandleBackendGenerationChanged();
			FOpenMobileSensorsMetadataService::
				HandleBackendGenerationChanged();
		}
	);
}

void FOpenMobileSensorsAndroidBackend::HandleSensorsChangedFromHandler()
{
	OpenMobile::DispatchToGameThread(
		[]()
		{
			FOpenMobileSensorsCapabilityService::
				HandleBackendGenerationChanged();
			FOpenMobileSensorsMetadataService::
				HandleBackendGenerationChanged();
		}
	);
}

void FOpenMobileSensorsAndroidBackend::BeginShutdown()
{
	if (bShuttingDown.Exchange(true))
	{
		return;
	}
	if (MotionActivityProviderUnregisteredHandle.IsValid())
	{
		IModularFeatures::Get().OnModularFeatureUnregistered().Remove(
			MotionActivityProviderUnregisteredHandle
		);
		MotionActivityProviderUnregisteredHandle.Reset();
	}
	if (MotionActivityProviderRegisteredHandle.IsValid())
	{
		IModularFeatures::Get().OnModularFeatureRegistered().Remove(
			MotionActivityProviderRegisteredHandle
		);
		MotionActivityProviderRegisteredHandle.Reset();
	}
	TArray<FActiveMotionActivityProviderStream> ProviderStreams;
	{
		FScopeLock Lock(&MotionActivityProviderStreamsMutex);
		MotionActivityProviderStreams.GenerateValueArray(ProviderStreams);
		MotionActivityProviderStreams.Reset();
	}
	for (const FActiveMotionActivityProviderStream& Active : ProviderStreams)
	{
		Active.Provider->StopStream(Active.ProviderHandle);
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

FOpenMobileSensorsAndroidBridge&
FOpenMobileSensorsAndroidBackend::GetBridge() const
{
	if (!Bridge)
	{
		Bridge = MakeUnique<FOpenMobileSensorsAndroidBridge>(
			const_cast<FOpenMobileSensorsAndroidBackend&>(*this)
		);
	}
	return *Bridge;
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsAndroidBackend::MapBridgeFailure(
	EOpenMobileSensorsAndroidBridgeFailure Failure,
	FString NativeCode
) const
{
	using namespace OpenMobileSensorsAndroidBackendPrivate;
	EOpenMobileSensorFailureReason Reason =
		EOpenMobileSensorFailureReason::OperationalFailure;
	switch (Failure)
	{
	case EOpenMobileSensorsAndroidBridgeFailure::InvalidArgument:
		Reason = EOpenMobileSensorFailureReason::InvalidRequest;
		break;
	case EOpenMobileSensorsAndroidBridgeFailure::SensorMissing:
		Reason = EOpenMobileSensorFailureReason::MissingHardware;
		break;
	case EOpenMobileSensorsAndroidBridgeFailure::PermissionDenied:
		Reason = EOpenMobileSensorFailureReason::PermissionDenied;
		break;
	case EOpenMobileSensorsAndroidBridgeFailure::StreamMissing:
		Reason = EOpenMobileSensorFailureReason::InvalidHandle;
		break;
	case EOpenMobileSensorsAndroidBridgeFailure::ActivityUnavailable:
	case EOpenMobileSensorsAndroidBridgeFailure::Paused:
	case EOpenMobileSensorsAndroidBridgeFailure::ShuttingDown:
	case EOpenMobileSensorsAndroidBridgeFailure::Timeout:
		Reason = EOpenMobileSensorFailureReason::TemporarilyUnavailable;
		break;
	case EOpenMobileSensorsAndroidBridgeFailure::None:
	{
		FOpenMobileSensorOperationResult Success;
		Success.Code = EOpenMobileSensorResultCode::Success;
		return Success;
	}
	case EOpenMobileSensorsAndroidBridgeFailure::BridgeClassMissing:
	case EOpenMobileSensorsAndroidBridgeFailure::BridgeMethodMissing:
	case EOpenMobileSensorsAndroidBridgeFailure::BridgeCreateFailed:
	case EOpenMobileSensorsAndroidBridgeFailure::JavaException:
	case EOpenMobileSensorsAndroidBridgeFailure::InvalidPayload:
	case EOpenMobileSensorsAndroidBridgeFailure::RegisterFailed:
	case EOpenMobileSensorsAndroidBridgeFailure::FlushFailed:
	default:
		break;
	}
	return FOpenMobileSensorsErrorMapper::Map(
		Reason,
		TEXT("Android"),
		NativeCode.IsEmpty() ? FailureCode(Failure) : MoveTemp(NativeCode)
	);
}

bool FOpenMobileSensorsAndroidBackend::QuerySensorDescriptors(
	TArray<FOpenMobileSensorsAndroidSensorDescriptor>& OutDescriptors,
	FOpenMobileSensorOperationResult* OutFailure
) const
{
	if (bShuttingDown.Load())
	{
		const FOpenMobileSensorOperationResult Failure = MapBridgeFailure(
			EOpenMobileSensorsAndroidBridgeFailure::ShuttingDown
		);
		if (OutFailure)
		{
			*OutFailure = Failure;
		}
		return false;
	}
	const FOpenMobileSensorsAndroidBridgeResult Result =
		GetBridge().QuerySensors(OutDescriptors);
	LastBridgeFailure.Store(static_cast<uint8>(Result.Failure));
	if (!Result.IsSuccess())
	{
		if (OutFailure)
		{
			*OutFailure = MapBridgeFailure(Result.Failure);
		}
		return false;
	}
	OpenMobileSensorsAndroidBackendPrivate::AddMagneticHeadingDescriptors(
		OutDescriptors
	);
	return true;
}

bool FOpenMobileSensorsAndroidBackend::SelectDescriptor(
	const FOpenMobileSensorPhysicalStreamRequest& Request,
	const TArray<FOpenMobileSensorsAndroidSensorDescriptor>& Descriptors,
	FOpenMobileSensorsAndroidSensorDescriptor& OutDescriptor,
	FOpenMobileSensorOperationResult& OutFailure
) const
{
	using namespace OpenMobileSensorsAndroidBackendPrivate;
	if (Request.Sensor.Type == EOpenMobileSensorType::Attitude
		&& Request.AttitudeReferenceFrame ==
			EOpenMobileAttitudeReferenceFrame::TrueNorth)
	{
		OutFailure = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidReferenceFrame,
			TEXT("Android"),
			TEXT("TrueNorthUnavailable")
		);
		return false;
	}
	const bool bSpecificInstance = !Request.Sensor.InstanceId.IsNone()
		&& Request.Sensor.InstanceId != TEXT("Default");
	const FOpenMobileSensorsAndroidSensorDescriptor* Best = nullptr;
	int32 BestScore = TNumericLimits<int32>::Max();
	for (const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor
		: Descriptors)
	{
		if (Descriptor.Sensor.Type != Request.Sensor.Type
			|| (bSpecificInstance
				&& Descriptor.Sensor.InstanceId != Request.Sensor.InstanceId))
		{
			continue;
		}
		const int32 Score = DescriptorPreference(
			Descriptor,
			Request.AttitudeReferenceFrame
		);
		if (Score >= 10000)
		{
			continue;
		}
		if (!Best
			|| Score < BestScore
			|| (Score == BestScore
				&& Descriptor.NativeIdentifier < Best->NativeIdentifier))
		{
			Best = &Descriptor;
			BestScore = Score;
		}
	}
	if (!Best)
	{
		OutFailure = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::MissingHardware,
			TEXT("Android"),
			TEXT("SensorMissing")
		);
		return false;
	}
	OutDescriptor = *Best;
	return true;
}

void FOpenMobileSensorsAndroidBackend::ResolveNativeRequest(
	const FOpenMobileSensorsAndroidSensorDescriptor& Descriptor,
	FOpenMobileSensorPhysicalStreamRequest& InOutRequest,
	int32& OutSamplingPeriodMicroseconds,
	int32& OutMaximumReportLatencyMicroseconds
) const
{
	using namespace OpenMobileSensorsAndroidBackendPrivate;
	const int64 RequestedPeriod = FMath::Clamp<int64>(
		FMath::RoundToInt64(
			MicrosecondsPerSecond / InOutRequest.RequestedFrequencyHz
		),
		1,
		MAX_int32
	);
	int64 AppliedPeriod = RequestedPeriod;
	if (Descriptor.MinimumDelayMicroseconds > 0)
	{
		AppliedPeriod = FMath::Max<int64>(
			AppliedPeriod,
			Descriptor.MinimumDelayMicroseconds
		);
	}
	if (Descriptor.MaximumDelayMicroseconds > 0)
	{
		AppliedPeriod = FMath::Min<int64>(
			AppliedPeriod,
			Descriptor.MaximumDelayMicroseconds
		);
	}
	OutSamplingPeriodMicroseconds = static_cast<int32>(AppliedPeriod);
	if (AppliedPeriod != RequestedPeriod)
	{
		InOutRequest.AppliedRateAdjustmentReason =
			EOpenMobileSensorRateAdjustmentReason::HardwareLimit;
	}
	InOutRequest.RequestedFrequencyHz =
		MicrosecondsPerSecond / AppliedPeriod;
	InOutRequest.bNativeBatchingApplied =
		InOutRequest.bNativeBatchingRequested
		&& !InOutRequest.bLowLatency
		&& InOutRequest.MaximumDeliveryLatencySeconds > 0.0
		&& Descriptor.FifoCapacitySamples > 0;
	OutMaximumReportLatencyMicroseconds =
		InOutRequest.bNativeBatchingApplied
		? static_cast<int32>(FMath::Clamp<int64>(
			FMath::RoundToInt64(
				InOutRequest.MaximumDeliveryLatencySeconds
					* MicrosecondsPerSecond
			),
			0,
			MAX_int32
		))
		: 0;
}
