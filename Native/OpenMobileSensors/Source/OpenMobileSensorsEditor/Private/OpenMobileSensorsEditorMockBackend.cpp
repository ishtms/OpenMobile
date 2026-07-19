#include "OpenMobileSensorsEditorMockBackend.h"

#include "Algo/BinarySearch.h"
#include "HAL/PlatformTime.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorQuality.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsEditorMockBackendPrivate
{
	constexpr int32 MockPriority = 10000;
	constexpr double MinimumFrequencyHz = 1.0;
	constexpr double MaximumFrequencyHz = 120.0;
	constexpr int32 MaximumTimelineFrames = 1024;
	constexpr double MaximumTimelineSeconds = 3600.0;

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X)
			&& FMath::IsFinite(Value.Y)
			&& FMath::IsFinite(Value.Z);
	}

	bool IsFiniteRotator(const FRotator& Value)
	{
		return FMath::IsFinite(Value.Pitch)
			&& FMath::IsFinite(Value.Yaw)
			&& FMath::IsFinite(Value.Roll);
	}

	bool IsPermissionLoss(EOpenMobilePermissionStatus Status)
	{
		return Status == EOpenMobilePermissionStatus::Denied
			|| Status == EOpenMobilePermissionStatus::Restricted
			|| Status == EOpenMobilePermissionStatus::PermanentlyDenied;
	}

	int32 MockFlags(EOpenMobileSensorType Sensor)
	{
		const int32 Mock = static_cast<int32>(
			EOpenMobileSensorSourceFlags::Mock);
		switch (Sensor)
		{
		case EOpenMobileSensorType::Attitude:
			return Mock | static_cast<int32>(
				EOpenMobileSensorSourceFlags::NativeFused);
		case EOpenMobileSensorType::MagneticHeading:
			return Mock
				| static_cast<int32>(
					EOpenMobileSensorSourceFlags::NativeFused)
				| static_cast<int32>(
					EOpenMobileSensorSourceFlags::MagneticNorthReferenced);
		case EOpenMobileSensorType::TrueHeading:
			return Mock
				| static_cast<int32>(
					EOpenMobileSensorSourceFlags::NativeFused)
				| static_cast<int32>(
					EOpenMobileSensorSourceFlags::TrueNorthReferenced);
		case EOpenMobileSensorType::Accelerometer:
		case EOpenMobileSensorType::Gyroscope:
			return Mock | static_cast<int32>(
				EOpenMobileSensorSourceFlags::CalibratedNative);
		default:
			return Mock | static_cast<int32>(
				EOpenMobileSensorSourceFlags::Raw);
		}
	}
}

void FOpenMobileSensorsEditorMockBackend::Activate()
{
	bActive = true;
}

FName FOpenMobileSensorsEditorMockBackend::GetBackendName() const
{
	return TEXT("OpenMobileEditorMock");
}

int32 FOpenMobileSensorsEditorMockBackend::GetPriority() const
{
	return OpenMobileSensorsEditorMockBackendPrivate::MockPriority;
}

bool FOpenMobileSensorsEditorMockBackend::IsAvailable() const
{
	return bActive;
}

FOpenMobileCapability
FOpenMobileSensorsEditorMockBackend::GetBackendCapability() const
{
	FOpenMobileCapability Capability;
	Capability.Name = IOpenMobileSensorsBackend::GetModularFeatureName();
	Capability.State = bActive
		? EOpenMobileCapabilityState::Available
		: EOpenMobileCapabilityState::TemporarilyUnavailable;
	Capability.Detail = bActive
		? TEXT("Explicit Editor mock sensor input is active.")
		: TEXT("Editor mock sensor input is disabled.");
	return Capability;
}

