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
		const bool bHasUsableDirectSource = Derived
			&& Derived->Availability.State ==
				EOpenMobileCapabilityState::Available
			&& (Derived->MaximumFrequencyHz <= 0.0
				|| Derived->MaximumFrequencyHz >=
					MinimumFallbackFrequencyHz);
		if (!Derived
			|| bHasUsableDirectSource
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
