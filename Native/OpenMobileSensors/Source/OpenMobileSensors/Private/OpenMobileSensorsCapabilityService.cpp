#include "OpenMobileSensorsCapabilityService.h"

#include "IOpenMobileSensorsBackend.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorsBackendRegistry.h"

namespace OpenMobileSensorsCapabilityServicePrivate
{
	bool bStarted = false;
	bool bApplicationActive = true;
	bool bLocationInputAvailable = false;
	bool bBackendDirty = true;
	bool bSnapshotDirty = true;
	bool bHasSnapshot = false;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;
	FOpenMobileSensorsBackendToken BaseBackendToken;
	FName BaseBackendName;
	FOpenMobileCapability BaseBackendAvailability;
	TArray<FOpenMobileSensorCapability> BaseCapabilities;
	TMap<FName, EOpenMobilePermissionStatus> PermissionStatuses;
	FOpenMobileSensorCapabilitySnapshot CachedSnapshot;
	FOnOpenMobileSensorCapabilityMatrixChanged ChangedEvent;

	FName GetDefaultPermission(EOpenMobileSensorType Type)
	{
		switch (Type)
		{
		case EOpenMobileSensorType::StepCounter:
		case EOpenMobileSensorType::StepDetector:
		case EOpenMobileSensorType::ActivityTransition:
			return FOpenMobileSensorPermissions::GetPermissionName(
				EOpenMobileSensorPermission::ActivityRecognition
			);
		case EOpenMobileSensorType::Pedometer:
		case EOpenMobileSensorType::MotionActivity:
			return FOpenMobileSensorPermissions::GetPermissionName(
				EOpenMobileSensorPermission::MotionActivity
			);
		case EOpenMobileSensorType::TrueHeading:
			return FOpenMobileSensorPermissions::GetPermissionName(
				EOpenMobileSensorPermission::TrueHeadingLocation
			);
		default:
			return NAME_None;
		}
	}

	void RefreshBackendBase()
	{
		if (!bBackendDirty)
		{
			return;
		}
		bBackendDirty = false;
		BaseBackendToken = {};
		BaseBackendName = NAME_None;
		BaseBackendAvailability = {};
		BaseCapabilities.Reset();
		IOpenMobileSensorsBackend* Backend =
			FOpenMobileSensorsBackendRegistry::FindBackend();
		if (!Backend)
		{
			BaseBackendAvailability.Name =
				IOpenMobileSensorsBackend::GetModularFeatureName();
			BaseBackendAvailability.State =
				EOpenMobileCapabilityState::NotSupported;
			BaseBackendAvailability.Detail =
				TEXT("No OpenMobile Sensors backend is registered.");
			return;
		}
		BaseBackendToken = FOpenMobileSensorsBackendRegistry::CaptureToken();
		BaseBackendName = Backend->GetBackendName();
		BaseBackendAvailability = Backend->GetBackendCapability();
		BaseCapabilities = Backend->GetSensorCapabilities();
	}

	void ApplyPermissionState(FOpenMobileSensorCapability& Capability)
	{
		if (Capability.RequiredPermission.IsNone())
		{
			return;
		}
		if (Capability.ActiveRestriction ==
			EOpenMobileSensorRestriction::MissingHardware
			|| Capability.Availability.State ==
				EOpenMobileCapabilityState::NotSupported)
		{
			return;
		}
		const EOpenMobilePermissionStatus* Status =
			PermissionStatuses.Find(Capability.RequiredPermission);
		if (!Status)
		{
			return;
		}
		switch (*Status)
		{
		case EOpenMobilePermissionStatus::Granted:
			if (Capability.ActiveRestriction ==
				EOpenMobileSensorRestriction::Permission)
			{
				Capability.ActiveRestriction =
					EOpenMobileSensorRestriction::None;
				Capability.Availability.State =
					EOpenMobileCapabilityState::Available;
				Capability.Availability.Detail.Reset();
			}
			break;
		case EOpenMobilePermissionStatus::NotDetermined:
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::Permission;
			Capability.Availability.State =
				EOpenMobileCapabilityState::PermissionRequired;
			Capability.Availability.Detail =
				TEXT("The required sensor permission has not been decided.");
			break;
		case EOpenMobilePermissionStatus::Denied:
		case EOpenMobilePermissionStatus::PermanentlyDenied:
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::Permission;
			Capability.Availability.State =
				EOpenMobileCapabilityState::Denied;
			Capability.Availability.Detail =
				TEXT("The required sensor permission was denied.");
			break;
		case EOpenMobilePermissionStatus::Restricted:
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::Permission;
			Capability.Availability.State =
				EOpenMobileCapabilityState::Restricted;
			Capability.Availability.Detail =
				TEXT("System policy restricts the required sensor permission.");
			break;
		}
	}