TArray<FOpenMobileSensorCapability>
FOpenMobileSensorsEditorMockBackend::GetSensorCapabilities() const
{
	TArray<FOpenMobileSensorCapability> Capabilities;
	for (const EOpenMobileSensorType Sensor : {
		EOpenMobileSensorType::Accelerometer,
		EOpenMobileSensorType::Gyroscope,
		EOpenMobileSensorType::Attitude,
		EOpenMobileSensorType::MagneticHeading,
		EOpenMobileSensorType::TrueHeading,
		EOpenMobileSensorType::StepCounter,
		EOpenMobileSensorType::StepDetector,
		EOpenMobileSensorType::Pedometer,
		EOpenMobileSensorType::MotionActivity,
		EOpenMobileSensorType::BarometricPressure,
		EOpenMobileSensorType::AmbientLight,
		EOpenMobileSensorType::Proximity
	})
	{
		Capabilities.Add(MakeCapability(Sensor));
	}
	return Capabilities;
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsEditorMockBackend::StartSensorStream(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	FOpenMobileSensorPhysicalStreamRequest& InOutRequest
)
{
	using namespace OpenMobileSensorsEditorMockBackendPrivate;
	if (!bActive
		|| !Handle.IsValid()
		|| !IsSupportedSensor(InOutRequest.Sensor.Type)
		|| !FMath::IsFinite(InOutRequest.RequestedFrequencyHz)
		|| InOutRequest.RequestedFrequencyHz <= 0.0)
	{
		return MakeFailure(
			EOpenMobileSensorFailureReason::InvalidRequest,
			TEXT("InvalidMockStreamRequest")
		);
	}
	InOutRequest.RequestedFrequencyHz = FMath::Clamp(
		InOutRequest.RequestedFrequencyHz,
		MinimumFrequencyHz,
		MaximumFrequencyHz
	);
	InOutRequest.bNativeBatchingApplied = false;
	if (InOutRequest.Sensor.Type == EOpenMobileSensorType::Attitude)
	{
		InOutRequest.AttitudeReferenceState.RequestedReferenceFrame =
			InOutRequest.AttitudeReferenceFrame;
		InOutRequest.AttitudeReferenceState.AppliedReferenceFrame =
			InOutRequest.AttitudeReferenceFrame;
		InOutRequest.AttitudeReferenceState.bHeadingDependent =
			InOutRequest.AttitudeReferenceFrame ==
				EOpenMobileAttitudeReferenceFrame::MagneticNorth
			|| InOutRequest.AttitudeReferenceFrame ==
				EOpenMobileAttitudeReferenceFrame::TrueNorth;
		InOutRequest.AttitudeReferenceState.bLocationDependent =
			InOutRequest.AttitudeReferenceFrame ==
				EOpenMobileAttitudeReferenceFrame::TrueNorth;
	}
	FActiveStream Stream;
	Stream.Request = InOutRequest;
	Stream.NextEmissionSeconds = FPlatformTime::Seconds();
	Stream.LastStepCount = CurrentInput.StepCount;
	FActiveStream& AddedStream = ActiveStreams.Add(Handle, MoveTemp(Stream));
	EnsureTicker();
	PublishStream(Handle, AddedStream, FPlatformTime::Seconds(), true);
	return MakeSuccess();
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsEditorMockBackend::ReconfigureSensorStream(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	FOpenMobileSensorPhysicalStreamRequest& InOutRequest
)
{
	using namespace OpenMobileSensorsEditorMockBackendPrivate;
	FActiveStream* Stream = ActiveStreams.Find(Handle);
	if (!Stream)
	{
		return MakeFailure(
			EOpenMobileSensorFailureReason::InvalidHandle,
			TEXT("InvalidMockStreamHandle")
		);
	}
	if (!FMath::IsFinite(InOutRequest.RequestedFrequencyHz)
		|| InOutRequest.RequestedFrequencyHz <= 0.0)
	{
		return MakeFailure(
			EOpenMobileSensorFailureReason::InvalidFrequency,
			TEXT("InvalidMockStreamFrequency")
		);
	}
	InOutRequest.RequestedFrequencyHz = FMath::Clamp(
		InOutRequest.RequestedFrequencyHz,
		MinimumFrequencyHz,
		MaximumFrequencyHz
	);
	InOutRequest.bNativeBatchingApplied = false;
	Stream->Request = InOutRequest;
	Stream->NextEmissionSeconds = FPlatformTime::Seconds();
	PublishStream(Handle, *Stream, FPlatformTime::Seconds(), true);
	return MakeSuccess();
}

void FOpenMobileSensorsEditorMockBackend::StopSensorStream(
	const FOpenMobileSensorBackendStreamHandle& Handle
)
{
	ActiveStreams.Remove(Handle);
	if (ActiveStreams.IsEmpty())
	{
		StopTicker();
	}
}

void FOpenMobileSensorsEditorMockBackend::BeginShutdown()
{
	bActive = false;
	bTimelinePlaying = false;
	ActiveTimeline = {};
	ActiveStreams.Reset();
	StopTicker();
}

FName FOpenMobileSensorsEditorMockBackend::GetProviderName() const
{
	return GetBackendName();
}

bool FOpenMobileSensorsEditorMockBackend::SupportsPermission(
	FName Permission
) const
{
	return Permission == FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::MotionActivity)
		|| Permission == FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::ActivityRecognition)
		|| Permission == FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::TrueHeadingLocation);
}

FOpenMobilePermissionResult
FOpenMobileSensorsEditorMockBackend::GetStatus(FName Permission) const
{
	FOpenMobilePermissionResult Result;
	Result.Permission = Permission;
	Result.Status = GetPermissionStatus(Permission);
	if (!SupportsPermission(Permission))
	{
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The Editor mock does not support this permission."),
			{},
			TEXT("OpenMobileEditorMock")
		);
	}
	return Result;
}

bool FOpenMobileSensorsEditorMockBackend::RequestPermission(
	FName Permission,
	const FGuid& RequestIdentifier,
	FOpenMobileNativePermissionCompletion&& Completion,
	FOpenMobileError& OutError
)
{
	static_cast<void>(RequestIdentifier);
	if (!bActive || !SupportsPermission(Permission))
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The Editor mock permission is unavailable."),
			{},
			TEXT("OpenMobileEditorMock")
		);
		return false;
	}
	Completion.ExecuteIfBound(GetPermissionStatus(Permission), {});
	return true;
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsEditorMockBackend::ApplyInput(
	const FOpenMobileSensorsMockInput& Input
)
{
	if (!bActive)
	{
		return MakeFailure(
			EOpenMobileSensorFailureReason::ConfigurationBlocked,
			TEXT("MockInputInactive")
		);
	}
	if (!ValidateInput(Input))
	{
		return MakeFailure(
			EOpenMobileSensorFailureReason::InvalidRequest,
			TEXT("InvalidMockInput")
		);
	}
	const FOpenMobileSensorsMockInput Previous = CurrentInput;
	CurrentInput = Input;
	bTimelinePlaying = false;
	ActiveTimeline = {};
	ManualTimelineElapsedSeconds = 0.0;
	if (ActiveStreams.IsEmpty())
	{
		StopTicker();
	}
	ApplyPermissionChanges(Previous, CurrentInput);
	PublishAll(FPlatformTime::Seconds(), true);
	return MakeSuccess();
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsEditorMockBackend::ApplyPreset(
	EOpenMobileSensorsMockPreset Preset
)
{
	if (Preset == EOpenMobileSensorsMockPreset::Custom
		|| !StaticEnum<EOpenMobileSensorsMockPreset>()->IsValidEnumValue(
			static_cast<int64>(Preset)))
	{
		return MakeFailure(
			EOpenMobileSensorFailureReason::InvalidRequest,
			TEXT("InvalidMockPreset")
		);
	}
	return ApplyInput(MakePreset(Preset));
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsEditorMockBackend::PlayTimeline(
	const FOpenMobileSensorsMockTimeline& Timeline
)
{
	if (!bActive)
	{
		return MakeFailure(
			EOpenMobileSensorFailureReason::ConfigurationBlocked,
			TEXT("MockInputInactive")
		);
	}
	if (!ValidateTimeline(Timeline))
	{
		return MakeFailure(
			EOpenMobileSensorFailureReason::InvalidRequest,
			TEXT("InvalidMockTimeline")
		);
	}
	ActiveTimeline = Timeline;
	TimelineStartSeconds = FPlatformTime::Seconds();
	ManualTimelineElapsedSeconds = 0.0;
	bTimelinePlaying = ActiveTimeline.Frames.Num() > 1;
	const FOpenMobileSensorsMockInput Previous = CurrentInput;
	CurrentInput = ActiveTimeline.Frames[0].Input;
	ApplyPermissionChanges(Previous, CurrentInput);
	PublishAll(TimelineStartSeconds, true);
	EnsureTicker();
	return MakeSuccess();
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsEditorMockBackend::StopTimeline()
{
	if (!bActive)
	{
		return MakeFailure(
			EOpenMobileSensorFailureReason::ConfigurationBlocked,
			TEXT("MockInputInactive")
		);
	}
	bTimelinePlaying = false;
	ActiveTimeline = {};
	ManualTimelineElapsedSeconds = 0.0;
	if (ActiveStreams.IsEmpty())
	{
		StopTicker();
	}
	return MakeSuccess();
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsEditorMockBackend::AdvanceTimeline(double DeltaSeconds)
{
	using namespace OpenMobileSensorsEditorMockBackendPrivate;
	if (!bActive
		|| !bTimelinePlaying
		|| !ActiveTimeline.bUseManualClock
		|| !FMath::IsFinite(DeltaSeconds)
		|| DeltaSeconds < 0.0
		|| DeltaSeconds > MaximumTimelineSeconds)
	{
		return MakeFailure(
			EOpenMobileSensorFailureReason::InvalidRequest,
			TEXT("InvalidMockTimelineAdvance")
		);
	}
	ManualTimelineElapsedSeconds += DeltaSeconds;
	ResolveTimelinePosition(
		ManualTimelineElapsedSeconds * ActiveTimeline.PlaybackSpeed
	);
	PublishAll(FPlatformTime::Seconds(), true);
	return MakeSuccess();
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsEditorMockBackend::InjectError(
	EOpenMobileSensorType Sensor,
	EOpenMobileSensorFailureReason FailureReason,
	const FString& NativeCode
)
{
	if (!bActive
		|| !IsSupportedSensor(Sensor)
		|| FailureReason == EOpenMobileSensorFailureReason::None)
	{
		return MakeFailure(
			EOpenMobileSensorFailureReason::InvalidRequest,
			TEXT("InvalidMockError")
		);
	}
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	TArray<FOpenMobileSensorBackendStreamHandle> FailedHandles;
	for (const TPair<FOpenMobileSensorBackendStreamHandle, FActiveStream>& Pair
		: ActiveStreams)
	{
		if (Pair.Value.Request.Sensor.Type == Sensor)
		{
			FailedHandles.Add(Pair.Key);
		}
	}
	if (FailedHandles.IsEmpty())
	{
		return MakeFailure(
			EOpenMobileSensorFailureReason::InvalidHandle,
			TEXT("NoActiveMockStream")
		);
	}
	FOpenMobileSensorOperationResult Failure =
		FOpenMobileSensorsErrorMapper::Map(
			FailureReason,
			TEXT("OpenMobileEditorMock"),
			NativeCode
		);
	for (const FOpenMobileSensorBackendStreamHandle& Handle : FailedHandles)
	{
		FOpenMobileSensorsSubscriptionService::FailPhysicalStreamFromBackend(
			Token,
			Handle,
			Failure
		);
	}
	return MakeSuccess();
}

bool FOpenMobileSensorsEditorMockBackend::IsActive() const
{
	return bActive;
}

bool FOpenMobileSensorsEditorMockBackend::ValidateInput(
	const FOpenMobileSensorsMockInput& Input
)
{
	using namespace OpenMobileSensorsEditorMockBackendPrivate;
	return IsFiniteVector(Input.AccelerationMetresPerSecondSquared)
		&& IsFiniteVector(Input.AngularVelocityRadiansPerSecond)
		&& IsFiniteRotator(Input.RotationDegrees)
		&& FMath::IsFinite(Input.HeadingDegrees)
		&& Input.HeadingDegrees >= 0.0
		&& Input.HeadingDegrees <= 360.0
		&& Input.StepCount >= 0
		&& StaticEnum<EOpenMobileMotionActivity>()->IsValidEnumValue(
			static_cast<int64>(Input.Activity))
		&& StaticEnum<EOpenMobileActivityConfidence>()->IsValidEnumValue(
			static_cast<int64>(Input.ActivityConfidence))
		&& FMath::IsFinite(Input.PressureHectopascals)
		&& Input.PressureHectopascals > 0.0
		&& FMath::IsFinite(Input.AmbientLightLux)
		&& Input.AmbientLightLux >= 0.0
		&& FMath::IsFinite(Input.ProximityDistanceMeters)
		&& Input.ProximityDistanceMeters >= 0.0
		&& StaticEnum<EOpenMobilePermissionStatus>()->IsValidEnumValue(
			static_cast<int64>(Input.MotionActivityPermission))
		&& StaticEnum<EOpenMobilePermissionStatus>()->IsValidEnumValue(
			static_cast<int64>(Input.ActivityRecognitionPermission))
		&& StaticEnum<EOpenMobilePermissionStatus>()->IsValidEnumValue(
			static_cast<int64>(Input.TrueHeadingLocationPermission))
		&& StaticEnum<EOpenMobileSensorAccuracy>()->IsValidEnumValue(
			static_cast<int64>(Input.Accuracy))
		&& StaticEnum<EOpenMobileSensorFusionQuality>()->IsValidEnumValue(
			static_cast<int64>(Input.FusionQuality))
		&& FMath::IsFinite(Input.EstimatedError)
		&& Input.EstimatedError >= 0.0;
}

bool FOpenMobileSensorsEditorMockBackend::ValidateTimeline(
	const FOpenMobileSensorsMockTimeline& Timeline
)
{
	using namespace OpenMobileSensorsEditorMockBackendPrivate;
	if (Timeline.Frames.IsEmpty()
		|| Timeline.Frames.Num() > MaximumTimelineFrames
		|| !FMath::IsFinite(Timeline.PlaybackSpeed)
		|| Timeline.PlaybackSpeed < 0.01
		|| Timeline.PlaybackSpeed > 100.0
		|| Timeline.Frames[0].TimeSeconds != 0.0)
	{
		return false;
	}
	double PreviousTimeSeconds = -1.0;
	for (const FOpenMobileSensorsMockTimelineFrame& Frame : Timeline.Frames)
	{
		if (!FMath::IsFinite(Frame.TimeSeconds)
			|| Frame.TimeSeconds <= PreviousTimeSeconds
			|| Frame.TimeSeconds > MaximumTimelineSeconds
			|| !ValidateInput(Frame.Input))
		{
			return false;
		}
		PreviousTimeSeconds = Frame.TimeSeconds;
	}
	return !Timeline.bLoop || PreviousTimeSeconds > 0.0;
}

FOpenMobileSensorsMockInput FOpenMobileSensorsEditorMockBackend::MakePreset(
	EOpenMobileSensorsMockPreset Preset
)
{
	FOpenMobileSensorsMockInput Input;
	switch (Preset)
	{
	case EOpenMobileSensorsMockPreset::Walking:
		Input.AccelerationMetresPerSecondSquared = FVector(0.4, 0.1, 9.9);
		Input.StepCount = 120;
		Input.Activity = EOpenMobileMotionActivity::Walking;
		break;
	case EOpenMobileSensorsMockPreset::Running:
		Input.AccelerationMetresPerSecondSquared = FVector(1.2, 0.4, 10.3);
		Input.StepCount = 240;
		Input.Activity = EOpenMobileMotionActivity::Running;
		break;
	case EOpenMobileSensorsMockPreset::Driving:
		Input.AccelerationMetresPerSecondSquared = FVector(0.2, 0.0, 9.80665);
		Input.HeadingDegrees = 90.0;
		Input.Activity = EOpenMobileMotionActivity::Automotive;
		break;
	case EOpenMobileSensorsMockPreset::PoorQuality:
		Input.bValuesValid = false;
		Input.Accuracy = EOpenMobileSensorAccuracy::Unreliable;
		Input.FusionQuality = EOpenMobileSensorFusionQuality::Degraded;
		Input.bCalibrationRequired = true;
		Input.EstimatedError = 25.0;
		break;
	case EOpenMobileSensorsMockPreset::PermissionDenied:
		Input.MotionActivityPermission = EOpenMobilePermissionStatus::Denied;
		Input.ActivityRecognitionPermission =
			EOpenMobilePermissionStatus::Denied;
		Input.TrueHeadingLocationPermission =
			EOpenMobilePermissionStatus::Denied;
		break;
	case EOpenMobileSensorsMockPreset::Custom:
	case EOpenMobileSensorsMockPreset::Stationary:
	default:
		break;
	}
	return Input;
}

bool FOpenMobileSensorsEditorMockBackend::IsSupportedSensor(
	EOpenMobileSensorType Sensor
)
{
	switch (Sensor)
	{
	case EOpenMobileSensorType::Accelerometer:
	case EOpenMobileSensorType::Gyroscope:
	case EOpenMobileSensorType::Attitude:
	case EOpenMobileSensorType::MagneticHeading:
	case EOpenMobileSensorType::TrueHeading:
	case EOpenMobileSensorType::StepCounter:
	case EOpenMobileSensorType::StepDetector:
	case EOpenMobileSensorType::Pedometer:
	case EOpenMobileSensorType::MotionActivity:
	case EOpenMobileSensorType::BarometricPressure:
	case EOpenMobileSensorType::AmbientLight:
	case EOpenMobileSensorType::Proximity:
		return true;
	default:
		return false;
	}
}

FOpenMobileSensorCapability
FOpenMobileSensorsEditorMockBackend::MakeCapability(
	EOpenMobileSensorType Sensor
)
{
	using namespace OpenMobileSensorsEditorMockBackendPrivate;
	FOpenMobileSensorCapability Capability;
	Capability.Sensor.Type = Sensor;
	Capability.Sensor.InstanceId = TEXT("Default");
	Capability.Availability.Name = FOpenMobileSensorTypes::GetStableName(Sensor);
	Capability.Availability.State = EOpenMobileCapabilityState::Available;
	Capability.Availability.Detail = TEXT("Controlled by OpenMobile Sensors Mocks.");
	Capability.Source = EOpenMobileSensorAvailabilitySource::Mock;
	Capability.MinimumFrequencyHz = MinimumFrequencyHz;
	Capability.MaximumFrequencyHz = MaximumFrequencyHz;
	Capability.BackgroundSupport =
		EOpenMobileSensorBackgroundSupport::Unsupported;
	if (Sensor == EOpenMobileSensorType::MotionActivity)
	{
		Capability.RequiredPermission =
			FOpenMobileSensorPermissions::GetPermissionName(
				EOpenMobileSensorPermission::MotionActivity);
	}
	if (Sensor == EOpenMobileSensorType::StepCounter
		|| Sensor == EOpenMobileSensorType::StepDetector
		|| Sensor == EOpenMobileSensorType::Pedometer)
	{
		Capability.RequiredPermission =
			FOpenMobileSensorPermissions::GetPermissionName(
				EOpenMobileSensorPermission::ActivityRecognition);
	}
	if (Sensor == EOpenMobileSensorType::TrueHeading)
	{
		Capability.RequiredPermission =
			FOpenMobileSensorPermissions::GetPermissionName(
				EOpenMobileSensorPermission::TrueHeadingLocation);
	}
	if (Sensor == EOpenMobileSensorType::Attitude)
	{
		for (const EOpenMobileAttitudeReferenceFrame Reference : {
			EOpenMobileAttitudeReferenceFrame::GameRelative,
			EOpenMobileAttitudeReferenceFrame::ArbitraryVertical,
			EOpenMobileAttitudeReferenceFrame::MagneticNorth,
			EOpenMobileAttitudeReferenceFrame::TrueNorth
		})
		{
			FOpenMobileAttitudeReferenceFrameCapability& ReferenceCapability =
				Capability.AttitudeReferenceFrames.AddDefaulted_GetRef();
			ReferenceCapability.ReferenceFrame = Reference;
			ReferenceCapability.Availability.Name =
				FOpenMobileSensorTypes::GetStableName(Sensor);
			ReferenceCapability.Availability.State =
				EOpenMobileCapabilityState::Available;
			ReferenceCapability.bHeadingDependent =
				Reference == EOpenMobileAttitudeReferenceFrame::MagneticNorth
				|| Reference == EOpenMobileAttitudeReferenceFrame::TrueNorth;
			ReferenceCapability.bLocationDependent =
				Reference == EOpenMobileAttitudeReferenceFrame::TrueNorth;
		}
	}
	return Capability;
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsEditorMockBackend::MakeSuccess()
{
	FOpenMobileSensorOperationResult Result;
	Result.Code = EOpenMobileSensorResultCode::Success;
	return Result;
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsEditorMockBackend::MakeFailure(
	EOpenMobileSensorFailureReason Reason,
	const TCHAR* NativeCode
)
{
	return FOpenMobileSensorsErrorMapper::Map(
		Reason,
		TEXT("OpenMobileEditorMock"),
		NativeCode
	);
}

FOpenMobileSensorSampleHeader
FOpenMobileSensorsEditorMockBackend::MakeHeader(
	const FOpenMobileSensorIdentifier& Sensor,
	const FOpenMobileSensorsMockInput& Input,
	double TimestampSeconds,
	int32 SourceFlags
)
{
	FOpenMobileSensorSampleHeader Header;
	Header.Sensor = Sensor;
	Header.TimestampSeconds = TimestampSeconds;
	Header.bUnitsNormalized = true;
	Header.bCoordinatesNormalized = true;
	Header.bValid = Input.bValuesValid;
	Header.Accuracy = Input.Accuracy;
	Header.bCalibrationRequired = Input.bCalibrationRequired;
	Header.SourceFlags = SourceFlags;
	Header.Fusion.Quality = Input.FusionQuality;
	Header.Fusion.bHasNativeQualityReport = true;
	Header.Fusion.NativeQuality = Input.FusionQuality;
	Header.bHasEstimatedError = true;
	Header.EstimatedError = Input.EstimatedError;
	return Header;
}

EOpenMobilePermissionStatus
FOpenMobileSensorsEditorMockBackend::GetPermissionStatus(
	FName Permission
) const
{
	if (Permission == FOpenMobileSensorPermissions::GetPermissionName(
		EOpenMobileSensorPermission::MotionActivity))
	{
		return CurrentInput.MotionActivityPermission;
	}
	if (Permission == FOpenMobileSensorPermissions::GetPermissionName(
		EOpenMobileSensorPermission::ActivityRecognition))
	{
		return CurrentInput.ActivityRecognitionPermission;
	}
	if (Permission == FOpenMobileSensorPermissions::GetPermissionName(
		EOpenMobileSensorPermission::TrueHeadingLocation))
	{
		return CurrentInput.TrueHeadingLocationPermission;
	}
	return EOpenMobilePermissionStatus::NotDetermined;
}

void FOpenMobileSensorsEditorMockBackend::EnsureTicker()
{
	const bool bNeedsRealtimeTimeline = bTimelinePlaying
		&& !ActiveTimeline.bUseManualClock;
	if (bActive
		&& (!ActiveStreams.IsEmpty() || bNeedsRealtimeTimeline)
		&& !TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(
				this,
				&FOpenMobileSensorsEditorMockBackend::Tick
			),
			0.0f
		);
	}
}

void FOpenMobileSensorsEditorMockBackend::StopTicker()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
}

bool FOpenMobileSensorsEditorMockBackend::Tick(float DeltaSeconds)
{
	static_cast<void>(DeltaSeconds);
	const bool bNeedsRealtimeTimeline = bTimelinePlaying
		&& !ActiveTimeline.bUseManualClock;
	if (!bActive
		|| (ActiveStreams.IsEmpty() && !bNeedsRealtimeTimeline))
	{
		TickHandle.Reset();
		return false;
	}
	const double NowSeconds = FPlatformTime::Seconds();
	ResolveTimeline(NowSeconds);
	PublishAll(NowSeconds, false);
	const bool bKeepTicking = !ActiveStreams.IsEmpty()
		|| (bTimelinePlaying && !ActiveTimeline.bUseManualClock);
	if (!bKeepTicking)
	{
		TickHandle.Reset();
	}
	return bKeepTicking;
}

void FOpenMobileSensorsEditorMockBackend::ApplyPermissionChanges(
	const FOpenMobileSensorsMockInput& Previous,
	const FOpenMobileSensorsMockInput& Next
)
{
	using namespace OpenMobileSensorsEditorMockBackendPrivate;
	if (!IsPermissionLoss(Previous.MotionActivityPermission)
		&& IsPermissionLoss(Next.MotionActivityPermission))
	{
		FOpenMobileSensorsSubscriptionService::
			InvalidateForUnrecoverablePermissionLoss(
				EOpenMobileSensorType::MotionActivity);
	}
	if (!IsPermissionLoss(Previous.ActivityRecognitionPermission)
		&& IsPermissionLoss(Next.ActivityRecognitionPermission))
	{
		for (const EOpenMobileSensorType Sensor : {
			EOpenMobileSensorType::StepCounter,
			EOpenMobileSensorType::StepDetector,
			EOpenMobileSensorType::Pedometer,
			EOpenMobileSensorType::ActivityTransition
		})
		{
			FOpenMobileSensorsSubscriptionService::
				InvalidateForUnrecoverablePermissionLoss(Sensor);
		}
	}
	if (!IsPermissionLoss(Previous.TrueHeadingLocationPermission)
		&& IsPermissionLoss(Next.TrueHeadingLocationPermission))
	{
		FOpenMobileSensorsSubscriptionService::
			InvalidateForUnrecoverablePermissionLoss(
				EOpenMobileSensorType::TrueHeading);
	}
}

void FOpenMobileSensorsEditorMockBackend::ResolveTimeline(double NowSeconds)
{
	if (!bTimelinePlaying
		|| ActiveTimeline.Frames.IsEmpty()
		|| ActiveTimeline.bUseManualClock)
	{
		return;
	}
	ResolveTimelinePosition(FMath::Max(
		0.0,
		(NowSeconds - TimelineStartSeconds) * ActiveTimeline.PlaybackSpeed
	));
}

void FOpenMobileSensorsEditorMockBackend::ResolveTimelinePosition(
	double PlaybackSeconds
)
{
	const double DurationSeconds = ActiveTimeline.Frames.Last().TimeSeconds;
	if (ActiveTimeline.bLoop)
	{
		PlaybackSeconds = FMath::Fmod(PlaybackSeconds, DurationSeconds);
	}
	else if (PlaybackSeconds >= DurationSeconds)
	{
		PlaybackSeconds = DurationSeconds;
		bTimelinePlaying = false;
	}
	const int32 FrameIndex = FMath::Max(
		0,
		Algo::UpperBoundBy(
			ActiveTimeline.Frames,
			PlaybackSeconds,
			[](const FOpenMobileSensorsMockTimelineFrame& Frame)
			{
				return Frame.TimeSeconds;
			}
		) - 1
	);
	const FOpenMobileSensorsMockInput Previous = CurrentInput;
	CurrentInput = ActiveTimeline.Frames[FrameIndex].Input;
	ApplyPermissionChanges(Previous, CurrentInput);
}

void FOpenMobileSensorsEditorMockBackend::PublishAll(
	double NowSeconds,
	bool bForce
)
{
	for (TPair<FOpenMobileSensorBackendStreamHandle, FActiveStream>& Pair
		: ActiveStreams)
	{
		PublishStream(Pair.Key, Pair.Value, NowSeconds, bForce);
	}
}

void FOpenMobileSensorsEditorMockBackend::PublishStream(
	const FOpenMobileSensorBackendStreamHandle& Handle,
	FActiveStream& Stream,
	double TimestampSeconds,
	bool bForce
)
{
	using namespace OpenMobileSensorsEditorMockBackendPrivate;
	if (!bForce && TimestampSeconds < Stream.NextEmissionSeconds)
	{
		return;
	}
	const double FrequencyHz = FMath::Clamp(
		Stream.Request.RequestedFrequencyHz,
		MinimumFrequencyHz,
		MaximumFrequencyHz
	);
	Stream.NextEmissionSeconds = TimestampSeconds + 1.0 / FrequencyHz;
	const FOpenMobileSensorsBackendToken Token =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	const FOpenMobileSensorIdentifier& Sensor = Stream.Request.Sensor;
	FOpenMobileSensorAccuracySnapshot Accuracy;
	Accuracy.Sensor = Sensor;
	Accuracy.Accuracy = CurrentInput.Accuracy;
	Accuracy.bCalibrationRequired = CurrentInput.bCalibrationRequired;
	Accuracy.bHasEstimatedError = true;
	Accuracy.EstimatedError = CurrentInput.EstimatedError;
	Accuracy.TimestampSeconds = TimestampSeconds;
	FOpenMobileSensorsSampleService::PublishAccuracyFromBackend(
		Token,
		Handle,
		Accuracy
	);

	switch (Sensor.Type)
	{
	case EOpenMobileSensorType::Accelerometer:
	case EOpenMobileSensorType::Gyroscope:
	{
		FOpenMobileVectorSensorBatch Batch;
		FOpenMobileVectorSensorSample& Sample =
			Batch.Samples.AddDefaulted_GetRef();
		Sample.Header = MakeHeader(
			Sensor,
			CurrentInput,
			TimestampSeconds,
			MockFlags(Sensor.Type)
		);
		Sample.Value = Sensor.Type == EOpenMobileSensorType::Accelerometer
			? CurrentInput.AccelerationMetresPerSecondSquared
			: CurrentInput.AngularVelocityRadiansPerSecond;
		FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
			Token,
			Handle,
			Batch
		);
		break;
	}
	case EOpenMobileSensorType::Attitude:
	{
		FOpenMobileAttitudeSensorBatch Batch;
		FOpenMobileAttitudeSensorSample& Sample =
			Batch.Samples.AddDefaulted_GetRef();
		Sample.Header = MakeHeader(
			Sensor,
			CurrentInput,
			TimestampSeconds,
			MockFlags(Sensor.Type)
		);
		Sample.Quaternion = CurrentInput.RotationDegrees.Quaternion();
		Sample.ReferenceFrame = Stream.Request.AttitudeReferenceFrame;
		Sample.FusionQuality = CurrentInput.FusionQuality;
		FOpenMobileSensorsSampleService::PublishAttitudeBatchFromBackend(
			Token,
			Handle,
			Batch
		);
		break;
	}
	case EOpenMobileSensorType::MagneticHeading:
	case EOpenMobileSensorType::TrueHeading:
	{
		FOpenMobileHeadingSensorBatch Batch;
		FOpenMobileHeadingSensorSample& Sample =
			Batch.Samples.AddDefaulted_GetRef();
		Sample.Header = MakeHeader(
			Sensor,
			CurrentInput,
			TimestampSeconds,
			MockFlags(Sensor.Type)
		);
		Sample.HeadingDegrees = FMath::Fmod(
			CurrentInput.HeadingDegrees,
			360.0
		);
		Sample.Reference = Sensor.Type == EOpenMobileSensorType::TrueHeading
			? EOpenMobileHeadingReference::TrueNorth
			: EOpenMobileHeadingReference::MagneticNorth;
		Sample.bTiltCompensated = true;
		Sample.bCalibrationRequired = CurrentInput.bCalibrationRequired;
		FOpenMobileSensorsSampleService::PublishHeadingBatchFromBackend(
			Token,
			Handle,
			Batch
		);
		break;
	}
	case EOpenMobileSensorType::StepCounter:
	case EOpenMobileSensorType::StepDetector:
	case EOpenMobileSensorType::Pedometer:
	{
		const int64 StepDelta = FMath::Max(
			0ll,
			CurrentInput.StepCount - Stream.LastStepCount
		);
		if (Sensor.Type == EOpenMobileSensorType::StepDetector
			&& StepDelta == 0)
		{
			return;
		}
		FOpenMobileStepsSensorBatch Batch;
		FOpenMobileStepsSensorSample& Sample =
			Batch.Samples.AddDefaulted_GetRef();
		Sample.Header = MakeHeader(
			Sensor,
			CurrentInput,
			TimestampSeconds,
			MockFlags(Sensor.Type)
		);
		Sample.Count = CurrentInput.StepCount;
		Sample.Origin = EOpenMobileStepCountOrigin::DeviceBoot;
		Sample.DetectedStepDelta = StepDelta;
		Sample.bHasNativeTotal = true;
		Sample.NativeTotal = CurrentInput.StepCount;
		Stream.LastStepCount = CurrentInput.StepCount;
		FOpenMobileSensorsSampleService::PublishStepsBatchFromBackend(
			Token,
			Handle,
			Batch
		);
		break;
	}
	case EOpenMobileSensorType::MotionActivity:
	{
		FOpenMobileActivitySensorBatch Batch;
		FOpenMobileActivitySensorSample& Sample =
			Batch.Samples.AddDefaulted_GetRef();
		Sample.Header = MakeHeader(
			Sensor,
			CurrentInput,
			TimestampSeconds,
			MockFlags(Sensor.Type)
		);
		Sample.Activity = CurrentInput.Activity;
		Sample.Confidence = CurrentInput.ActivityConfidence;
		Sample.ActivityProvider = GetProviderName();
		FOpenMobileSensorsSampleService::PublishActivityBatchFromBackend(
			Token,
			Handle,
			Batch
		);
		break;
	}
	case EOpenMobileSensorType::BarometricPressure:
	case EOpenMobileSensorType::AmbientLight:
	{
		FOpenMobileScalarSensorBatch Batch;
		FOpenMobileScalarSensorSample& Sample =
			Batch.Samples.AddDefaulted_GetRef();
		Sample.Header = MakeHeader(
			Sensor,
			CurrentInput,
			TimestampSeconds,
			MockFlags(Sensor.Type)
		);
		Sample.Value = Sensor.Type == EOpenMobileSensorType::BarometricPressure
			? CurrentInput.PressureHectopascals
			: CurrentInput.AmbientLightLux;
		FOpenMobileSensorsSampleService::PublishScalarBatchFromBackend(
			Token,
			Handle,
			Batch
		);
		break;
	}
	case EOpenMobileSensorType::Proximity:
	{
		FOpenMobileProximitySensorBatch Batch;
		FOpenMobileProximitySensorSample& Sample =
			Batch.Samples.AddDefaulted_GetRef();
		Sample.Header = MakeHeader(
			Sensor,
			CurrentInput,
			TimestampSeconds,
			MockFlags(Sensor.Type)
		);
		Sample.bNear = CurrentInput.bProximityNear;
		Sample.bHasDistanceMeters = true;
		Sample.DistanceMeters = CurrentInput.ProximityDistanceMeters;
		Sample.bHasMaximumRangeMeters = true;
		Sample.MaximumRangeMeters = FMath::Max(
			0.1,
			CurrentInput.ProximityDistanceMeters
		);
		FOpenMobileSensorsSampleService::PublishProximityBatchFromBackend(
			Token,
			Handle,
			Batch
		);
		break;
	}
	default:
		break;
	}
}