	void ApplyLocationState(FOpenMobileSensorCapability& Capability)
	{
		if (Capability.Sensor.Type != EOpenMobileSensorType::TrueHeading)
		{
			return;
		}
		if (Capability.ActiveRestriction ==
			EOpenMobileSensorRestriction::MissingHardware
			|| Capability.Availability.State ==
				EOpenMobileCapabilityState::NotSupported)
		{
			return;
		}
		if (!bLocationInputAvailable)
		{
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::MissingInput;
			Capability.Availability.State =
				EOpenMobileCapabilityState::TemporarilyUnavailable;
			Capability.Availability.Detail =
				TEXT("True heading requires recent caller-owned location input.");
			return;
		}
		if (Capability.ActiveRestriction ==
			EOpenMobileSensorRestriction::MissingInput)
		{
			Capability.ActiveRestriction = EOpenMobileSensorRestriction::None;
			Capability.Availability.State =
				EOpenMobileCapabilityState::Available;
			Capability.Availability.Detail.Reset();
		}
	}

	void ApplyLifecycleState(FOpenMobileSensorCapability& Capability)
	{
		if (bApplicationActive
			|| Capability.Availability.State !=
				EOpenMobileCapabilityState::Available)
		{
			return;
		}
		switch (Capability.BackgroundSupport)
		{
		case EOpenMobileSensorBackgroundSupport::Supported:
		case EOpenMobileSensorBackgroundSupport::EventDriven:
			break;
		case EOpenMobileSensorBackgroundSupport::Limited:
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::Background;
			Capability.Availability.Detail =
				TEXT("Background sensor delivery is limited.");
			break;
		case EOpenMobileSensorBackgroundSupport::Unknown:
		case EOpenMobileSensorBackgroundSupport::Unsupported:
		case EOpenMobileSensorBackgroundSupport::Suspended:
		default:
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::Background;
			Capability.Availability.State =
				EOpenMobileCapabilityState::TemporarilyUnavailable;
			Capability.Availability.Detail =
				TEXT("The sensor is suspended while the application is inactive.");
			break;
		}
	}

	FOpenMobileSensorCapability MakeMissingCapability(
		EOpenMobileSensorType Type,
		bool bHasBackend
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(Type);
		Capability.RequiredPermission = GetDefaultPermission(Type);
		Capability.BackgroundSupport =
			EOpenMobileSensorBackgroundSupport::Unsupported;
		if (bHasBackend)
		{
			Capability.Availability.State =
				EOpenMobileCapabilityState::Unavailable;
			Capability.Availability.Detail =
				TEXT("The active backend reported no matching sensor hardware.");
			Capability.ActiveRestriction =
				EOpenMobileSensorRestriction::MissingHardware;
		}
		else
		{
			Capability.Availability.State =
				EOpenMobileCapabilityState::NotSupported;
			Capability.Availability.Detail =
				TEXT("No Sensors backend is registered on this host.");
		}
		return Capability;
	}

	void ApplyAccelerometerFallback(
		FOpenMobileSensorCapabilitySnapshot& Snapshot,
		EOpenMobileSensorType DerivedType,
		const TCHAR* Detail
	)
	{
		FOpenMobileSensorCapability* Derived =
			Snapshot.Sensors.FindByPredicate(
				[DerivedType](const FOpenMobileSensorCapability& Capability)
				{
					return Capability.Sensor.Type == DerivedType;
				}
			);
		const FOpenMobileSensorCapability* Accelerometer =
			Snapshot.Sensors.FindByPredicate(
				[](const FOpenMobileSensorCapability& Capability)
				{
					return Capability.Sensor.Type ==
						EOpenMobileSensorType::Accelerometer;
				}
			);
		constexpr double MinimumFallbackFrequencyHz = 15.0;
		if (!Derived)
		{
			return;
		}
		const bool bLinearAcceleration = DerivedType ==
			EOpenMobileSensorType::LinearAcceleration;
		Derived->Fallback.bImplemented = true;
		Derived->Fallback.RequiredInputs = {
			EOpenMobileSensorType::Accelerometer
		};
		Derived->Fallback.MinimumInputFrequencyHz =
			MinimumFallbackFrequencyHz;
		Derived->Fallback.bRequiresCalibratedInput =
			bLinearAcceleration;
		Derived->Fallback.ExpectedQuality = bLinearAcceleration
			? EOpenMobileSensorFusionQuality::Degraded
			: EOpenMobileSensorFusionQuality::Nominal;
		Derived->Fallback.PowerCost =
			EOpenMobileSensorFallbackPowerCost::Low;
		Derived->Fallback.CpuBudgetMicrosecondsPerSample =
			bLinearAcceleration ? 60.0 : 40.0;
		Derived->Fallback.UnsupportedConditionFlags =
			static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::MissingInput
			)
			| static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::
					InsufficientRate
			)
			| static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::
					PermissionUnavailable
			)
			| static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::
					LifecycleUnavailable
			);
		if (bLinearAcceleration)
		{
			Derived->Fallback.UnsupportedConditionFlags |=
				static_cast<int32>(
					EOpenMobileSensorFallbackUnsupportedCondition::
						UncalibratedInput
				);
		}
		Derived->Fallback.bAvailable = Accelerometer
			&& Accelerometer->Availability.State ==
				EOpenMobileCapabilityState::Available
			&& (Accelerometer->MaximumFrequencyHz <= 0.0
				|| Accelerometer->MaximumFrequencyHz >=
					MinimumFallbackFrequencyHz);
		const bool bHasUsableDirectSource = Derived
			&& Derived->Availability.State ==
				EOpenMobileCapabilityState::Available
			&& (Derived->MaximumFrequencyHz <= 0.0
				|| Derived->MaximumFrequencyHz >=
					MinimumFallbackFrequencyHz);
		if (bHasUsableDirectSource
			|| !Accelerometer
			|| Accelerometer->Availability.State !=
				EOpenMobileCapabilityState::Available
			|| (Accelerometer->MaximumFrequencyHz > 0.0
				&& Accelerometer->MaximumFrequencyHz <
					MinimumFallbackFrequencyHz))
		{
			return;
		}
		Derived->Availability.State = EOpenMobileCapabilityState::Available;
		Derived->Availability.Detail = Detail;
		Derived->Source = EOpenMobileSensorAvailabilitySource::Derived;
		Derived->ActiveRestriction = EOpenMobileSensorRestriction::None;
		Derived->MinimumFrequencyHz = FMath::Max(
			MinimumFallbackFrequencyHz,
			Accelerometer->MinimumFrequencyHz
		);
		Derived->MaximumFrequencyHz = Accelerometer->MaximumFrequencyHz;
		Derived->bSupportsNativeBatching =
			Accelerometer->bSupportsNativeBatching;
		Derived->BackgroundSupport = Accelerometer->BackgroundSupport;
	}

	void ApplyTrueHeadingFallback(
		FOpenMobileSensorCapabilitySnapshot& Snapshot
	)
	{
		FOpenMobileSensorCapability* TrueHeading =
			Snapshot.Sensors.FindByPredicate(
				[](const FOpenMobileSensorCapability& Capability)
				{
					return Capability.Sensor.Type ==
						EOpenMobileSensorType::TrueHeading;
				}
			);
		const FOpenMobileSensorCapability* MagneticHeading =
			Snapshot.Sensors.FindByPredicate(
				[](const FOpenMobileSensorCapability& Capability)
				{
					return Capability.Sensor.Type ==
						EOpenMobileSensorType::MagneticHeading;
				}
			);
		if (!TrueHeading)
		{
			return;
		}
		TrueHeading->Fallback.bImplemented = true;
		TrueHeading->Fallback.RequiredInputs = {
			EOpenMobileSensorType::MagneticHeading
		};
		TrueHeading->Fallback.MinimumInputFrequencyHz = 1.0;
		TrueHeading->Fallback.bRequiresCalibratedInput = true;
		TrueHeading->Fallback.ExpectedQuality =
			EOpenMobileSensorFusionQuality::Nominal;
		TrueHeading->Fallback.PowerCost =
			EOpenMobileSensorFallbackPowerCost::Low;
		TrueHeading->Fallback.CpuBudgetMicrosecondsPerSample = 80.0;
		TrueHeading->Fallback.UnsupportedConditionFlags =
			static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::MissingInput
			)
			| static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::
					PermissionUnavailable
			)
			| static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::
					UncalibratedInput
			);
		const bool bMagneticAvailable = MagneticHeading
			&& MagneticHeading->Availability.State ==
				EOpenMobileCapabilityState::Available;
		TrueHeading->Fallback.bAvailable = bMagneticAvailable
			&& bLocationInputAvailable;
		const bool bDirectAvailable = TrueHeading->Availability.State ==
			EOpenMobileCapabilityState::Available
			&& TrueHeading->Source !=
				EOpenMobileSensorAvailabilitySource::Derived;
		if (bDirectAvailable || !bMagneticAvailable)
		{
			return;
		}
		TrueHeading->Availability.State =
			EOpenMobileCapabilityState::Available;
		TrueHeading->Availability.Detail =
			TEXT("True heading is derived from magnetic heading and caller-owned location input.");
		TrueHeading->Source = EOpenMobileSensorAvailabilitySource::Derived;
		TrueHeading->ActiveRestriction = EOpenMobileSensorRestriction::None;
		TrueHeading->MinimumFrequencyHz = MagneticHeading->MinimumFrequencyHz;
		TrueHeading->MaximumFrequencyHz = MagneticHeading->MaximumFrequencyHz;
		TrueHeading->bSupportsNativeBatching =
			MagneticHeading->bSupportsNativeBatching;
		TrueHeading->BackgroundSupport =
			MagneticHeading->BackgroundSupport;
	}

	void ApplyRelativeAltitudeFallback(
		FOpenMobileSensorCapabilitySnapshot& Snapshot
	)
	{
		FOpenMobileSensorCapability* RelativeAltitude =
			Snapshot.Sensors.FindByPredicate(
				[](const FOpenMobileSensorCapability& Capability)
				{
					return Capability.Sensor.Type ==
						EOpenMobileSensorType::RelativeAltitude;
				}
			);
		const FOpenMobileSensorCapability* Pressure =
			Snapshot.Sensors.FindByPredicate(
				[](const FOpenMobileSensorCapability& Capability)
				{
					return Capability.Sensor.Type ==
						EOpenMobileSensorType::BarometricPressure;
				}
			);
		if (!RelativeAltitude)
		{
			return;
		}
		RelativeAltitude->Fallback.bImplemented = true;
		RelativeAltitude->Fallback.RequiredInputs = {
			EOpenMobileSensorType::BarometricPressure
		};
		RelativeAltitude->Fallback.MinimumInputFrequencyHz = 1.0;
		RelativeAltitude->Fallback.ExpectedQuality =
			EOpenMobileSensorFusionQuality::Degraded;
		RelativeAltitude->Fallback.PowerCost =
			EOpenMobileSensorFallbackPowerCost::Low;
		RelativeAltitude->Fallback.CpuBudgetMicrosecondsPerSample = 40.0;
		RelativeAltitude->Fallback.UnsupportedConditionFlags =
			static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::MissingInput
			)
			| static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::
					LifecycleUnavailable
			);
		const bool bPressureAvailable = Pressure
			&& Pressure->Availability.State ==
				EOpenMobileCapabilityState::Available;
		RelativeAltitude->Fallback.bAvailable = bPressureAvailable;
		const bool bDirectAvailable =
			RelativeAltitude->Availability.State ==
				EOpenMobileCapabilityState::Available
			&& RelativeAltitude->Source !=
				EOpenMobileSensorAvailabilitySource::Derived;
		if (bDirectAvailable || !bPressureAvailable)
		{
			return;
		}
		RelativeAltitude->Availability.State =
			EOpenMobileCapabilityState::Available;
		RelativeAltitude->Availability.Detail =
			TEXT("Relative altitude is derived from a session pressure baseline using the standard atmosphere model.");
		RelativeAltitude->Source =
			EOpenMobileSensorAvailabilitySource::Derived;
		RelativeAltitude->ActiveRestriction =
			EOpenMobileSensorRestriction::None;
		RelativeAltitude->MinimumFrequencyHz = FMath::Max(
			1.0,
			Pressure->MinimumFrequencyHz
		);
		RelativeAltitude->MaximumFrequencyHz =
			Pressure->MaximumFrequencyHz;
		RelativeAltitude->bSupportsNativeBatching =
			Pressure->bSupportsNativeBatching;
		RelativeAltitude->BackgroundSupport = Pressure->BackgroundSupport;
	}

	void ApplyActivityTransitionFallback(
		FOpenMobileSensorCapabilitySnapshot& Snapshot
	)
	{
		FOpenMobileSensorCapability* Transition =
			Snapshot.Sensors.FindByPredicate(
				[](const FOpenMobileSensorCapability& Capability)
				{
					return Capability.Sensor.Type ==
						EOpenMobileSensorType::ActivityTransition;
				}
			);
		const FOpenMobileSensorCapability* Activity =
			Snapshot.Sensors.FindByPredicate(
				[](const FOpenMobileSensorCapability& Capability)
				{
					return Capability.Sensor.Type ==
						EOpenMobileSensorType::MotionActivity;
				}
			);
		if (!Transition)
		{
			return;
		}
		Transition->Fallback.bImplemented = true;
		Transition->Fallback.RequiredInputs = {
			EOpenMobileSensorType::MotionActivity
		};
		Transition->Fallback.ExpectedQuality =
			EOpenMobileSensorFusionQuality::Nominal;
		Transition->Fallback.PowerCost =
			EOpenMobileSensorFallbackPowerCost::Low;
		Transition->Fallback.CpuBudgetMicrosecondsPerSample = 20.0;
		Transition->Fallback.UnsupportedConditionFlags =
			static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::MissingInput
			)
			| static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::
					PermissionUnavailable
			)
			| static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::
					LifecycleUnavailable
			);
		const bool bActivityAvailable = Activity
			&& Activity->Availability.State ==
				EOpenMobileCapabilityState::Available;
		Transition->Fallback.bAvailable = bActivityAvailable;
		const bool bDirectAvailable = Transition->Availability.State ==
				EOpenMobileCapabilityState::Available
			&& Transition->Source !=
				EOpenMobileSensorAvailabilitySource::Derived;
		if (bDirectAvailable || !bActivityAvailable)
		{
			return;
		}
		Transition->Availability.State =
			EOpenMobileCapabilityState::Available;
		Transition->Availability.Detail =
			TEXT("Activity transitions are derived from debounced classification changes.");
		Transition->Source = EOpenMobileSensorAvailabilitySource::Derived;
		Transition->ActiveRestriction = EOpenMobileSensorRestriction::None;
		Transition->RequiredPermission = Activity->RequiredPermission;
		Transition->MinimumFrequencyHz = Activity->MinimumFrequencyHz;
		Transition->MaximumFrequencyHz = Activity->MaximumFrequencyHz;
		Transition->bSupportsNativeBatching = false;
		Transition->BackgroundSupport = Activity->BackgroundSupport;
	}

	void ApplyShakeFallback(FOpenMobileSensorCapabilitySnapshot& Snapshot)
	{
		FOpenMobileSensorCapability* Shake =
			Snapshot.Sensors.FindByPredicate(
				[](const FOpenMobileSensorCapability& Capability)
				{
					return Capability.Sensor.Type ==
						EOpenMobileSensorType::Shake;
				}
			);
		const FOpenMobileSensorCapability* LinearAcceleration =
			Snapshot.Sensors.FindByPredicate(
				[](const FOpenMobileSensorCapability& Capability)
				{
					return Capability.Sensor.Type ==
						EOpenMobileSensorType::LinearAcceleration;
				}
			);
		const FOpenMobileSensorCapability* Accelerometer =
			Snapshot.Sensors.FindByPredicate(
				[](const FOpenMobileSensorCapability& Capability)
				{
					return Capability.Sensor.Type ==
						EOpenMobileSensorType::Accelerometer;
				}
			);
		if (!Shake)
		{
			return;
		}
		constexpr double MinimumInputFrequencyHz = 15.0;
		auto IsAvailableAtRequiredRate =
			[](const FOpenMobileSensorCapability* Capability)
			{
				return Capability
					&& Capability->Availability.State ==
						EOpenMobileCapabilityState::Available
					&& (Capability->MaximumFrequencyHz <= 0.0
						|| Capability->MaximumFrequencyHz >=
							MinimumInputFrequencyHz);
			};
		const bool bNativeLinearAvailable =
			LinearAcceleration
			&& LinearAcceleration->Source !=
				EOpenMobileSensorAvailabilitySource::Derived
			&& IsAvailableAtRequiredRate(LinearAcceleration);
		const FOpenMobileSensorCapability* Input = bNativeLinearAvailable
			? LinearAcceleration
			: IsAvailableAtRequiredRate(Accelerometer)
				? Accelerometer
				: nullptr;
		if (!Input
			&& LinearAcceleration
			&& LinearAcceleration->Source !=
				EOpenMobileSensorAvailabilitySource::Derived
			&& LinearAcceleration->ActiveRestriction !=
				EOpenMobileSensorRestriction::MissingHardware)
		{
			Input = LinearAcceleration;
		}
		if (!Input && Accelerometer
			&& Accelerometer->ActiveRestriction !=
				EOpenMobileSensorRestriction::MissingHardware)
		{
			Input = Accelerometer;
		}
		Shake->Source = EOpenMobileSensorAvailabilitySource::Derived;
		Shake->Fallback.bImplemented = true;
		Shake->Fallback.bAvailable =
			IsAvailableAtRequiredRate(Input);
		Shake->Fallback.MinimumInputFrequencyHz = MinimumInputFrequencyHz;
		Shake->Fallback.PowerCost =
			EOpenMobileSensorFallbackPowerCost::Low;
		Shake->Fallback.UnsupportedConditionFlags =
			static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::MissingInput
			)
			| static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::InsufficientRate
			)
			| static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::PermissionUnavailable
			)
			| static_cast<int32>(
				EOpenMobileSensorFallbackUnsupportedCondition::LifecycleUnavailable
			);
		if (!Input)
		{
			Shake->Fallback.RequiredInputs = {
				EOpenMobileSensorType::LinearAcceleration,
				EOpenMobileSensorType::Accelerometer
			};
			Shake->Availability.Detail =
				TEXT("Shake detection requires linear acceleration or calibrated acceleration input.");
			return;
		}
		Shake->Fallback.RequiredInputs = {Input->Sensor.Type};
		const bool bUsesAccelerometer = Input->Sensor.Type ==
			EOpenMobileSensorType::Accelerometer;
		Shake->Fallback.bRequiresCalibratedInput = bUsesAccelerometer;
		Shake->Fallback.ExpectedQuality = bUsesAccelerometer
			? EOpenMobileSensorFusionQuality::Degraded
			: EOpenMobileSensorFusionQuality::Nominal;
		Shake->Fallback.CpuBudgetMicrosecondsPerSample =
			bUsesAccelerometer ? 80.0 : 25.0;
		if (bUsesAccelerometer)
		{
			Shake->Fallback.UnsupportedConditionFlags |=
				static_cast<int32>(
					EOpenMobileSensorFallbackUnsupportedCondition::UncalibratedInput
				);
		}
		Shake->Availability.State = Input->Availability.State;
		Shake->Availability.Detail =
			Shake->Fallback.bAvailable
			? TEXT("Shake events are derived from a shared motion stream using configurable impulse detection.")
			: Input->Availability.Detail;
		Shake->ActiveRestriction = Input->ActiveRestriction;
		if (Input->Availability.State == EOpenMobileCapabilityState::Available
			&& !Shake->Fallback.bAvailable)
		{
			Shake->Availability.State =
				EOpenMobileCapabilityState::TemporarilyUnavailable;
			Shake->Availability.Detail =
				TEXT("The available motion input cannot meet the minimum shake detection rate.");
			Shake->ActiveRestriction =
				EOpenMobileSensorRestriction::RateLimited;
		}
		Shake->RequiredPermission = Input->RequiredPermission;
		Shake->MinimumFrequencyHz = FMath::Max(
			MinimumInputFrequencyHz,
			Input->MinimumFrequencyHz
		);
		Shake->MaximumFrequencyHz = Input->MaximumFrequencyHz;
		Shake->bSupportsNativeBatching = false;
		Shake->BackgroundSupport = Input->BackgroundSupport;
	}

	FOpenMobileSensorCapabilitySnapshot BuildSnapshot()
	{
		RefreshBackendBase();
		FOpenMobileSensorCapabilitySnapshot Snapshot;
		Snapshot.BackendName = BaseBackendName;
		Snapshot.BackendAvailability = BaseBackendAvailability;
		Snapshot.BackendGeneration =
			static_cast<int64>(BaseBackendToken.Generation);
		const bool bHasBackend = BaseBackendToken.Generation != 0;
		for (EOpenMobileSensorType Type : FOpenMobileSensorTypes::GetAll())
		{
			const FOpenMobileSensorCapability* Reported =
				BaseCapabilities.FindByPredicate(
					[Type](const FOpenMobileSensorCapability& Candidate)
					{
						return Candidate.Sensor.Type == Type;
					}
				);
			FOpenMobileSensorCapability Capability = Reported
				? *Reported
				: MakeMissingCapability(Type, bHasBackend);
			Capability.Sensor.Type = Type;
			if (Capability.Sensor.InstanceId.IsNone())
			{
				Capability.Sensor.InstanceId = TEXT("Default");
			}
			if (Capability.Availability.Name.IsNone())
			{
				Capability.Availability.Name =
					FOpenMobileSensorTypes::GetStableName(Type);
			}
			if (Capability.RequiredPermission.IsNone())
			{
				Capability.RequiredPermission = GetDefaultPermission(Type);
			}
			ApplyLocationState(Capability);
			ApplyPermissionState(Capability);
			ApplyLifecycleState(Capability);
			Snapshot.Sensors.Add(MoveTemp(Capability));
		}
		ApplyAccelerometerFallback(
			Snapshot,
			EOpenMobileSensorType::Gravity,
			TEXT("Gravity is derived from the accelerometer with bounded low-pass filtering.")
		);
		ApplyAccelerometerFallback(
			Snapshot,
			EOpenMobileSensorType::LinearAcceleration,
			TEXT("Linear acceleration is derived from calibrated acceleration with bounded gravity filtering.")
		);
		ApplyAccelerometerFallback(
			Snapshot,
			EOpenMobileSensorType::PhysicalOrientation,
			TEXT("Physical orientation is derived from device-fixed acceleration with bounded gravity filtering, hysteresis, and debounce.")
		);
		ApplyTrueHeadingFallback(Snapshot);
		ApplyRelativeAltitudeFallback(Snapshot);
		ApplyActivityTransitionFallback(Snapshot);
		ApplyShakeFallback(Snapshot);
		if (FOpenMobileSensorCapability* TrueHeading =
			Snapshot.Sensors.FindByPredicate(
				[](const FOpenMobileSensorCapability& Capability)
				{
					return Capability.Sensor.Type ==
						EOpenMobileSensorType::TrueHeading;
				}
			))
		{
			ApplyLocationState(*TrueHeading);
			ApplyPermissionState(*TrueHeading);
			ApplyLifecycleState(*TrueHeading);
		}
		return Snapshot;
	}

	bool IsMateriallyEqual(
		const FOpenMobileSensorCapabilitySnapshot& Left,
		const FOpenMobileSensorCapabilitySnapshot& Right
	)
	{
		return Left == Right;
	}

	void RefreshAndBroadcast()
	{
		if (!bSnapshotDirty)
		{
			return;
		}
		bSnapshotDirty = false;
		FOpenMobileSensorCapabilitySnapshot Snapshot = BuildSnapshot();
		const bool bChanged = bHasSnapshot
			&& !IsMateriallyEqual(CachedSnapshot, Snapshot);
		CachedSnapshot = MoveTemp(Snapshot);
		bHasSnapshot = true;
		if (bChanged)
		{
			ChangedEvent.Broadcast(CachedSnapshot);
		}
	}

	void HandleApplicationWillEnterBackground()
	{
		FOpenMobileSensorsCapabilityService::SetApplicationActive(false);
	}

	void HandleApplicationHasEnteredForeground()
	{
		FOpenMobileSensorsCapabilityService::SetApplicationActive(true);
	}
}

void FOpenMobileSensorsCapabilityService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsCapabilityServicePrivate;
	if (bStarted)
	{
		return;
	}
	bStarted = true;
	bApplicationActive = true;
	bLocationInputAvailable = false;
	bBackendDirty = true;
	bSnapshotDirty = true;
	bHasSnapshot = false;
	PermissionStatuses.Reset();
	BackgroundHandle =
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddStatic(
			&HandleApplicationWillEnterBackground
		);
	ForegroundHandle =
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddStatic(
			&HandleApplicationHasEnteredForeground
		);
}

void FOpenMobileSensorsCapabilityService::BeginShutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsCapabilityServicePrivate;
	if (BackgroundHandle.IsValid())
	{
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(
			BackgroundHandle
		);
		BackgroundHandle.Reset();
	}
	if (ForegroundHandle.IsValid())
	{
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(
			ForegroundHandle
		);
		ForegroundHandle.Reset();
	}
	ChangedEvent.Clear();
	PermissionStatuses.Reset();
	BaseCapabilities.Reset();
	CachedSnapshot = {};
	bStarted = false;
	bHasSnapshot = false;
	bBackendDirty = true;
	bSnapshotDirty = true;
}

FOpenMobileSensorCapabilitySnapshot
FOpenMobileSensorsCapabilityService::GetSnapshot()
{
	check(IsInGameThread());
	OpenMobileSensorsCapabilityServicePrivate::RefreshAndBroadcast();
	return OpenMobileSensorsCapabilityServicePrivate::CachedSnapshot;
}

void FOpenMobileSensorsCapabilityService::HandleBackendGenerationChanged()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsCapabilityServicePrivate;
	bBackendDirty = true;
	bSnapshotDirty = true;
	if (bStarted)
	{
		RefreshAndBroadcast();
	}
}

void FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
	FName Permission,
	EOpenMobilePermissionStatus Status
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsCapabilityServicePrivate;
	if (Permission.IsNone())
	{
		return;
	}
	if (const EOpenMobilePermissionStatus* Existing =
		PermissionStatuses.Find(Permission);
		Existing && *Existing == Status)
	{
		return;
	}
	PermissionStatuses.Add(Permission, Status);
	bSnapshotDirty = true;
	RefreshAndBroadcast();
}

void FOpenMobileSensorsCapabilityService::SetApplicationActive(bool bActive)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsCapabilityServicePrivate;
	if (bApplicationActive == bActive)
	{
		return;
	}
	bApplicationActive = bActive;
	bSnapshotDirty = true;
	RefreshAndBroadcast();
}

void FOpenMobileSensorsCapabilityService::SetLocationInputAvailable(
	bool bAvailable
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsCapabilityServicePrivate;
	if (bLocationInputAvailable == bAvailable)
	{
		return;
	}
	bLocationInputAvailable = bAvailable;
	bSnapshotDirty = true;
	RefreshAndBroadcast();
}

FOnOpenMobileSensorCapabilityMatrixChanged&
FOpenMobileSensorsCapabilityService::OnChanged()
{
	return OpenMobileSensorsCapabilityServicePrivate::ChangedEvent;
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileSensorsCapabilityService::ResetForTests()
{
	check(IsInGameThread());
	BeginShutdown();
	Start();
}
#endif
