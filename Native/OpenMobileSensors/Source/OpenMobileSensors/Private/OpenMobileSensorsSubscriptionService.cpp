#include "OpenMobileSensorsSubscriptionService.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "IOpenMobileSensorsBackend.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeExit.h"
#include "OpenMobileAsync.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorScreenRotationService.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorsTrueHeadingService.h"
#include "OpenMobileSensorsSettings.h"

namespace OpenMobileSensorsSubscriptionServicePrivate
{
	struct FPhysicalStreamKey
	{
		FOpenMobileSensorIdentifier Sensor;
		EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame =
			EOpenMobileAttitudeReferenceFrame::GameRelative;

		bool operator==(const FPhysicalStreamKey& Other) const
		{
			return Sensor == Other.Sensor
				&& AttitudeReferenceFrame == Other.AttitudeReferenceFrame;
		}

		friend uint32 GetTypeHash(const FPhysicalStreamKey& Key)
		{
			uint32 Hash = GetTypeHash(Key.Sensor);
			Hash = HashCombine(
				Hash,
				GetTypeHash(static_cast<uint8>(Key.AttitudeReferenceFrame))
			);
			return Hash;
		}
	};

	struct FSubscriptionEntry
	{
		FGuid OwnerIdentifier;
		FOpenMobileSensorSubscriptionHandle Handle;
		FOpenMobileSensorSubscriptionRequest Request;
		FOpenMobileSensorStreamOptions AppliedOptions;
		FOpenMobileSensorRateResolution RateResolution;
		EOpenMobileSensorRateAdjustmentReason CommonRateAdjustmentReason =
			EOpenMobileSensorRateAdjustmentReason::None;
		FPhysicalStreamKey PhysicalKey;
		FOpenMobileSensorsBackendToken BackendToken;
		EOpenMobileSensorSubscriptionState State =
			EOpenMobileSensorSubscriptionState::Accepted;
		FOpenMobileError Error;
		FOpenMobileSensorFailureDetails Failure;
		double LastDeliveryTimestampSeconds = 0.0;
		bool bHasDeliveredSample = false;
		bool bResettableStepCountSession = false;
		bool bPausedForLifecycle = false;
		uint8 NativeRecoveryAttemptCount = 0;
		double NextRecoveryAttemptSeconds = 0.0;
	};

	struct FPhysicalStreamEntry
	{
		FOpenMobileSensorBackendStreamHandle Handle;
		FPhysicalStreamKey Key;
		FOpenMobileSensorPhysicalStreamRequest Request;
		FOpenMobileSensorsBackendToken BackendToken;
		IOpenMobileSensorsBackend* Backend = nullptr;
	};

	struct FPendingFlush
	{
		FGuid OwnerIdentifier;
		FOpenMobileSensorSubscriptionHandle Handle;
		FOpenMobileSensorBackendStreamHandle PhysicalStreamHandle;
		FOpenMobileSensorsBackendToken BackendToken;
		double DeadlineSeconds = 0.0;
		TFunction<void(const FOpenMobileSensorFlushResult&)> Completion;
	};

	struct FRecentErrorEntry
	{
		FGuid OwnerIdentifier;
		FOpenMobileSensorErrorReport Report;
	};

	TMap<FGuid, FSubscriptionEntry> Subscriptions;
	TMap<FPhysicalStreamKey, FPhysicalStreamEntry> PhysicalStreams;
	FOnOpenMobileSensorSubscriptionServiceStateChanged StateChangedEvent;
	FTSTicker::FDelegateHandle PendingOperationsTickHandle;
	double PendingOperationsDeadlineSeconds = 0.0;
	FTSTicker::FDelegateHandle FlushTimeoutTickHandle;
	TMap<FGuid, FPendingFlush> PendingFlushes;
	TArray<FRecentErrorEntry> RecentErrors;
	uint32 NextHandleGeneration = 1;
	bool bShuttingDown = false;
	bool bApplicationActive = true;
	bool bApplicationInForeground = true;
	FDelegateHandle DeactivatedHandle;
	FDelegateHandle ReactivatedHandle;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;

	FOpenMobileSensorOperationResult MakeSuccess(
		EOpenMobileSensorResultCode ResultCode =
			EOpenMobileSensorResultCode::Success
	)
	{
		FOpenMobileSensorOperationResult Result;
		Result.Code = ResultCode;
		return Result;
	}

	void RecordRecentError(
		const FGuid& OwnerIdentifier,
		FOpenMobileSensorErrorReport Report
	)
	{
		if (!Report.Error.IsSet())
		{
			return;
		}
		constexpr int32 MaximumRecentErrors = 32;
		FRecentErrorEntry& Entry = RecentErrors.AddDefaulted_GetRef();
		Entry.OwnerIdentifier = OwnerIdentifier;
		Entry.Report = MoveTemp(Report);
		if (RecentErrors.Num() > MaximumRecentErrors)
		{
			RecentErrors.RemoveAt(
				0,
				RecentErrors.Num() - MaximumRecentErrors,
				EAllowShrinking::No
			);
		}
	}

	FName GetBackendName(const FSubscriptionEntry* Subscription = nullptr)
	{
		if (Subscription)
		{
			if (const FPhysicalStreamEntry* Physical =
				PhysicalStreams.Find(Subscription->PhysicalKey))
			{
				return Physical->Backend
					? Physical->Backend->GetBackendName()
					: NAME_None;
			}
		}
		const IOpenMobileSensorsBackend* Backend =
			FOpenMobileSensorsBackendRegistry::FindBackend();
		return Backend ? Backend->GetBackendName() : NAME_None;
	}

	void RecordOperationFailure(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorOperationResult& Operation,
		const FOpenMobileSensorErrorContext& Context
	)
	{
		if (!OwnerIdentifier.IsValid() || Operation.IsSuccess())
		{
			return;
		}
		RecordRecentError(
			OwnerIdentifier,
			FOpenMobileSensorsErrorMapper::Describe(Operation, Context)
		);
	}

	FOpenMobileSensorOperationResult MakeHandleFailure(
		const FOpenMobileSensorSubscriptionHandle& Handle
	)
	{
		if (!Handle.IsValid())
		{
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::InvalidHandle
			);
		}
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::StaleHandle
		);
	}

	uint32 AllocateGeneration()
	{
		const uint32 Generation = NextHandleGeneration++;
		if (NextHandleGeneration == 0)
		{
			NextHandleGeneration = 1;
		}
		return Generation == 0 ? NextHandleGeneration++ : Generation;
	}

	bool IsFiniteInRange(double Value, double Minimum, double Maximum)
	{
		return FMath::IsFinite(Value)
			&& Value >= Minimum
			&& Value <= Maximum;
	}

	template <typename EnumType>
	bool IsValidEnum(EnumType Value)
	{
		return StaticEnum<EnumType>()->IsValidEnumValue(
			static_cast<int64>(Value)
		);
	}

	bool ValidateFilterOptions(
		const FOpenMobileSensorFilterOptions& Filters
	)
	{
		return (!Filters.bEnableLowPass
				|| IsFiniteInRange(
					Filters.LowPassTimeConstantSeconds,
					0.0001,
					60.0
				))
			&& (!Filters.bEnableHighPass
				|| IsFiniteInRange(
					Filters.HighPassTimeConstantSeconds,
					0.0001,
					60.0
				))
			&& (!Filters.bEnableExponentialSmoothing
				|| IsFiniteInRange(
					Filters.SmoothingTimeConstantSeconds,
					0.0001,
					60.0
				))
			&& FMath::IsFinite(Filters.DeadZone)
			&& Filters.DeadZone >= 0.0;
	}

	bool ValidateShakeDetectionOptions(
		const FOpenMobileShakeDetectionOptions& Options
	)
	{
		return IsFiniteInRange(
				Options.StrengthThresholdMetresPerSecondSquared,
				0.1,
				1000.0
			)
			&& Options.MinimumImpulses >= 1
			&& Options.MinimumImpulses <= 32
			&& IsFiniteInRange(
				Options.DurationWindowSeconds,
				0.01,
				10.0
			)
			&& IsFiniteInRange(
				Options.QuietResetSeconds,
				0.0,
				5.0
			)
			&& IsFiniteInRange(
				Options.CooldownSeconds,
				0.0,
				60.0
			);
	}

	bool ResolvePreset(
		const FOpenMobileSensorStreamOptions& Requested,
		const UOpenMobileSensorsSettings& Settings,
		FOpenMobileSensorStreamOptions& OutApplied
	)
	{
		OutApplied = Requested;
		const FOpenMobileSensorRatePresetSettings* Preset = nullptr;
		switch (Requested.RatePreset)
		{
		case EOpenMobileSensorRatePreset::UI:
			Preset = &Settings.UIPreset;
			break;
		case EOpenMobileSensorRatePreset::Game:
			Preset = &Settings.GamePreset;
			break;
		case EOpenMobileSensorRatePreset::Fast:
			Preset = &Settings.FastPreset;
			break;
		case EOpenMobileSensorRatePreset::Custom:
			return true;
		default:
			return false;
		}
		OutApplied.CustomFrequencyHz = Preset->RequestedFrequencyHz;
		OutApplied.MaximumDeliveryLatencySeconds =
			Preset->MaximumDeliveryLatencySeconds;
		OutApplied.MaximumCallbackFrequencyHz =
			Preset->MaximumCallbackFrequencyHz;
		return true;
	}

	void ApplyLowPowerDefaults(
		const FOpenMobileSensorIdentifier& Sensor,
		const FOpenMobileSensorStreamOptions& Requested,
		FOpenMobileSensorStreamOptions& OutApplied
	)
	{
		if ((Sensor.Type == EOpenMobileSensorType::BarometricPressure
			|| Sensor.Type == EOpenMobileSensorType::AmbientLight)
			&& Requested.RatePreset == EOpenMobileSensorRatePreset::UI)
		{
			OutApplied.CustomFrequencyHz = 1.0;
			OutApplied.MaximumCallbackFrequencyHz = 1.0;
		}
	}

	bool IsPressureRateSupported(
		const FOpenMobileSensorIdentifier& Sensor,
		double RequestedFrequencyHz
	)
	{
		if (Sensor.Type != EOpenMobileSensorType::BarometricPressure)
		{
			return true;
		}
		const FOpenMobileSensorCapabilitySnapshot Snapshot =
			FOpenMobileSensorsCapabilityService::GetSnapshot();
		const FOpenMobileSensorCapability* Capability =
			Snapshot.Sensors.FindByPredicate(
				[&Sensor](const FOpenMobileSensorCapability& Candidate)
				{
					return Candidate.Sensor.Type == Sensor.Type
						&& (Sensor.InstanceId.IsNone()
							|| Candidate.Sensor.InstanceId ==
								Sensor.InstanceId);
				}
			);
		return !Capability
			|| Capability->MaximumFrequencyHz <= 0.0
			|| RequestedFrequencyHz <=
				Capability->MaximumFrequencyHz + 1.e-9;
	}

	void ApplyRateLimits(
		const FOpenMobileSensorIdentifier& Sensor,
		const FOpenMobileSensorStreamOptions& Requested,
		const UOpenMobileSensorsSettings& Settings,
		FOpenMobileSensorStreamOptions& OutApplied,
		FOpenMobileSensorRateResolution& OutResolution
	)
	{
		constexpr double NormalMaximumFrequencyHz = 200.0;
		OutResolution = {};
		OutResolution.RequestedFrequencyHz = OutApplied.CustomFrequencyHz;
		const FOpenMobileSensorCapabilitySnapshot Snapshot =
			FOpenMobileSensorsCapabilityService::GetSnapshot();
		const FOpenMobileSensorCapability* Capability =
			Snapshot.Sensors.FindByPredicate(
				[&Sensor](const FOpenMobileSensorCapability& Candidate)
				{
					return Candidate.Sensor.Type == Sensor.Type
						&& (Sensor.InstanceId.IsNone()
							|| Candidate.Sensor.InstanceId ==
								Sensor.InstanceId);
				}
			);
		if (Capability)
		{
			const bool bHasMinimum =
				FMath::IsFinite(Capability->MinimumFrequencyHz)
				&& Capability->MinimumFrequencyHz > 0.0;
			const bool bHasMaximum =
				FMath::IsFinite(Capability->MaximumFrequencyHz)
				&& Capability->MaximumFrequencyHz > 0.0;
			if (bHasMinimum && bHasMaximum
				&& Capability->MinimumFrequencyHz <=
					Capability->MaximumFrequencyHz)
			{
				OutApplied.CustomFrequencyHz = FMath::Clamp(
					OutApplied.CustomFrequencyHz,
					Capability->MinimumFrequencyHz,
					Capability->MaximumFrequencyHz
				);
			}
			else if (bHasMaximum)
			{
				OutApplied.CustomFrequencyHz = FMath::Min(
					OutApplied.CustomFrequencyHz,
					Capability->MaximumFrequencyHz
				);
			}
			else if (bHasMinimum)
			{
				OutApplied.CustomFrequencyHz = FMath::Max(
					OutApplied.CustomFrequencyHz,
					Capability->MinimumFrequencyHz
				);
			}
			if (OutApplied.CustomFrequencyHz !=
				OutResolution.RequestedFrequencyHz)
			{
				OutResolution.AdjustmentReason =
					EOpenMobileSensorRateAdjustmentReason::HardwareLimit;
			}
		}
		if (Sensor.Type == EOpenMobileSensorType::AmbientLight)
		{
			constexpr double AmbientLightMaximumFrequencyHz = 5.0;
			double PhysicalCeilingHz = AmbientLightMaximumFrequencyHz;
			if (Capability
				&& FMath::IsFinite(Capability->MinimumFrequencyHz)
				&& Capability->MinimumFrequencyHz > PhysicalCeilingHz)
			{
				PhysicalCeilingHz = Capability->MinimumFrequencyHz;
			}
			const double BeforeAmbientLightLimit =
				OutApplied.CustomFrequencyHz;
			OutApplied.CustomFrequencyHz = FMath::Min(
				OutApplied.CustomFrequencyHz,
				PhysicalCeilingHz
			);
			OutApplied.MaximumCallbackFrequencyHz = FMath::Min(
				OutApplied.MaximumCallbackFrequencyHz,
				AmbientLightMaximumFrequencyHz
			);
			if (OutApplied.CustomFrequencyHz < BeforeAmbientLightLimit)
			{
				OutResolution.AdjustmentReason =
					EOpenMobileSensorRateAdjustmentReason::ProjectPolicy;
			}
		}
		if (!Requested.bAllowHighSamplingRate
			|| !Settings.bAllowHighSamplingRate)
		{
			const double BeforeProjectLimit =
				OutApplied.CustomFrequencyHz;
			OutApplied.CustomFrequencyHz = FMath::Min(
				OutApplied.CustomFrequencyHz,
				NormalMaximumFrequencyHz
			);
			if (OutApplied.CustomFrequencyHz < BeforeProjectLimit)
			{
				OutResolution.AdjustmentReason =
					EOpenMobileSensorRateAdjustmentReason::ProjectPolicy;
			}
		}
		if (OutApplied.CustomFrequencyHz > NormalMaximumFrequencyHz)
		{
			const IOpenMobileSensorsBackend* Backend =
				FOpenMobileSensorsBackendRegistry::FindBackend();
			if (Backend
				&& Backend->RequiresHighSamplingRateDeclaration()
				&& !Backend->HasHighSamplingRateDeclaration())
			{
				OutApplied.CustomFrequencyHz = NormalMaximumFrequencyHz;
				OutResolution.AdjustmentReason =
					EOpenMobileSensorRateAdjustmentReason::
						MissingPlatformDeclaration;
			}
		}
		OutApplied.MaximumCallbackFrequencyHz = FMath::Min(
			OutApplied.MaximumCallbackFrequencyHz,
			OutApplied.CustomFrequencyHz
		);
		OutResolution.ClampedFrequencyHz = OutApplied.CustomFrequencyHz;
		OutResolution.AppliedNativeFrequencyHz =
			OutApplied.CustomFrequencyHz;
	}

	bool ValidateAndResolveOptions(
		const FOpenMobileSensorIdentifier& Sensor,
		const FOpenMobileSensorStreamOptions& Requested,
		FOpenMobileSensorStreamOptions& OutApplied,
		FOpenMobileSensorRateResolution& OutRateResolution
	)
	{
		const int32 AllowedAttitudeRepresentations =
			static_cast<int32>(
				EOpenMobileAttitudeRepresentation::Quaternion
			)
			| static_cast<int32>(
				EOpenMobileAttitudeRepresentation::EulerAngles
			)
			| static_cast<int32>(
				EOpenMobileAttitudeRepresentation::RotationMatrix
			);
		if (!Sensor.IsValid()
			|| !IsValidEnum(Requested.RatePreset)
			|| !IsValidEnum(Requested.DeliveryMode)
			|| !IsValidEnum(Requested.CoordinateSpace)
			|| !IsValidEnum(Requested.OverflowPolicy)
			|| !IsValidEnum(Requested.LifecyclePolicy)
			|| !IsValidEnum(Requested.AttitudeReferenceFrame)
			|| !IsValidEnum(Requested.MinimumCallbackAccuracy)
			|| !IsValidEnum(Requested.MinimumActivityConfidence)
			|| !IsFiniteInRange(Requested.CustomFrequencyHz, 1.0, 1000.0)
			|| !IsFiniteInRange(
				Requested.MaximumDeliveryLatencySeconds,
				0.0,
				10.0
			)
			|| !IsFiniteInRange(
				Requested.MaximumCallbackFrequencyHz,
				1.0,
				120.0
			)
			|| Requested.BufferCapacitySamples < 1
			|| Requested.BufferCapacitySamples > 4096
			|| !FMath::IsFinite(Requested.MinimumScalarEventChange)
			|| Requested.MinimumScalarEventChange < 0.0
			|| !IsFiniteInRange(
				Requested.MinimumActivityStableDurationSeconds,
				0.0,
				3600.0
			)
			|| !ValidateFilterOptions(Requested.Filters)
			|| !ValidateShakeDetectionOptions(Requested.ShakeDetection)
			|| Requested.AttitudeRepresentations == 0
			|| (Requested.AttitudeRepresentations
				& ~AllowedAttitudeRepresentations) != 0)
		{
			return false;
		}

		const UOpenMobileSensorsSettings* Settings =
			GetDefault<UOpenMobileSensorsSettings>();
		if (!ResolvePreset(Requested, *Settings, OutApplied))
		{
			return false;
		}
		ApplyLowPowerDefaults(Sensor, Requested, OutApplied);
		if (!IsPressureRateSupported(
			Sensor,
			OutApplied.CustomFrequencyHz
		))
		{
			return false;
		}
		ApplyRateLimits(
			Sensor,
			Requested,
			*Settings,
			OutApplied,
			OutRateResolution
		);
		if (OutApplied.bLowLatency)
		{
			OutApplied.MaximumDeliveryLatencySeconds = 0.0;
		}
		return IsFiniteInRange(OutApplied.CustomFrequencyHz, 1.0, 1000.0)
			&& IsFiniteInRange(
				OutApplied.MaximumDeliveryLatencySeconds,
				0.0,
				10.0
			)
			&& IsFiniteInRange(
				OutApplied.MaximumCallbackFrequencyHz,
				1.0,
				120.0
			);
	}

	FPhysicalStreamKey MakePhysicalKey(
		const FOpenMobileSensorIdentifier& Sensor,
		const FOpenMobileSensorStreamOptions& Options
	)
	{
		FPhysicalStreamKey Key;
		Key.Sensor = Sensor;
		if (Sensor.Type == EOpenMobileSensorType::Attitude)
		{
			Key.AttitudeReferenceFrame = Options.AttitudeReferenceFrame;
		}
		return Key;
	}

	bool ResolvePhysicalSensor(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorIdentifier& LogicalSensor,
		const FOpenMobileSensorStreamOptions& Options,
		FOpenMobileSensorIdentifier& OutPhysicalSensor,
		EOpenMobileSensorFailureReason& OutFailureReason
	)
	{
		OutPhysicalSensor = LogicalSensor;
		OutFailureReason =
			EOpenMobileSensorFailureReason::DerivedInputUnavailable;
		if (LogicalSensor.Type == EOpenMobileSensorType::Shake)
		{
			if (!Options.bAllowDerivedFallback)
			{
				return false;
			}
			const FOpenMobileSensorCapabilitySnapshot Snapshot =
				FOpenMobileSensorsCapabilityService::GetSnapshot();
			const FOpenMobileSensorCapability* Shake =
				Snapshot.Sensors.FindByPredicate(
					[](const FOpenMobileSensorCapability& Capability)
					{
						return Capability.Sensor.Type ==
							EOpenMobileSensorType::Shake;
					}
				);
			if (!Shake
				|| Shake->Availability.State !=
					EOpenMobileCapabilityState::Available
				|| !Shake->Fallback.bAvailable
				|| Shake->Fallback.RequiredInputs.Num() != 1)
			{
				if (Shake)
				{
					switch (Shake->Availability.State)
					{
					case EOpenMobileCapabilityState::PermissionRequired:
						OutFailureReason = EOpenMobileSensorFailureReason::
							PermissionRequired;
						break;
					case EOpenMobileCapabilityState::Denied:
						OutFailureReason = EOpenMobileSensorFailureReason::
							PermissionDenied;
						break;
					case EOpenMobileCapabilityState::Restricted:
						OutFailureReason = EOpenMobileSensorFailureReason::
							PermissionRestricted;
						break;
					case EOpenMobileCapabilityState::TemporarilyUnavailable:
						OutFailureReason = EOpenMobileSensorFailureReason::
							TemporarilyUnavailable;
						break;
					case EOpenMobileCapabilityState::Unavailable:
						OutFailureReason =
							EOpenMobileSensorFailureReason::MissingHardware;
						break;
					default:
						break;
					}
				}
				return false;
			}
			const EOpenMobileSensorType InputType =
				Shake->Fallback.RequiredInputs[0];
			const FOpenMobileSensorCapability* Input =
				Snapshot.Sensors.FindByPredicate(
					[InputType](const FOpenMobileSensorCapability& Capability)
					{
						return Capability.Sensor.Type == InputType
							&& Capability.Availability.State ==
								EOpenMobileCapabilityState::Available;
					}
				);
			if (!Input)
			{
				return false;
			}
			OutPhysicalSensor = Input->Sensor;
			OutFailureReason = EOpenMobileSensorFailureReason::None;
			return true;
		}
		if (LogicalSensor.Type == EOpenMobileSensorType::TrueHeading)
		{
			FOpenMobileSensorsCapabilityService::RefreshPermissionStatus(
				FOpenMobileSensorPermissions::GetPermissionName(
					EOpenMobileSensorPermission::TrueHeadingLocation
				)
			);
			const FOpenMobileSensorCapabilitySnapshot Snapshot =
				FOpenMobileSensorsCapabilityService::GetSnapshot();
			const FOpenMobileSensorCapability* TrueHeading =
				Snapshot.Sensors.FindByPredicate(
					[&LogicalSensor](
						const FOpenMobileSensorCapability& Capability
					)
					{
						return Capability.Sensor.Type ==
							EOpenMobileSensorType::TrueHeading
							&& (LogicalSensor.InstanceId.IsNone()
								|| Capability.Sensor.InstanceId ==
									LogicalSensor.InstanceId);
					}
				);
			if (TrueHeading
				&& TrueHeading->ActiveRestriction ==
					EOpenMobileSensorRestriction::Permission)
			{
				switch (TrueHeading->Availability.State)
				{
				case EOpenMobileCapabilityState::Denied:
					OutFailureReason =
						EOpenMobileSensorFailureReason::PermissionDenied;
					return false;
				case EOpenMobileCapabilityState::Restricted:
					OutFailureReason =
						EOpenMobileSensorFailureReason::PermissionRestricted;
					return false;
				case EOpenMobileCapabilityState::PermissionRequired:
					OutFailureReason =
						EOpenMobileSensorFailureReason::PermissionRequired;
					return false;
				default:
					break;
				}
			}
			FOpenMobileSensorLocationInput LocationInput;
			double LocationAgeSeconds = 0.0;
			OutFailureReason = FOpenMobileSensorsTrueHeadingService::
				GetUsableLocationInput(
					OwnerIdentifier,
					FPlatformTime::Seconds(),
					LocationInput,
					LocationAgeSeconds
				);
			if (OutFailureReason != EOpenMobileSensorFailureReason::None)
			{
				return false;
			}
			const bool bDirectAvailable = TrueHeading
				&& TrueHeading->Availability.State ==
					EOpenMobileCapabilityState::Available
				&& TrueHeading->Source !=
					EOpenMobileSensorAvailabilitySource::Derived;
			if (bDirectAvailable)
			{
				return true;
			}
			if (!Options.bAllowDerivedFallback)
			{
				return false;
			}
			const FOpenMobileSensorCapability* MagneticHeading =
				Snapshot.Sensors.FindByPredicate(
					[](const FOpenMobileSensorCapability& Capability)
					{
						return Capability.Sensor.Type ==
							EOpenMobileSensorType::MagneticHeading
							&& Capability.Availability.State ==
								EOpenMobileCapabilityState::Available;
					}
				);
			if (!MagneticHeading)
			{
				return false;
			}
			OutPhysicalSensor = MagneticHeading->Sensor;
			return true;
		}
		if (LogicalSensor.Type == EOpenMobileSensorType::Attitude)
		{
			const FOpenMobileSensorCapabilitySnapshot Snapshot =
				FOpenMobileSensorsCapabilityService::GetSnapshot();
			const FOpenMobileSensorCapability* Attitude =
				Snapshot.Sensors.FindByPredicate(
					[&LogicalSensor](
						const FOpenMobileSensorCapability& Capability
					)
					{
						return Capability.Sensor.Type ==
							EOpenMobileSensorType::Attitude
							&& (LogicalSensor.InstanceId.IsNone()
								|| Capability.Sensor.InstanceId ==
									LogicalSensor.InstanceId);
					}
				);
			if (!Attitude
				|| Attitude->Availability.State !=
					EOpenMobileCapabilityState::Available
				|| Attitude->Source ==
					EOpenMobileSensorAvailabilitySource::Derived)
			{
				return false;
			}
			if (Attitude->AttitudeReferenceFrames.IsEmpty())
			{
				return true;
			}
			const FOpenMobileAttitudeReferenceFrameCapability* Reference =
				Attitude->AttitudeReferenceFrames.FindByPredicate(
					[&Options](
						const FOpenMobileAttitudeReferenceFrameCapability& Candidate
					)
					{
						return Candidate.ReferenceFrame ==
							Options.AttitudeReferenceFrame;
					}
				);
			const bool bAvailable = Reference
				&& Reference->Availability.State ==
					EOpenMobileCapabilityState::Available;
			if (!bAvailable)
			{
				OutFailureReason =
					EOpenMobileSensorFailureReason::InvalidReferenceFrame;
			}
			return bAvailable;
		}
		if (LogicalSensor.Type == EOpenMobileSensorType::RelativeAltitude)
		{
			const FOpenMobileSensorCapabilitySnapshot Snapshot =
				FOpenMobileSensorsCapabilityService::GetSnapshot();
			const FOpenMobileSensorCapability* RelativeAltitude =
				Snapshot.Sensors.FindByPredicate(
					[&LogicalSensor](
						const FOpenMobileSensorCapability& Capability
					)
					{
						return Capability.Sensor.Type ==
							LogicalSensor.Type
							&& (LogicalSensor.InstanceId.IsNone()
								|| Capability.Sensor.InstanceId ==
									LogicalSensor.InstanceId);
					}
				);
			const bool bDirectAvailable = RelativeAltitude
				&& RelativeAltitude->Availability.State ==
					EOpenMobileCapabilityState::Available
				&& RelativeAltitude->Source !=
					EOpenMobileSensorAvailabilitySource::Derived;
			if (bDirectAvailable)
			{
				return true;
			}
			if (!Options.bAllowDerivedFallback)
			{
				return false;
			}
			const FOpenMobileSensorCapability* Pressure =
				Snapshot.Sensors.FindByPredicate(
					[](const FOpenMobileSensorCapability& Capability)
					{
						return Capability.Sensor.Type ==
							EOpenMobileSensorType::BarometricPressure
							&& Capability.Availability.State ==
								EOpenMobileCapabilityState::Available;
					}
				);
			if (!Pressure)
			{
				return false;
			}
			OutPhysicalSensor = Pressure->Sensor;
			return true;
		}
		if (LogicalSensor.Type == EOpenMobileSensorType::StepCounter
			|| LogicalSensor.Type == EOpenMobileSensorType::StepDetector
			|| LogicalSensor.Type == EOpenMobileSensorType::MotionActivity
			|| LogicalSensor.Type == EOpenMobileSensorType::ActivityTransition)
		{
			const FOpenMobileSensorCapabilitySnapshot Snapshot =
				FOpenMobileSensorsCapabilityService::GetSnapshot();
			const FOpenMobileSensorCapability* DirectCapability =
				Snapshot.Sensors.FindByPredicate(
					[&LogicalSensor](
						const FOpenMobileSensorCapability& Capability
					)
					{
						return Capability.Sensor.Type ==
							LogicalSensor.Type
							&& (LogicalSensor.InstanceId.IsNone()
								|| Capability.Sensor.InstanceId ==
									LogicalSensor.InstanceId);
					}
				);
			if (LogicalSensor.Type ==
					EOpenMobileSensorType::ActivityTransition
				&& DirectCapability
				&& DirectCapability->Availability.State ==
					EOpenMobileCapabilityState::Available
				&& DirectCapability->Source ==
					EOpenMobileSensorAvailabilitySource::Derived)
			{
				if (!Options.bAllowDerivedFallback)
				{
					return false;
				}
				const FOpenMobileSensorCapability* Activity =
					Snapshot.Sensors.FindByPredicate(
						[](const FOpenMobileSensorCapability& Capability)
						{
							return Capability.Sensor.Type ==
								EOpenMobileSensorType::MotionActivity
								&& Capability.Availability.State ==
									EOpenMobileCapabilityState::Available;
						}
					);
				if (!Activity)
				{
					return false;
				}
				OutPhysicalSensor = Activity->Sensor;
				return true;
			}
			if (DirectCapability
				&& DirectCapability->Availability.State ==
					EOpenMobileCapabilityState::Available
				&& (LogicalSensor.Type ==
						EOpenMobileSensorType::MotionActivity
					|| DirectCapability->Source !=
						EOpenMobileSensorAvailabilitySource::Derived))
			{
				return true;
			}
			if (!DirectCapability
				|| DirectCapability->Availability.State ==
					EOpenMobileCapabilityState::Unavailable)
			{
				OutFailureReason =
					EOpenMobileSensorFailureReason::MissingHardware;
			}
			else
			{
				switch (DirectCapability->Availability.State)
				{
				case EOpenMobileCapabilityState::PermissionRequired:
					OutFailureReason =
						EOpenMobileSensorFailureReason::PermissionRequired;
					break;
				case EOpenMobileCapabilityState::Denied:
					OutFailureReason =
						EOpenMobileSensorFailureReason::PermissionDenied;
					break;
				case EOpenMobileCapabilityState::Restricted:
					OutFailureReason =
						EOpenMobileSensorFailureReason::PermissionRestricted;
					break;
				case EOpenMobileCapabilityState::TemporarilyUnavailable:
					OutFailureReason = EOpenMobileSensorFailureReason::
						TemporarilyUnavailable;
					break;
				case EOpenMobileCapabilityState::NotConfigured:
					OutFailureReason =
						EOpenMobileSensorFailureReason::ConfigurationBlocked;
					break;
				case EOpenMobileCapabilityState::NotSupported:
				case EOpenMobileCapabilityState::Available:
				case EOpenMobileCapabilityState::Unavailable:
				default:
					OutFailureReason =
						EOpenMobileSensorFailureReason::UnsupportedPlatform;
					break;
				}
			}
			return false;
		}
		if (LogicalSensor.Type == EOpenMobileSensorType::AbsoluteAltitude
			|| LogicalSensor.Type == EOpenMobileSensorType::AmbientLight
			|| LogicalSensor.Type == EOpenMobileSensorType::Proximity)
		{
			const FOpenMobileSensorCapabilitySnapshot Snapshot =
				FOpenMobileSensorsCapabilityService::GetSnapshot();
			const FOpenMobileSensorCapability* DirectCapability =
				Snapshot.Sensors.FindByPredicate(
					[&LogicalSensor](
						const FOpenMobileSensorCapability& Capability
					)
					{
						return Capability.Sensor.Type ==
							LogicalSensor.Type
							&& (LogicalSensor.InstanceId.IsNone()
								|| Capability.Sensor.InstanceId ==
									LogicalSensor.InstanceId);
					}
				);
			if (DirectCapability
				&& DirectCapability->Availability.State ==
					EOpenMobileCapabilityState::Available
				&& DirectCapability->Source !=
					EOpenMobileSensorAvailabilitySource::Derived)
			{
				return true;
			}
			if (!DirectCapability
				|| DirectCapability->Availability.State ==
					EOpenMobileCapabilityState::Unavailable)
			{
				OutFailureReason =
					EOpenMobileSensorFailureReason::MissingHardware;
			}
			else if (DirectCapability->Availability.State ==
				EOpenMobileCapabilityState::TemporarilyUnavailable)
			{
				OutFailureReason = EOpenMobileSensorFailureReason::
					TemporarilyUnavailable;
			}
			else
			{
				OutFailureReason =
					EOpenMobileSensorFailureReason::UnsupportedPlatform;
			}
			return false;
		}
		if (LogicalSensor.Type ==
			EOpenMobileSensorType::BarometricPressure)
		{
			const FOpenMobileSensorCapabilitySnapshot Snapshot =
				FOpenMobileSensorsCapabilityService::GetSnapshot();
			const FOpenMobileSensorCapability* Pressure =
				Snapshot.Sensors.FindByPredicate(
					[&LogicalSensor](
						const FOpenMobileSensorCapability& Capability
					)
					{
						return Capability.Sensor.Type ==
							LogicalSensor.Type
							&& (LogicalSensor.InstanceId.IsNone()
								|| Capability.Sensor.InstanceId ==
									LogicalSensor.InstanceId);
					}
				);
			if (Pressure
				&& Pressure->Availability.State !=
					EOpenMobileCapabilityState::Available)
			{
				OutFailureReason = Pressure->Availability.State ==
					EOpenMobileCapabilityState::TemporarilyUnavailable
					? EOpenMobileSensorFailureReason::TemporarilyUnavailable
					: EOpenMobileSensorFailureReason::MissingHardware;
				return false;
			}
			return true;
		}
		const bool bSupportsAccelerometerFallback =
			LogicalSensor.Type == EOpenMobileSensorType::Gravity
			|| LogicalSensor.Type ==
				EOpenMobileSensorType::LinearAcceleration
			|| LogicalSensor.Type ==
				EOpenMobileSensorType::PhysicalOrientation;
		if (!bSupportsAccelerometerFallback)
		{
			return true;
		}
		const FOpenMobileSensorCapabilitySnapshot Snapshot =
			FOpenMobileSensorsCapabilityService::GetSnapshot();
		const FOpenMobileSensorCapability* Derived =
			Snapshot.Sensors.FindByPredicate(
				[&LogicalSensor](
					const FOpenMobileSensorCapability& Capability
				)
				{
					return Capability.Sensor.Type == LogicalSensor.Type
						&& (LogicalSensor.InstanceId.IsNone()
							|| Capability.Sensor.InstanceId ==
								LogicalSensor.InstanceId);
				}
			);
		constexpr double MinimumDerivedFrequencyHz = 15.0;
		const bool bHasUsableDirectSource = Derived
			&& Derived->Availability.State ==
				EOpenMobileCapabilityState::Available
			&& Derived->Source !=
				EOpenMobileSensorAvailabilitySource::Derived
			&& (Derived->MaximumFrequencyHz <= 0.0
				|| Derived->MaximumFrequencyHz >=
					MinimumDerivedFrequencyHz);
		if (bHasUsableDirectSource)
		{
			return true;
		}
		if (!Options.bAllowDerivedFallback)
		{
			return false;
		}
		const FOpenMobileSensorCapability* Accelerometer =
			Snapshot.Sensors.FindByPredicate(
				[](const FOpenMobileSensorCapability& Capability)
				{
					return Capability.Sensor.Type ==
						EOpenMobileSensorType::Accelerometer
						&& Capability.Availability.State ==
							EOpenMobileCapabilityState::Available
						&& (Capability.MaximumFrequencyHz <= 0.0
							|| Capability.MaximumFrequencyHz >=
								MinimumDerivedFrequencyHz);
				}
			);
		if (!Accelerometer)
		{
			return false;
		}
		OutPhysicalSensor = Accelerometer->Sensor;
		return true;
	}

	FSubscriptionEntry* FindOwnedEntry(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle
	)
	{
		if (!OwnerIdentifier.IsValid() || !Handle.IsValid())
		{
			return nullptr;
		}
		FSubscriptionEntry* Entry = Subscriptions.Find(
			Handle.GetIdentifier()
		);
		if (!Entry
			|| Entry->OwnerIdentifier != OwnerIdentifier
			|| Entry->Handle != Handle
			|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
				Entry->BackendToken
			))
		{
			return nullptr;
		}
		return Entry;
	}

	FOpenMobileSensorSubscriptionStateSnapshot MakeSnapshot(
		const FSubscriptionEntry& Entry
	)
	{
		FOpenMobileSensorSubscriptionStateSnapshot Snapshot;
		Snapshot.Handle = Entry.Handle;
		Snapshot.Sensor = Entry.Request.Sensor;
		Snapshot.bResettableStepCountSession =
			Entry.bResettableStepCountSession;
		Snapshot.State = Entry.State;
		Snapshot.RequestedOptions = Entry.Request.Options;
		Snapshot.AppliedOptions = Entry.AppliedOptions;
		Snapshot.RateResolution = Entry.RateResolution;
		FOpenMobileSensorsErrorMapper::ApplyRateAdjustmentText(
			Snapshot.RateResolution
		);
		Snapshot.AttitudeReference.RequestedReferenceFrame =
			Entry.Request.Options.AttitudeReferenceFrame;
		Snapshot.AttitudeReference.AppliedReferenceFrame =
			Entry.AppliedOptions.AttitudeReferenceFrame;
		if (Entry.Request.Sensor.Type == EOpenMobileSensorType::Attitude)
		{
			if (const FPhysicalStreamEntry* Physical =
				PhysicalStreams.Find(Entry.PhysicalKey))
			{
				Snapshot.AttitudeReference =
					Physical->Request.AttitudeReferenceState;
			}
			FOpenMobileSensorsSampleService::GetAttitudeRecenterState(
				Entry.OwnerIdentifier,
				Entry.Handle,
				Snapshot.Recenter
			);
		}
		Snapshot.Error = Entry.Error;
		Snapshot.Failure = Entry.Failure;
		return Snapshot;
	}

	void CancelFlushTimeoutTick()
	{
		if (FlushTimeoutTickHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(FlushTimeoutTickHandle);
			FlushTimeoutTickHandle.Reset();
		}
	}

	void CompleteFlush(
		const FGuid& RequestId,
		FOpenMobileSensorOperationResult Operation
	)
	{
		FPendingFlush Pending;
		if (!PendingFlushes.RemoveAndCopyValue(RequestId, Pending))
		{
			return;
		}
		FOpenMobileSensorFlushResult Result;
		Result.RequestId = RequestId;
		Result.Handle = Pending.Handle;
		Result.Operation = MoveTemp(Operation);
		if (Result.Operation.IsSuccess()
			&& !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
				Pending.BackendToken
			))
		{
			Result.Operation = FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::Cancelled
			);
		}
		if (Result.Operation.IsSuccess()
			&& !FOpenMobileSensorsSampleService::FlushPluginSamples(
				Pending.OwnerIdentifier,
				Pending.Handle,
				Result.FlushedSamples
			))
		{
			Result.Operation = FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::StaleHandle
			);
			Result.FlushedSamples = 0;
		}
		if (PendingFlushes.IsEmpty())
		{
			CancelFlushTimeoutTick();
		}
		if (Pending.Completion)
		{
			Pending.Completion(Result);
		}
	}

	void ProcessFlushTimeouts(double NowSeconds)
	{
		TArray<FGuid> TimedOutRequests;
		for (const TPair<FGuid, FPendingFlush>& Pair : PendingFlushes)
		{
			if (NowSeconds >= Pair.Value.DeadlineSeconds)
			{
				TimedOutRequests.Add(Pair.Key);
			}
		}
		for (const FGuid& RequestId : TimedOutRequests)
		{
			CompleteFlush(
				RequestId,
				FOpenMobileSensorsErrorMapper::Map(
					EOpenMobileSensorFailureReason::OperationalFailure,
					TEXT("OpenMobileSensors"),
					TEXT("FlushTimeout")
				)
			);
		}
	}

	bool TickFlushTimeouts(float DeltaSeconds)
	{
		static_cast<void>(DeltaSeconds);
		FlushTimeoutTickHandle.Reset();
		ProcessFlushTimeouts(FPlatformTime::Seconds());
		if (!PendingFlushes.IsEmpty())
		{
			FlushTimeoutTickHandle =
				FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateStatic(&TickFlushTimeouts),
					0.1f
				);
		}
		return false;
	}

	void EnsureFlushTimeoutTick()
	{
		if (!FlushTimeoutTickHandle.IsValid())
		{
			FlushTimeoutTickHandle =
				FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateStatic(&TickFlushTimeouts),
					0.1f
				);
		}
	}

	void CancelFlushesForPhysicalStream(
		const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle
	)
	{
		TArray<FGuid> RequestIds;
		for (const TPair<FGuid, FPendingFlush>& Pair : PendingFlushes)
		{
			if (Pair.Value.PhysicalStreamHandle == PhysicalStreamHandle)
			{
				RequestIds.Add(Pair.Key);
			}
		}
		for (const FGuid& RequestId : RequestIds)
		{
			CompleteFlush(
				RequestId,
				FOpenMobileSensorsErrorMapper::Map(
					EOpenMobileSensorFailureReason::Cancelled
				)
			);
		}
	}

	void CancelFlushesForHandle(
		const FOpenMobileSensorSubscriptionHandle& Handle
	)
	{
		TArray<FGuid> RequestIds;
		for (const TPair<FGuid, FPendingFlush>& Pair : PendingFlushes)
		{
			if (Pair.Value.Handle == Handle)
			{
				RequestIds.Add(Pair.Key);
			}
		}
		for (const FGuid& RequestId : RequestIds)
		{
			CompleteFlush(
				RequestId,
				FOpenMobileSensorsErrorMapper::Map(
					EOpenMobileSensorFailureReason::Cancelled
				)
			);
		}
	}

	void CancelAllFlushes()
	{
		TArray<FGuid> RequestIds;
		PendingFlushes.GetKeys(RequestIds);
		for (const FGuid& RequestId : RequestIds)
		{
			CompleteFlush(
				RequestId,
				FOpenMobileSensorsErrorMapper::Map(
					EOpenMobileSensorFailureReason::Cancelled
				)
			);
		}
		CancelFlushTimeoutTick();
	}

	bool SupportsNativeBatching(
		const FOpenMobileSensorIdentifier& Sensor
	)
	{
		const FOpenMobileSensorCapabilitySnapshot Snapshot =
			FOpenMobileSensorsCapabilityService::GetSnapshot();
		const FOpenMobileSensorCapability* Capability =
			Snapshot.Sensors.FindByPredicate(
				[&Sensor](const FOpenMobileSensorCapability& Candidate)
				{
					return Candidate.Sensor.Type == Sensor.Type
						&& (Sensor.InstanceId.IsNone()
							|| Candidate.Sensor.InstanceId ==
								Sensor.InstanceId);
				}
			);
		return Capability && Capability->bSupportsNativeBatching;
	}

	EOpenMobileSensorBatchingMode GetBatchingMode(
		const FSubscriptionEntry& Entry
	)
	{
		if (Entry.AppliedOptions.bLowLatency
			|| Entry.AppliedOptions.MaximumDeliveryLatencySeconds <= 0.0)
		{
			return EOpenMobileSensorBatchingMode::Disabled;
		}
		const FPhysicalStreamEntry* Physical =
			PhysicalStreams.Find(Entry.PhysicalKey);
		if (Physical && Physical->Request.bNativeBatchingApplied)
		{
			return EOpenMobileSensorBatchingMode::Native;
		}
		if (Entry.AppliedOptions.DeliveryMode ==
				EOpenMobileSensorDeliveryMode::Buffered
			|| Entry.AppliedOptions.DeliveryMode ==
				EOpenMobileSensorDeliveryMode::EventBatches)
		{
			return EOpenMobileSensorBatchingMode::Plugin;
		}
		return EOpenMobileSensorBatchingMode::Unavailable;
	}

	void BroadcastState(const FSubscriptionEntry& Entry)
	{
		const FGuid OwnerIdentifier = Entry.OwnerIdentifier;
		const FOpenMobileSensorSubscriptionStateSnapshot Snapshot =
			MakeSnapshot(Entry);
		StateChangedEvent.Broadcast(OwnerIdentifier, Snapshot);
	}

	void SetState(
		const FGuid& Identifier,
		EOpenMobileSensorSubscriptionState State,
		const FOpenMobileError& Error = {},
		const FOpenMobileSensorFailureDetails& Failure = {}
	)
	{
		FSubscriptionEntry* Entry = Subscriptions.Find(Identifier);
		if (!Entry)
		{
			return;
		}
		const FOpenMobileSensorSubscriptionHandle EntryHandle = Entry->Handle;
		Entry->State = State;
		Entry->Error = Error;
		Entry->Failure = Failure;
		if (Error.IsSet() || Failure.IsSet())
		{
			FOpenMobileSensorOperationResult Operation = Failure.IsSet()
				? FOpenMobileSensorsErrorMapper::Map(
					Failure.Reason,
					Failure.NativeDomain,
					Failure.NativeCode
				)
				: FOpenMobileSensorsErrorMapper::FromCommon(Error);
			if (Error.Code != EOpenMobileErrorCode::None)
			{
				Operation.Error.Code = Error.Code;
			}
			FOpenMobileSensorErrorContext Context;
			Context.Sensor = Entry->Request.Sensor;
			Context.SubscriptionIdentifier = Entry->Handle.GetIdentifier();
			Context.BackendName = GetBackendName(Entry);
			Context.bHasRateContext = true;
			Context.RequestedFrequencyHz =
				Entry->RateResolution.RequestedFrequencyHz;
			Context.AppliedFrequencyHz =
				Entry->RateResolution.AppliedNativeFrequencyHz;
			switch (Operation.Failure.Reason)
			{
			case EOpenMobileSensorFailureReason::PermissionRequired:
			case EOpenMobileSensorFailureReason::PermissionDenied:
			case EOpenMobileSensorFailureReason::PermissionRestricted:
				Context.Operation = EOpenMobileSensorOperation::Permission;
				break;
			case EOpenMobileSensorFailureReason::PoorCalibration:
				Context.Operation = EOpenMobileSensorOperation::Calibration;
				break;
			case EOpenMobileSensorFailureReason::BackgroundRestricted:
				Context.Operation = EOpenMobileSensorOperation::Lifecycle;
				break;
			default:
				Context.Operation = EOpenMobileSensorOperation::BackendCallback;
				break;
			}
			RecordOperationFailure(
				Entry->OwnerIdentifier,
				Operation,
				Context
			);
		}
		FOpenMobileSensorsSampleService::SetSubscriptionState(
			Entry->Handle,
			State
		);
		BroadcastState(*Entry);
		if (State == EOpenMobileSensorSubscriptionState::Paused
			|| State == EOpenMobileSensorSubscriptionState::Stopping
			|| State == EOpenMobileSensorSubscriptionState::Stopped
			|| State == EOpenMobileSensorSubscriptionState::Failed)
		{
			CancelFlushesForHandle(EntryHandle);
		}
	}

	bool BuildPhysicalRequest(
		const FPhysicalStreamKey& Key,
		FOpenMobileSensorPhysicalStreamRequest& OutRequest
	)
	{
		bool bFound = false;
		OutRequest = {};
		OutRequest.Sensor = Key.Sensor;
		OutRequest.AttitudeReferenceFrame = Key.AttitudeReferenceFrame;
		OutRequest.AttitudeReferenceState.RequestedReferenceFrame =
			Key.AttitudeReferenceFrame;
		OutRequest.AttitudeReferenceState.AppliedReferenceFrame =
			Key.AttitudeReferenceFrame;
		OutRequest.bAllowDerivedFallback = false;
		for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
		{
			const FSubscriptionEntry& Entry = Pair.Value;
			if (!(Entry.PhysicalKey == Key)
				|| (Entry.State != EOpenMobileSensorSubscriptionState::Accepted
					&& Entry.State !=
						EOpenMobileSensorSubscriptionState::Starting
					&& Entry.State !=
						EOpenMobileSensorSubscriptionState::Active))
			{
				continue;
			}
			if (!bFound)
			{
				OutRequest.MaximumDeliveryLatencySeconds =
					Entry.AppliedOptions.MaximumDeliveryLatencySeconds;
				bFound = true;
			}
			else
			{
				OutRequest.MaximumDeliveryLatencySeconds = FMath::Min(
					OutRequest.MaximumDeliveryLatencySeconds,
					Entry.AppliedOptions.MaximumDeliveryLatencySeconds
				);
			}
			OutRequest.RequestedFrequencyHz = FMath::Max(
				OutRequest.RequestedFrequencyHz,
				Entry.AppliedOptions.CustomFrequencyHz
			);
			OutRequest.bAllowHighSamplingRate |=
				Entry.AppliedOptions.bAllowHighSamplingRate;
			OutRequest.bLowLatency |= Entry.AppliedOptions.bLowLatency;
		}
		if (bFound && (OutRequest.bLowLatency
			|| OutRequest.MaximumDeliveryLatencySeconds <= 0.0))
		{
			OutRequest.MaximumDeliveryLatencySeconds = 0.0;
		}
		OutRequest.bNativeBatchingRequested = bFound
			&& OutRequest.MaximumDeliveryLatencySeconds > 0.0
			&& SupportsNativeBatching(Key.Sensor);
		return bFound;
	}

	void UpdateAppliedNativeRate(
		const FPhysicalStreamKey& Key,
		const FOpenMobileSensorPhysicalStreamRequest& AppliedRequest
	)
	{
		const double AppliedNativeFrequencyHz =
			AppliedRequest.RequestedFrequencyHz;
		if (!FMath::IsFinite(AppliedNativeFrequencyHz)
			|| AppliedNativeFrequencyHz <= 0.0)
		{
			return;
		}
		constexpr double RateToleranceHz = 1.e-9;
		for (TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
		{
			FSubscriptionEntry& Entry = Pair.Value;
			if (!(Entry.PhysicalKey == Key))
			{
				continue;
			}
			Entry.RateResolution.AppliedNativeFrequencyHz =
				AppliedNativeFrequencyHz;
			Entry.RateResolution.AdjustmentReason =
				Entry.CommonRateAdjustmentReason;
			if (AppliedNativeFrequencyHz + RateToleranceHz <
				Entry.RateResolution.ClampedFrequencyHz)
			{
				Entry.RateResolution.AdjustmentReason =
					AppliedRequest.AppliedRateAdjustmentReason !=
						EOpenMobileSensorRateAdjustmentReason::None
					? AppliedRequest.AppliedRateAdjustmentReason
					: EOpenMobileSensorRateAdjustmentReason::BackendLimit;
			}
			FOpenMobileSensorsErrorMapper::ApplyRateAdjustmentText(
				Entry.RateResolution
			);
		}
	}

	FOpenMobileError GetOperationError(
		const FOpenMobileSensorOperationResult& Operation
	)
	{
		return Operation.Error.IsSet()
			? Operation.Error
			: FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::OperationalFailure
			).Error;
	}

	void CancelPendingOperationsTick()
	{
		if (PendingOperationsTickHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(
				PendingOperationsTickHandle
			);
			PendingOperationsTickHandle.Reset();
		}
		PendingOperationsDeadlineSeconds = 0.0;
	}

	void ProcessPendingBackendOperations(double NowSeconds);

	bool TickPendingBackendOperations(float DeltaSeconds)
	{
		static_cast<void>(DeltaSeconds);
		PendingOperationsTickHandle.Reset();
		PendingOperationsDeadlineSeconds = 0.0;
		ProcessPendingBackendOperations(FPlatformTime::Seconds());
		return false;
	}

	void SchedulePendingBackendOperations(double DelaySeconds = 0.0)
	{
		const double ClampedDelay = FMath::Clamp(
			DelaySeconds,
			0.0,
			60.0
		);
		const double Deadline = FPlatformTime::Seconds() + ClampedDelay;
		if (PendingOperationsTickHandle.IsValid()
			&& Deadline + 0.000001 >= PendingOperationsDeadlineSeconds)
		{
			return;
		}
		CancelPendingOperationsTick();
		PendingOperationsDeadlineSeconds = Deadline;
		PendingOperationsTickHandle =
			FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateStatic(
					&TickPendingBackendOperations
				),
				static_cast<float>(ClampedDelay)
			);
	}

	bool IsRecoverableNativeFailure(
		const FOpenMobileSensorOperationResult& Failure
	)
	{
		return Failure.Failure.Reason ==
				EOpenMobileSensorFailureReason::TemporarilyUnavailable
			|| Failure.Failure.Reason ==
				EOpenMobileSensorFailureReason::OperationalFailure;
	}

	void RecoverOrFailSubscriptions(
		const TArray<FGuid>& Identifiers,
		const FOpenMobileSensorOperationResult& Failure,
		double NowSeconds
	)
	{
		constexpr uint8 MaximumRecoveryAttempts = 2;
		constexpr double InitialRecoveryDelaySeconds = 0.1;
		double EarliestRetrySeconds = TNumericLimits<double>::Max();
		for (const FGuid& Identifier : Identifiers)
		{
			FSubscriptionEntry* Entry = Subscriptions.Find(Identifier);
			if (!Entry)
			{
				continue;
			}
			if (bShuttingDown
				|| !IsRecoverableNativeFailure(Failure)
				|| Entry->NativeRecoveryAttemptCount >=
					MaximumRecoveryAttempts)
			{
				SetState(
					Identifier,
					EOpenMobileSensorSubscriptionState::Failed,
					GetOperationError(Failure),
					Failure.Failure
				);
				continue;
			}
			++Entry->NativeRecoveryAttemptCount;
			const double DelaySeconds = InitialRecoveryDelaySeconds
				* static_cast<double>(
					1u << (Entry->NativeRecoveryAttemptCount - 1)
				);
			Entry->NextRecoveryAttemptSeconds = NowSeconds + DelaySeconds;
			EarliestRetrySeconds = FMath::Min(
				EarliestRetrySeconds,
				Entry->NextRecoveryAttemptSeconds
			);
			SetState(
				Identifier,
				EOpenMobileSensorSubscriptionState::Accepted,
				GetOperationError(Failure),
				Failure.Failure
			);
		}
		if (EarliestRetrySeconds < TNumericLimits<double>::Max())
		{
			SchedulePendingBackendOperations(
				FMath::Max(0.0, EarliestRetrySeconds - NowSeconds)
			);
		}
	}

	void ReconcilePhysicalStream(const FPhysicalStreamKey& Key)
	{
		FPhysicalStreamEntry* Physical = PhysicalStreams.Find(Key);
		if (!Physical)
		{
			return;
		}
		FOpenMobileSensorPhysicalStreamRequest DesiredRequest;
		if (!BuildPhysicalRequest(Key, DesiredRequest))
		{
			if (Physical->Backend
				&& FOpenMobileSensorsBackendRegistry::IsBackendRegistered(
					Physical->Backend
				))
			{
				Physical->Backend->StopSensorStream(Physical->Handle);
			}
			PhysicalStreams.Remove(Key);
			return;
		}
		if (DesiredRequest == Physical->Request
			|| !Physical->Backend
			|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
				Physical->BackendToken
			))
		{
			return;
		}
		FOpenMobileSensorPhysicalStreamRequest AppliedRequest = DesiredRequest;
		const FOpenMobileSensorOperationResult Result =
			Physical->Backend->ReconfigureSensorStream(
				Physical->Handle,
				AppliedRequest
			);
		if (Result.IsSuccess())
		{
			Physical->Request = MoveTemp(AppliedRequest);
			UpdateAppliedNativeRate(
				Key,
				Physical->Request
			);
		}
	}

	void ProcessPendingKey(
		const FPhysicalStreamKey& Key,
		double NowSeconds
	)
	{
		TArray<FGuid> PendingIdentifiers;
		for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
		{
			if (Pair.Value.State ==
					EOpenMobileSensorSubscriptionState::Accepted
				&& Pair.Value.PhysicalKey == Key
				&& Pair.Value.NextRecoveryAttemptSeconds <= NowSeconds)
			{
				PendingIdentifiers.Add(Pair.Key);
			}
		}
		for (const FGuid& Identifier : PendingIdentifiers)
		{
			if (FSubscriptionEntry* Entry = Subscriptions.Find(Identifier))
			{
				Entry->NextRecoveryAttemptSeconds = 0.0;
			}
			SetState(
				Identifier,
				EOpenMobileSensorSubscriptionState::Starting
			);
		}

		TArray<FGuid> StartingIdentifiers;
		for (const FGuid& Identifier : PendingIdentifiers)
		{
			const FSubscriptionEntry* Entry = Subscriptions.Find(Identifier);
			if (Entry
				&& Entry->State ==
					EOpenMobileSensorSubscriptionState::Starting
				&& Entry->PhysicalKey == Key)
			{
				StartingIdentifiers.Add(Identifier);
			}
		}
		if (StartingIdentifiers.IsEmpty())
		{
			ReconcilePhysicalStream(Key);
			return;
		}

		FOpenMobileSensorPhysicalStreamRequest DesiredRequest;
		if (!BuildPhysicalRequest(Key, DesiredRequest))
		{
			return;
		}
		const FSubscriptionEntry* FirstEntry =
			Subscriptions.Find(StartingIdentifiers[0]);
		const FOpenMobileSensorsBackendToken PendingBackendToken =
			FirstEntry ? FirstEntry->BackendToken :
				FOpenMobileSensorsBackendToken{};
		IOpenMobileSensorsBackend* Backend =
			FOpenMobileSensorsBackendRegistry::FindBackend();
		FOpenMobileSensorOperationResult Operation;
		Operation.Code = EOpenMobileSensorResultCode::Failed;
		FPhysicalStreamEntry* ExistingPhysical = PhysicalStreams.Find(Key);
		const bool bStartingNewPhysical = ExistingPhysical == nullptr;
		const FOpenMobileSensorPhysicalStreamRequest DemandBeforeBackendCall =
			DesiredRequest;
		FOpenMobileSensorBackendStreamHandle NewPhysicalHandle;
		if (!FirstEntry
			|| !Backend
			|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
				PendingBackendToken
			))
		{
			Operation = FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::TemporarilyUnavailable
			);
		}
		else if (ExistingPhysical)
		{
			if (DesiredRequest == ExistingPhysical->Request)
			{
				Operation = MakeSuccess();
			}
			else
			{
				const FOpenMobileSensorBackendStreamHandle ExistingHandle =
					ExistingPhysical->Handle;
				CancelFlushesForPhysicalStream(ExistingHandle);
				ExistingPhysical = PhysicalStreams.Find(Key);
				if (ExistingPhysical)
				{
					Operation = Backend->ReconfigureSensorStream(
						ExistingPhysical->Handle,
						DesiredRequest
					);
				}
			}
		}
		else
		{
			NewPhysicalHandle.Identifier = FGuid::NewGuid();
			Operation = Backend->StartSensorStream(
				NewPhysicalHandle,
				DesiredRequest
			);
		}

		if (Operation.IsSuccess())
		{
			if (bShuttingDown)
			{
				if (bStartingNewPhysical
					&& Backend
					&& FOpenMobileSensorsBackendRegistry::IsBackendRegistered(
						Backend
					))
				{
					Backend->StopSensorStream(NewPhysicalHandle);
				}
				return;
			}
			FOpenMobileSensorPhysicalStreamRequest CurrentDemand;
			if (!BuildPhysicalRequest(Key, CurrentDemand))
			{
				if (bStartingNewPhysical)
				{
					if (FOpenMobileSensorsBackendRegistry::
						IsBackendRegistered(Backend))
					{
						Backend->StopSensorStream(NewPhysicalHandle);
					}
				}
				else
				{
					ReconcilePhysicalStream(Key);
				}
				return;
			}
			const bool bDemandChangedDuringBackendCall =
				CurrentDemand != DemandBeforeBackendCall;
			FPhysicalStreamEntry* ActivePhysical =
				PhysicalStreams.Find(Key);
			if (bStartingNewPhysical)
			{
				FPhysicalStreamEntry Physical;
				Physical.Handle = NewPhysicalHandle;
				Physical.Key = Key;
				Physical.Request = DesiredRequest;
				Physical.BackendToken = PendingBackendToken;
				Physical.Backend = Backend;
				PhysicalStreams.Add(Key, MoveTemp(Physical));
				ActivePhysical = PhysicalStreams.Find(Key);
			}
			else if (ActivePhysical)
			{
				ActivePhysical->Request = DesiredRequest;
			}
			if (!ActivePhysical)
			{
				return;
			}
			const FOpenMobileSensorBackendStreamHandle ActivePhysicalHandle =
				ActivePhysical->Handle;
			UpdateAppliedNativeRate(
				Key,
				ActivePhysical->Request
			);
			for (const FGuid& Identifier : StartingIdentifiers)
			{
				const FSubscriptionEntry* Entry =
					Subscriptions.Find(Identifier);
				if (!Entry
					|| Entry->State !=
						EOpenMobileSensorSubscriptionState::Starting)
				{
					continue;
				}
				FOpenMobileSensorsSampleService::SetPhysicalStreamHandle(
					Entry->Handle,
					ActivePhysicalHandle
				);
				SetState(
					Identifier,
					EOpenMobileSensorSubscriptionState::Active
				);
			}
			if (bDemandChangedDuringBackendCall)
			{
				ReconcilePhysicalStream(Key);
			}
			return;
		}

		RecoverOrFailSubscriptions(
			StartingIdentifiers,
			Operation,
			NowSeconds
		);
	}

	void ProcessPendingBackendOperations(double NowSeconds)
	{
		CancelPendingOperationsTick();
		if (bShuttingDown)
		{
			return;
		}
		TSet<FPhysicalStreamKey> PendingKeys;
		for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
		{
			if (Pair.Value.State ==
					EOpenMobileSensorSubscriptionState::Accepted
				&& Pair.Value.NextRecoveryAttemptSeconds <= NowSeconds)
			{
				PendingKeys.Add(Pair.Value.PhysicalKey);
			}
		}
		for (const FPhysicalStreamKey& Key : PendingKeys)
		{
			ProcessPendingKey(Key, NowSeconds);
		}
		double EarliestRetrySeconds = TNumericLimits<double>::Max();
		for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
		{
			if (Pair.Value.State ==
					EOpenMobileSensorSubscriptionState::Accepted
				&& Pair.Value.NextRecoveryAttemptSeconds > NowSeconds)
			{
				EarliestRetrySeconds = FMath::Min(
					EarliestRetrySeconds,
					Pair.Value.NextRecoveryAttemptSeconds
				);
			}
		}
		if (EarliestRetrySeconds < TNumericLimits<double>::Max())
		{
			SchedulePendingBackendOperations(
				EarliestRetrySeconds - NowSeconds
			);
		}
	}

	void StopPhysicalStreams()
	{
		for (const TPair<FPhysicalStreamKey, FPhysicalStreamEntry>& Pair
			: PhysicalStreams)
		{
			const FPhysicalStreamEntry& Physical = Pair.Value;
			if (Physical.Backend
				&& FOpenMobileSensorsBackendRegistry::IsBackendRegistered(
					Physical.Backend
				))
			{
				Physical.Backend->StopSensorStream(Physical.Handle);
			}
		}
		PhysicalStreams.Reset();
	}

	bool IsApplicationReadyForForegroundStreams()
	{
		return bApplicationActive && bApplicationInForeground;
	}

	enum class ELifecycleAction : uint8
	{
		Pause,
		Continue,
		Stop
	};

	ELifecycleAction ResolveLifecycleAction(
		const FOpenMobileSensorIdentifier& Sensor,
		const FOpenMobileSensorStreamOptions& Options,
		bool bBackgroundDeliveryAllowed,
		const FOpenMobileSensorCapabilitySnapshot& Snapshot
	)
	{
		switch (Options.LifecyclePolicy)
		{
		case EOpenMobileSensorLifecyclePolicy::StopInBackground:
			return ELifecycleAction::Stop;
		case EOpenMobileSensorLifecyclePolicy::SuspendInBackground:
			return ELifecycleAction::Pause;
		case EOpenMobileSensorLifecyclePolicy::ContinueWhenSupported:
			break;
		}
		if (!bBackgroundDeliveryAllowed)
		{
			return ELifecycleAction::Pause;
		}
		const FOpenMobileSensorCapability* Capability =
			Snapshot.Sensors.FindByPredicate(
				[&Sensor](const FOpenMobileSensorCapability& Candidate)
				{
					return Candidate.Sensor.Type == Sensor.Type
						&& (Sensor.InstanceId.IsNone()
							|| Candidate.Sensor.InstanceId ==
								Sensor.InstanceId);
				}
			);
		return Capability
			&& (Capability->BackgroundSupport ==
					EOpenMobileSensorBackgroundSupport::EventDriven
				|| Capability->BackgroundSupport ==
					EOpenMobileSensorBackgroundSupport::Supported)
			? ELifecycleAction::Continue
			: ELifecycleAction::Pause;
	}

	void StopSubscriptionForLifecycle(
		const FGuid& Identifier,
		const FOpenMobileSensorOperationResult& Restriction
	)
	{
		FSubscriptionEntry* Entry = Subscriptions.Find(Identifier);
		if (!Entry)
		{
			return;
		}
		const FSubscriptionEntry StoppedEntry = *Entry;
		SetState(
			Identifier,
			EOpenMobileSensorSubscriptionState::Stopping,
			Restriction.Error,
			Restriction.Failure
		);
		Entry = Subscriptions.Find(Identifier);
		if (!Entry)
		{
			return;
		}
		Subscriptions.Remove(Identifier);
		FOpenMobileSensorsSampleService::UnregisterSubscription(
			StoppedEntry.Handle
		);
		FSubscriptionEntry FinalEntry = StoppedEntry;
		FinalEntry.State = EOpenMobileSensorSubscriptionState::Stopped;
		FinalEntry.Error = Restriction.Error;
		FinalEntry.Failure = Restriction.Failure;
		BroadcastState(FinalEntry);
	}

	void ApplyApplicationLifecycleState()
	{
		if (bShuttingDown)
		{
			return;
		}
		if (!IsApplicationReadyForForegroundStreams())
		{
			CancelPendingOperationsTick();
			const UOpenMobileSensorsSettings* Settings =
				GetDefault<UOpenMobileSensorsSettings>();
			const bool bBackgroundDeliveryAllowed = Settings
				&& Settings->bAllowBackgroundSensorDelivery;
			const FOpenMobileSensorCapabilitySnapshot CapabilitySnapshot =
				FOpenMobileSensorsCapabilityService::GetSnapshot();
			TArray<FGuid> PausedIdentifiers;
			TArray<FGuid> ResumedIdentifiers;
			TArray<FGuid> StoppedIdentifiers;
			TSet<FPhysicalStreamKey> Keys;
			for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
			{
				if (Pair.Value.State ==
						EOpenMobileSensorSubscriptionState::Accepted
					|| Pair.Value.State ==
						EOpenMobileSensorSubscriptionState::Starting
					|| Pair.Value.State ==
						EOpenMobileSensorSubscriptionState::Active)
				{
					const ELifecycleAction Action = ResolveLifecycleAction(
						Pair.Value.Request.Sensor,
						Pair.Value.AppliedOptions,
						bBackgroundDeliveryAllowed,
						CapabilitySnapshot
					);
					if (Action != ELifecycleAction::Continue)
					{
						Keys.Add(Pair.Value.PhysicalKey);
					}
					if (Action == ELifecycleAction::Pause)
					{
						PausedIdentifiers.Add(Pair.Key);
					}
					else if (Action == ELifecycleAction::Stop)
					{
						StoppedIdentifiers.Add(Pair.Key);
					}
				}
				else if (Pair.Value.State ==
						EOpenMobileSensorSubscriptionState::Paused
					&& Pair.Value.bPausedForLifecycle)
				{
					const ELifecycleAction Action = ResolveLifecycleAction(
						Pair.Value.Request.Sensor,
						Pair.Value.AppliedOptions,
						bBackgroundDeliveryAllowed,
						CapabilitySnapshot
					);
					if (Action == ELifecycleAction::Continue)
					{
						ResumedIdentifiers.Add(Pair.Key);
					}
					else if (Action == ELifecycleAction::Stop)
					{
						StoppedIdentifiers.Add(Pair.Key);
						Keys.Add(Pair.Value.PhysicalKey);
					}
				}
			}
			const FOpenMobileSensorOperationResult Restriction =
				FOpenMobileSensorsErrorMapper::Map(
					EOpenMobileSensorFailureReason::BackgroundRestricted
				);
			for (const FGuid& Identifier : PausedIdentifiers)
			{
				FSubscriptionEntry* Entry = Subscriptions.Find(Identifier);
				if (!Entry)
				{
					continue;
				}
				Entry->bPausedForLifecycle = true;
				SetState(
					Identifier,
					EOpenMobileSensorSubscriptionState::Paused,
					Restriction.Error,
					Restriction.Failure
				);
			}
			for (const FGuid& Identifier : StoppedIdentifiers)
			{
				StopSubscriptionForLifecycle(Identifier, Restriction);
			}
			bool bNeedsBackendWork = false;
			for (const FGuid& Identifier : ResumedIdentifiers)
			{
				FSubscriptionEntry* Entry = Subscriptions.Find(Identifier);
				if (!Entry || !Entry->bPausedForLifecycle)
				{
					continue;
				}
				Entry->bPausedForLifecycle = false;
				SetState(
					Identifier,
					EOpenMobileSensorSubscriptionState::Accepted
				);
				bNeedsBackendWork = true;
			}
			for (const FPhysicalStreamKey& Key : Keys)
			{
				ReconcilePhysicalStream(Key);
			}
			if (bNeedsBackendWork)
			{
				SchedulePendingBackendOperations();
			}
			return;
		}

		bool bNeedsBackendWork = false;
		TArray<FGuid> Identifiers;
		for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
		{
			if (Pair.Value.State == EOpenMobileSensorSubscriptionState::Paused
				&& Pair.Value.bPausedForLifecycle)
			{
				Identifiers.Add(Pair.Key);
			}
		}
		for (const FGuid& Identifier : Identifiers)
		{
			FSubscriptionEntry* Entry = Subscriptions.Find(Identifier);
			if (!Entry || !Entry->bPausedForLifecycle)
			{
				continue;
			}
			Entry->bPausedForLifecycle = false;
			SetState(
				Identifier,
				EOpenMobileSensorSubscriptionState::Accepted
			);
			bNeedsBackendWork = true;
		}
		if (bNeedsBackendWork)
		{
			SchedulePendingBackendOperations();
		}
	}

	FOpenMobileSensorOperationResult CompleteSubscriptionUpdate()
	{
		ApplyApplicationLifecycleState();
		return MakeSuccess();
	}

	void HandleApplicationWillDeactivate()
	{
		bApplicationActive = false;
		ApplyApplicationLifecycleState();
	}

	void HandleApplicationHasReactivated()
	{
		bApplicationActive = true;
		ApplyApplicationLifecycleState();
	}

	void HandleApplicationWillEnterBackground()
	{
		bApplicationInForeground = false;
		ApplyApplicationLifecycleState();
	}

	void HandleApplicationHasEnteredForeground()
	{
		bApplicationInForeground = true;
		ApplyApplicationLifecycleState();
	}

	void RemoveLifecycleDelegates()
	{
		if (DeactivatedHandle.IsValid())
		{
			FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(
				DeactivatedHandle
			);
			DeactivatedHandle.Reset();
		}
		if (ReactivatedHandle.IsValid())
		{
			FCoreDelegates::ApplicationHasReactivatedDelegate.Remove(
				ReactivatedHandle
			);
			ReactivatedHandle.Reset();
		}
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
	}

	void RegisterLifecycleDelegates()
	{
		RemoveLifecycleDelegates();
		DeactivatedHandle =
			FCoreDelegates::ApplicationWillDeactivateDelegate.AddStatic(
				&HandleApplicationWillDeactivate
			);
		ReactivatedHandle =
			FCoreDelegates::ApplicationHasReactivatedDelegate.AddStatic(
				&HandleApplicationHasReactivated
			);
		BackgroundHandle =
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddStatic(
				&HandleApplicationWillEnterBackground
			);
		ForegroundHandle =
			FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddStatic(
				&HandleApplicationHasEnteredForeground
			);
	}
}

void FOpenMobileSensorsSubscriptionService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	CancelPendingOperationsTick();
	CancelAllFlushes();
	bShuttingDown = false;
	bApplicationActive = true;
	bApplicationInForeground = true;
	Subscriptions.Reset();
	PhysicalStreams.Reset();
	RecentErrors.Reset();
	FOpenMobileSensorsSampleService::UnregisterAll();
	RegisterLifecycleDelegates();
}

bool FOpenMobileSensorsSubscriptionService::PreviewOptions(
	const FOpenMobileSensorIdentifier& Sensor,
	const FOpenMobileSensorStreamOptions& Requested,
	FOpenMobileSensorStreamOptions& OutApplied,
	FOpenMobileSensorRateResolution& OutRateResolution
)
{
	return OpenMobileSensorsSubscriptionServicePrivate::ValidateAndResolveOptions(
		Sensor,
		Requested,
		OutApplied,
		OutRateResolution
	);
}

void FOpenMobileSensorsSubscriptionService::BeginShutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	if (bShuttingDown)
	{
		return;
	}
	bShuttingDown = true;
	RemoveLifecycleDelegates();
	CancelPendingOperationsTick();
	CancelAllFlushes();
	StopPhysicalStreams();
	Subscriptions.Reset();
	RecentErrors.Reset();
	FOpenMobileSensorsSampleService::UnregisterAll();
	StateChangedEvent.Clear();
}

void FOpenMobileSensorsSubscriptionService::HandleBackendGenerationChanged()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	CancelPendingOperationsTick();
	CancelAllFlushes();
	StopPhysicalStreams();
	Subscriptions.Reset();
	FOpenMobileSensorsSampleService::UnregisterAll();
}

FOpenMobileSensorSubscriptionResult
FOpenMobileSensorsSubscriptionService::StartSubscription(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionRequest& Request
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	FOpenMobileSensorSubscriptionResult Result;
	Result.RequestedOptions = Request.Options;
	Result.RateResolution.RequestedFrequencyHz =
		Request.Options.CustomFrequencyHz;
	ON_SCOPE_EXIT
	{
		FOpenMobileSensorsErrorMapper::ApplyRateAdjustmentText(
			Result.RateResolution
		);
		if (!Result.Operation.IsSuccess() && OwnerIdentifier.IsValid())
		{
			FOpenMobileSensorErrorContext Context;
			Context.Sensor = Request.Sensor;
			Context.Operation = EOpenMobileSensorOperation::StartStream;
			Context.SubscriptionIdentifier = Result.Handle.GetIdentifier();
			Context.BackendName = GetBackendName();
			Context.bHasRateContext = true;
			Context.RequestedFrequencyHz =
				Result.RateResolution.RequestedFrequencyHz;
			Context.AppliedFrequencyHz =
				Result.RateResolution.AppliedNativeFrequencyHz;
			RecordOperationFailure(
				OwnerIdentifier,
				Result.Operation,
				Context
			);
		}
	};
	if (!OwnerIdentifier.IsValid())
	{
		Result.AppliedOptions = Request.Options;
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
		return Result;
	}
	if (bShuttingDown
		|| FOpenMobileSensorsBackendRegistry::IsShuttingDown())
	{
		Result.AppliedOptions = Request.Options;
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
		return Result;
	}
	if (Request.bResettableStepCountSession
		&& Request.Sensor.Type != EOpenMobileSensorType::StepCounter)
	{
		Result.AppliedOptions = Request.Options;
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidRequest
		);
		return Result;
	}
	if (!IsFiniteInRange(Request.Options.CustomFrequencyHz, 1.0, 1000.0))
	{
		Result.AppliedOptions = Request.Options;
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidFrequency
		);
		return Result;
	}
	FOpenMobileSensorIdentifier PhysicalSensor;
	EOpenMobileSensorFailureReason ResolutionFailure;
	if (!ResolvePhysicalSensor(
		OwnerIdentifier,
		Request.Sensor,
		Request.Options,
		PhysicalSensor,
		ResolutionFailure
	))
	{
		Result.AppliedOptions = Request.Options;
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			ResolutionFailure
		);
		return Result;
	}
	FOpenMobileSensorStreamOptions AppliedOptions;
	FOpenMobileSensorRateResolution RateResolution;
	if (!ValidateAndResolveOptions(
		PhysicalSensor,
		Request.Options,
		AppliedOptions,
		RateResolution
	))
	{
		Result.AppliedOptions = Request.Options;
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidRequest
		);
		return Result;
	}
	Result.AppliedOptions = AppliedOptions;
	Result.RateResolution = RateResolution;
	if (AppliedOptions.CoordinateSpace ==
			EOpenMobileSensorCoordinateSpace::CurrentScreen
		&& !FOpenMobileSensorsScreenRotationService::HasRotationSource(
			OwnerIdentifier))
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::ConfigurationBlocked);
		return Result;
	}
	ELifecycleAction InactiveLifecycleAction = ELifecycleAction::Continue;
	if (!IsApplicationReadyForForegroundStreams())
	{
		const UOpenMobileSensorsSettings* Settings =
			GetDefault<UOpenMobileSensorsSettings>();
		const FOpenMobileSensorCapabilitySnapshot CapabilitySnapshot =
			FOpenMobileSensorsCapabilityService::GetSnapshot();
		InactiveLifecycleAction = ResolveLifecycleAction(
			Request.Sensor,
			AppliedOptions,
			Settings && Settings->bAllowBackgroundSensorDelivery,
			CapabilitySnapshot
		);
	}
	if (InactiveLifecycleAction == ELifecycleAction::Stop)
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::BackgroundRestricted
		);
		return Result;
	}
	const FOpenMobileSensorsBackendToken BackendToken =
		FOpenMobileSensorsBackendRegistry::CaptureToken();
	if (BackendToken.Generation == 0)
	{
		Result.Operation = FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedPlatform
		);
		return Result;
	}

	FOpenMobileSensorSubscriptionHandle Handle;
	do
	{
		Handle.Identifier = FGuid::NewGuid();
	}
	while (!Handle.Identifier.IsValid()
		|| Subscriptions.Contains(Handle.Identifier));
	Handle.Generation = AllocateGeneration();

	FSubscriptionEntry Entry;
	Entry.OwnerIdentifier = OwnerIdentifier;
	Entry.Handle = Handle;
	Entry.Request = Request;
	Entry.AppliedOptions = AppliedOptions;
	Entry.RateResolution = RateResolution;
	Entry.CommonRateAdjustmentReason = RateResolution.AdjustmentReason;
	Entry.PhysicalKey = MakePhysicalKey(PhysicalSensor, AppliedOptions);
	Entry.BackendToken = BackendToken;
	Entry.bResettableStepCountSession =
		Request.bResettableStepCountSession;
	Subscriptions.Add(Handle.Identifier, MoveTemp(Entry));
	FOpenMobileSensorsSampleService::RegisterSubscription(
		OwnerIdentifier,
		Handle,
		Request.Sensor,
		PhysicalSensor,
		AppliedOptions,
		Request.bResettableStepCountSession,
		BackendToken.Generation
	);
	if (InactiveLifecycleAction == ELifecycleAction::Continue)
	{
		SchedulePendingBackendOperations();
	}
	else if (FSubscriptionEntry* StoredEntry =
		Subscriptions.Find(Handle.Identifier))
	{
		StoredEntry->bPausedForLifecycle = true;
		const FOpenMobileSensorOperationResult Restriction =
			FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::BackgroundRestricted
			);
		SetState(
			Handle.Identifier,
			EOpenMobileSensorSubscriptionState::Paused,
			Restriction.Error,
			Restriction.Failure
		);
	}

	Result.Handle = Handle;
	Result.Operation = MakeSuccess(EOpenMobileSensorResultCode::Accepted);
	return Result;
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsSubscriptionService::UpdateSubscription(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	const FOpenMobileSensorStreamOptions& Options
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	FSubscriptionEntry* Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		return MakeHandleFailure(Handle);
	}
	if (Entry->State == EOpenMobileSensorSubscriptionState::Starting
		|| Entry->State == EOpenMobileSensorSubscriptionState::Failed)
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
	}
	if (!IsFiniteInRange(Options.CustomFrequencyHz, 1.0, 1000.0))
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidFrequency
		);
	}
	FOpenMobileSensorIdentifier PhysicalSensor;
	EOpenMobileSensorFailureReason ResolutionFailure;
	if (Entry->State == EOpenMobileSensorSubscriptionState::Paused
		&& Entry->bPausedForLifecycle
		&& !IsApplicationReadyForForegroundStreams())
	{
		PhysicalSensor = Entry->PhysicalKey.Sensor;
	}
	else if (!ResolvePhysicalSensor(
		OwnerIdentifier,
		Entry->Request.Sensor,
		Options,
		PhysicalSensor,
		ResolutionFailure
	))
	{
		return FOpenMobileSensorsErrorMapper::Map(
			ResolutionFailure
		);
	}
	FOpenMobileSensorStreamOptions AppliedOptions;
	FOpenMobileSensorRateResolution RateResolution;
	if (!ValidateAndResolveOptions(
		PhysicalSensor,
		Options,
		AppliedOptions,
		RateResolution
	))
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidRequest
		);
	}
	const FPhysicalStreamKey NewKey = MakePhysicalKey(
		PhysicalSensor,
		AppliedOptions
	);
	if (Entry->State == EOpenMobileSensorSubscriptionState::Active
		&& !(NewKey == Entry->PhysicalKey))
	{
		const FOpenMobileSensorStreamOptions PreviousRequested =
			Entry->Request.Options;
		const FOpenMobileSensorStreamOptions PreviousApplied =
			Entry->AppliedOptions;
		const FOpenMobileSensorRateResolution PreviousRateResolution =
			Entry->RateResolution;
		const EOpenMobileSensorRateAdjustmentReason PreviousCommonRateReason =
			Entry->CommonRateAdjustmentReason;
		const FPhysicalStreamKey PreviousKey = Entry->PhysicalKey;
		FPhysicalStreamEntry* PreviousPhysical =
			PhysicalStreams.Find(PreviousKey);
		if (!PreviousPhysical
			|| !PreviousPhysical->Backend
			|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
				PreviousPhysical->BackendToken
			))
		{
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::TemporarilyUnavailable
			);
		}
		CancelFlushesForPhysicalStream(PreviousPhysical->Handle);
		Entry = FindOwnedEntry(OwnerIdentifier, Handle);
		PreviousPhysical = PhysicalStreams.Find(PreviousKey);
		if (!Entry || !PreviousPhysical)
		{
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::TemporarilyUnavailable
			);
		}
		bool bPreviousStreamHasOtherSubscribers = false;
		for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
		{
			if (Pair.Key != Handle.GetIdentifier()
				&& Pair.Value.PhysicalKey == PreviousKey
				&& Pair.Value.State ==
					EOpenMobileSensorSubscriptionState::Active)
			{
				bPreviousStreamHasOtherSubscribers = true;
				break;
			}
		}
		auto RestoreEntry = [&]()
		{
			Entry->Request.Options = PreviousRequested;
			Entry->AppliedOptions = PreviousApplied;
			Entry->RateResolution = PreviousRateResolution;
			Entry->CommonRateAdjustmentReason = PreviousCommonRateReason;
			Entry->PhysicalKey = PreviousKey;
		};
		Entry->Request.Options = Options;
		Entry->AppliedOptions = AppliedOptions;
		Entry->RateResolution = RateResolution;
		Entry->CommonRateAdjustmentReason = RateResolution.AdjustmentReason;
		Entry->PhysicalKey = NewKey;
		FOpenMobileSensorPhysicalStreamRequest DesiredRequest;
		if (!BuildPhysicalRequest(NewKey, DesiredRequest))
		{
			RestoreEntry();
			return FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::TemporarilyUnavailable
			);
		}

		FOpenMobileSensorOperationResult Operation;
		Operation.Code = EOpenMobileSensorResultCode::Failed;
		FOpenMobileSensorBackendStreamHandle ActivePhysicalHandle;
		bool bMovedPreviousPhysical = false;
		FPhysicalStreamEntry* TargetPhysical = PhysicalStreams.Find(NewKey);
		if (TargetPhysical)
		{
			if (DesiredRequest == TargetPhysical->Request)
			{
				Operation = MakeSuccess();
			}
			else
			{
				Operation = TargetPhysical->Backend->ReconfigureSensorStream(
					TargetPhysical->Handle,
					DesiredRequest
				);
				if (Operation.IsSuccess())
				{
					TargetPhysical->Request = DesiredRequest;
				}
			}
			ActivePhysicalHandle = TargetPhysical->Handle;
		}
		else
		{
			FOpenMobileSensorBackendStreamHandle NewPhysicalHandle;
			NewPhysicalHandle.Identifier = FGuid::NewGuid();
			Operation = PreviousPhysical->Backend->StartSensorStream(
				NewPhysicalHandle,
				DesiredRequest
			);
			if (Operation.IsSuccess())
			{
				FPhysicalStreamEntry NewPhysical;
				NewPhysical.Handle = NewPhysicalHandle;
				NewPhysical.Key = NewKey;
				NewPhysical.Request = DesiredRequest;
				NewPhysical.BackendToken = PreviousPhysical->BackendToken;
				NewPhysical.Backend = PreviousPhysical->Backend;
				PhysicalStreams.Add(NewKey, MoveTemp(NewPhysical));
				ActivePhysicalHandle = NewPhysicalHandle;
			}
			else if (!bPreviousStreamHasOtherSubscribers)
			{
				PreviousPhysical = PhysicalStreams.Find(PreviousKey);
				FOpenMobileSensorPhysicalStreamRequest ReconfiguredRequest =
					DesiredRequest;
				if (PreviousPhysical)
				{
					Operation = PreviousPhysical->Backend->
						ReconfigureSensorStream(
							PreviousPhysical->Handle,
							ReconfiguredRequest
						);
				}
				if (PreviousPhysical && Operation.IsSuccess())
				{
					FPhysicalStreamEntry MovedPhysical;
					PhysicalStreams.RemoveAndCopyValue(
						PreviousKey,
						MovedPhysical
					);
					MovedPhysical.Key = NewKey;
					MovedPhysical.Request = MoveTemp(ReconfiguredRequest);
					ActivePhysicalHandle = MovedPhysical.Handle;
					PhysicalStreams.Add(NewKey, MoveTemp(MovedPhysical));
					bMovedPreviousPhysical = true;
				}
			}
		}
		if (!Operation.IsSuccess())
		{
			RestoreEntry();
			return Operation;
		}
		if (!bMovedPreviousPhysical)
		{
			ReconcilePhysicalStream(PreviousKey);
		}
		FOpenMobileSensorsSampleService::UpdateSubscriptionOptions(
			Handle,
			AppliedOptions
		);
		FOpenMobileSensorsSampleService::SetPhysicalStreamHandle(
			Handle,
			ActivePhysicalHandle
		);
		if (const FPhysicalStreamEntry* ActivePhysical =
			PhysicalStreams.Find(NewKey))
		{
			UpdateAppliedNativeRate(NewKey, ActivePhysical->Request);
		}
		BroadcastState(*Entry);
		return CompleteSubscriptionUpdate();
	}
	if (Entry->State == EOpenMobileSensorSubscriptionState::Active)
	{
		if (const FPhysicalStreamEntry* Physical =
			PhysicalStreams.Find(Entry->PhysicalKey))
		{
			CancelFlushesForPhysicalStream(Physical->Handle);
			Entry = FindOwnedEntry(OwnerIdentifier, Handle);
			if (!Entry)
			{
				return MakeHandleFailure(Handle);
			}
		}
	}

	const FOpenMobileSensorStreamOptions PreviousRequested =
		Entry->Request.Options;
	const FOpenMobileSensorStreamOptions PreviousApplied =
		Entry->AppliedOptions;
	const FOpenMobileSensorRateResolution PreviousRateResolution =
		Entry->RateResolution;
	const EOpenMobileSensorRateAdjustmentReason PreviousCommonRateReason =
		Entry->CommonRateAdjustmentReason;
	const FPhysicalStreamKey PreviousKey = Entry->PhysicalKey;
	Entry->Request.Options = Options;
	Entry->AppliedOptions = AppliedOptions;
	Entry->RateResolution = RateResolution;
	Entry->CommonRateAdjustmentReason = RateResolution.AdjustmentReason;
	Entry->PhysicalKey = NewKey;
	if (Entry->State != EOpenMobileSensorSubscriptionState::Active)
	{
		FOpenMobileSensorsSampleService::UpdateSubscriptionOptions(
			Handle,
			AppliedOptions
		);
		return CompleteSubscriptionUpdate();
	}

	FPhysicalStreamEntry* Physical = PhysicalStreams.Find(PreviousKey);
	FOpenMobileSensorPhysicalStreamRequest DesiredRequest;
	if (!Physical || !BuildPhysicalRequest(PreviousKey, DesiredRequest))
	{
		Entry->Request.Options = PreviousRequested;
		Entry->AppliedOptions = PreviousApplied;
		Entry->RateResolution = PreviousRateResolution;
		Entry->CommonRateAdjustmentReason = PreviousCommonRateReason;
		Entry->PhysicalKey = PreviousKey;
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
	}
	if (DesiredRequest == Physical->Request)
	{
		UpdateAppliedNativeRate(
			PreviousKey,
			Physical->Request
		);
		FOpenMobileSensorsSampleService::UpdateSubscriptionOptions(
			Handle,
			AppliedOptions
		);
		return CompleteSubscriptionUpdate();
	}
	FOpenMobileSensorPhysicalStreamRequest BackendRequest = DesiredRequest;
	const FOpenMobileSensorOperationResult ReconfigureResult =
		Physical->Backend->ReconfigureSensorStream(
			Physical->Handle,
			BackendRequest
		);
	if (!ReconfigureResult.IsSuccess())
	{
		Entry->Request.Options = PreviousRequested;
		Entry->AppliedOptions = PreviousApplied;
		Entry->RateResolution = PreviousRateResolution;
		Entry->CommonRateAdjustmentReason = PreviousCommonRateReason;
		Entry->PhysicalKey = PreviousKey;
		return ReconfigureResult;
	}
	Physical->Request = MoveTemp(BackendRequest);
	UpdateAppliedNativeRate(
		PreviousKey,
		Physical->Request
	);
	FOpenMobileSensorsSampleService::UpdateSubscriptionOptions(
		Handle,
		AppliedOptions
	);
	return CompleteSubscriptionUpdate();
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsSubscriptionService::StopSubscription(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	FSubscriptionEntry* Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		return MakeHandleFailure(Handle);
	}
	if (Entry->State == EOpenMobileSensorSubscriptionState::Stopping)
	{
		return MakeSuccess();
	}
	if (const FPhysicalStreamEntry* Physical =
		PhysicalStreams.Find(Entry->PhysicalKey))
	{
		CancelFlushesForPhysicalStream(Physical->Handle);
		Entry = FindOwnedEntry(OwnerIdentifier, Handle);
		if (!Entry)
		{
			return MakeSuccess();
		}
	}
	const FSubscriptionEntry StoppedEntry = *Entry;
	SetState(
		Handle.GetIdentifier(),
		EOpenMobileSensorSubscriptionState::Stopping
	);
	Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		return MakeSuccess();
	}
	const FPhysicalStreamKey Key = Entry->PhysicalKey;
	Subscriptions.Remove(Handle.GetIdentifier());
	FOpenMobileSensorsSampleService::UnregisterSubscription(Handle);
	ReconcilePhysicalStream(Key);
	FSubscriptionEntry FinalEntry = StoppedEntry;
	FinalEntry.State = EOpenMobileSensorSubscriptionState::Stopped;
	FinalEntry.Error = {};
	BroadcastState(FinalEntry);
	return MakeSuccess();
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsSubscriptionService::RecenterAttitude(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	EOpenMobileSensorRecenterMode Mode
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	const FSubscriptionEntry* Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		return MakeHandleFailure(Handle);
	}
	if (Entry->Request.Sensor.Type != EOpenMobileSensorType::Attitude)
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		);
	}
	if (Mode != EOpenMobileSensorRecenterMode::FullAttitude
		&& Mode != EOpenMobileSensorRecenterMode::YawOnly
		&& Mode != EOpenMobileSensorRecenterMode::Clear)
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidRequest
		);
	}
	if (!FOpenMobileSensorsSampleService::RecenterAttitude(
		OwnerIdentifier,
		Handle,
		Mode
	))
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
	}
	return MakeSuccess();
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsSubscriptionService::RecenterRelativeAltitude(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	const FSubscriptionEntry* Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		return MakeHandleFailure(Handle);
	}
	if (Entry->Request.Sensor.Type !=
		EOpenMobileSensorType::RelativeAltitude)
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		);
	}
	if (!FOpenMobileSensorsSampleService::RecenterRelativeAltitude(
		OwnerIdentifier,
		Handle
	))
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
	}
	return MakeSuccess();
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsSubscriptionService::ResetStepCountSession(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	const FSubscriptionEntry* Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		return MakeHandleFailure(Handle);
	}
	if (!Entry->bResettableStepCountSession
		|| Entry->Request.Sensor.Type != EOpenMobileSensorType::StepCounter)
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		);
	}
	if (!FOpenMobileSensorsSampleService::ResetStepCountSession(
		OwnerIdentifier,
		Handle
	))
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
	}
	return MakeSuccess();
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsSubscriptionService::RequestNativeCalibrationPrompt(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	const FSubscriptionEntry* Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		return MakeHandleFailure(Handle);
	}
	if (Entry->State != EOpenMobileSensorSubscriptionState::Active)
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
	}
	FPhysicalStreamEntry* Physical = PhysicalStreams.Find(Entry->PhysicalKey);
	if (!Physical
		|| !Physical->Backend
		|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
			Physical->BackendToken
		))
	{
		return FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		);
	}
	return Physical->Backend->RequestCalibrationPrompt(
		Physical->Handle,
		Entry->Request.Sensor
	);
}

int32 FOpenMobileSensorsSubscriptionService::StopAllSubscriptions(
	const FGuid& OwnerIdentifier
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	if (!OwnerIdentifier.IsValid())
	{
		return 0;
	}
	TArray<FOpenMobileSensorSubscriptionHandle> Handles;
	for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
	{
		if (Pair.Value.OwnerIdentifier == OwnerIdentifier)
		{
			Handles.Add(Pair.Value.Handle);
		}
	}
	int32 Removed = 0;
	for (const FOpenMobileSensorSubscriptionHandle& Handle : Handles)
	{
		Removed += StopSubscription(OwnerIdentifier, Handle).IsSuccess() ? 1 : 0;
	}
	return Removed;
}

bool FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	FOpenMobileSensorSubscriptionStateSnapshot& OutState
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	OutState = {};
	OutState.Handle = Handle;
	FSubscriptionEntry* Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		OutState.Error = MakeHandleFailure(Handle).Error;
		return false;
	}
	OutState = MakeSnapshot(*Entry);
	return true;
}

FOpenMobileSensorOperationResult
FOpenMobileSensorsSubscriptionService::GetHandleStatus(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	return FindOwnedEntry(OwnerIdentifier, Handle)
		? MakeSuccess()
		: MakeHandleFailure(Handle);
}

bool FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	return OpenMobileSensorsSubscriptionServicePrivate::FindOwnedEntry(
		OwnerIdentifier,
		Handle
	) != nullptr;
}

bool FOpenMobileSensorsSubscriptionService::IsHandleCurrent(
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	if (!Handle.IsValid())
	{
		return false;
	}
	FSubscriptionEntry* Entry = Subscriptions.Find(Handle.GetIdentifier());
	return Entry
		&& Entry->Handle == Handle
		&& FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
			Entry->BackendToken
		);
}

void FOpenMobileSensorsSubscriptionService::FlushSubscription(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	const FGuid& RequestId,
	TFunction<void(const FOpenMobileSensorFlushResult&)>&& Completion
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	auto FinishImmediately = [&Completion, &Handle, &RequestId](
		const FOpenMobileSensorOperationResult& Operation
	)
	{
		FOpenMobileSensorFlushResult Result;
		Result.RequestId = RequestId;
		Result.Handle = Handle;
		Result.Operation = Operation;
		if (Completion)
		{
			Completion(Result);
		}
	};
	if (!RequestId.IsValid() || PendingFlushes.Contains(RequestId))
	{
		FinishImmediately(FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::InvalidRequest
		));
		return;
	}
	FSubscriptionEntry* Entry = FindOwnedEntry(OwnerIdentifier, Handle);
	if (!Entry)
	{
		FinishImmediately(MakeHandleFailure(Handle));
		return;
	}
	if (Entry->State != EOpenMobileSensorSubscriptionState::Active)
	{
		FinishImmediately(FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		));
		return;
	}
	FPhysicalStreamEntry* Physical = PhysicalStreams.Find(Entry->PhysicalKey);
	if (!Physical
		|| !Physical->Backend
		|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(
			Physical->BackendToken
		))
	{
		FinishImmediately(FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::TemporarilyUnavailable
		));
		return;
	}
	const bool bHasNativeFlush =
		Physical->Request.bNativeBatchingApplied;
	const bool bHasPluginFlush =
		Entry->AppliedOptions.DeliveryMode ==
			EOpenMobileSensorDeliveryMode::Buffered
		|| Entry->AppliedOptions.DeliveryMode ==
			EOpenMobileSensorDeliveryMode::EventBatches;
	if (!bHasNativeFlush && !bHasPluginFlush)
	{
		FinishImmediately(FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		));
		return;
	}
	for (const TPair<FGuid, FPendingFlush>& Pair : PendingFlushes)
	{
		if (Pair.Value.PhysicalStreamHandle == Physical->Handle)
		{
			FinishImmediately(FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::OperationalFailure,
				TEXT("OpenMobileSensors"),
				TEXT("ConcurrentFlush")
			));
			return;
		}
	}
	FPendingFlush Pending;
	Pending.OwnerIdentifier = OwnerIdentifier;
	Pending.Handle = Handle;
	Pending.PhysicalStreamHandle = Physical->Handle;
	Pending.BackendToken = Physical->BackendToken;
	Pending.DeadlineSeconds = FPlatformTime::Seconds() + 5.0;
	Pending.Completion = MoveTemp(Completion);
	PendingFlushes.Add(RequestId, MoveTemp(Pending));
	if (!bHasNativeFlush)
	{
		CompleteFlush(RequestId, MakeSuccess());
		return;
	}
	IOpenMobileSensorsBackend* Backend = Physical->Backend;
	const FOpenMobileSensorBackendStreamHandle PhysicalHandle = Physical->Handle;
	const FOpenMobileSensorOperationResult Operation =
		Backend->FlushSensorStream(
			PhysicalHandle,
			RequestId,
			FOnOpenMobileSensorBackendFlushComplete::CreateLambda(
				[RequestId](
					const FGuid& CallbackRequestId,
					const FOpenMobileSensorOperationResult& CallbackOperation
				)
				{
					if (CallbackRequestId != RequestId)
					{
						return;
					}
					OpenMobile::DispatchToGameThread(
						[RequestId, CallbackOperation]()
						{
							CompleteFlush(RequestId, CallbackOperation);
						}
					);
				}
			)
		);
	if (!Operation.IsSuccess())
	{
		CompleteFlush(RequestId, Operation);
		return;
	}
	if (PendingFlushes.Contains(RequestId))
	{
		EnsureFlushTimeoutTick();
	}
}

bool FOpenMobileSensorsSubscriptionService::CancelFlush(
	const FGuid& OwnerIdentifier,
	const FGuid& RequestId
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	const FPendingFlush* Pending = PendingFlushes.Find(RequestId);
	if (!Pending || Pending->OwnerIdentifier != OwnerIdentifier)
	{
		return false;
	}
	CompleteFlush(
		RequestId,
		FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::Cancelled
		)
	);
	return true;
}

TArray<FOpenMobileSensorStreamDiagnostics>
FOpenMobileSensorsSubscriptionService::GetStreamDiagnostics(
	const FGuid& OwnerIdentifier
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	TArray<FOpenMobileSensorStreamDiagnostics> Streams;
	if (!OwnerIdentifier.IsValid())
	{
		return Streams;
	}
	for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
	{
		const FSubscriptionEntry& Entry = Pair.Value;
		if (Entry.OwnerIdentifier != OwnerIdentifier)
		{
			continue;
		}
		FOpenMobileSensorStreamDiagnostics& Diagnostics =
			Streams.AddDefaulted_GetRef();
		Diagnostics.Subscription = MakeSnapshot(Entry);
		FOpenMobileSensorsSampleService::GetRateDiagnostics(
			OwnerIdentifier,
			Entry.Handle,
			Diagnostics.Rate
		);
		FOpenMobileSensorsSampleService::GetDeliveryDiagnostics(
			OwnerIdentifier,
			Entry.Handle,
			FPlatformTime::Seconds(),
			Diagnostics
		);
		Diagnostics.Rate.RequestedFrequencyHz =
			Entry.RateResolution.RequestedFrequencyHz;
		Diagnostics.Rate.AppliedFrequencyHz =
			Entry.RateResolution.AppliedNativeFrequencyHz;
		Diagnostics.BatchingMode = GetBatchingMode(Entry);
	}
	return Streams;
}

TArray<FOpenMobileSensorStreamDiagnostics>
FOpenMobileSensorsSubscriptionService::GetAllStreamDiagnostics()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	TArray<FOpenMobileSensorStreamDiagnostics> Streams;
	for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
	{
		const FSubscriptionEntry& Entry = Pair.Value;
		FOpenMobileSensorStreamDiagnostics& Diagnostics =
			Streams.AddDefaulted_GetRef();
		Diagnostics.Subscription = MakeSnapshot(Entry);
		FOpenMobileSensorsSampleService::GetRateDiagnostics(
			Entry.OwnerIdentifier,
			Entry.Handle,
			Diagnostics.Rate
		);
		FOpenMobileSensorsSampleService::GetDeliveryDiagnostics(
			Entry.OwnerIdentifier,
			Entry.Handle,
			FPlatformTime::Seconds(),
			Diagnostics
		);
		Diagnostics.Rate.RequestedFrequencyHz =
			Entry.RateResolution.RequestedFrequencyHz;
		Diagnostics.Rate.AppliedFrequencyHz =
			Entry.RateResolution.AppliedNativeFrequencyHz;
		Diagnostics.BatchingMode = GetBatchingMode(Entry);
	}
	return Streams;
}

TArray<FOpenMobileSensorPhysicalStreamDiagnostics>
FOpenMobileSensorsSubscriptionService::GetPhysicalStreamDiagnostics(
	const FGuid* OwnerIdentifier
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	TArray<FOpenMobileSensorPhysicalStreamDiagnostics> Streams;
	for (const TPair<FPhysicalStreamKey, FPhysicalStreamEntry>& Pair
		: PhysicalStreams)
	{
		const FPhysicalStreamEntry& Physical = Pair.Value;
		FOpenMobileSensorPhysicalStreamDiagnostics Diagnostics;
		Diagnostics.Sensor = Physical.Key.Sensor;
		Diagnostics.BackendName = Physical.Backend
			? Physical.Backend->GetBackendName()
			: NAME_None;
		Diagnostics.AttitudeReferenceFrame =
			Physical.Key.AttitudeReferenceFrame;
		Diagnostics.AppliedFrequencyHz =
			Physical.Request.RequestedFrequencyHz;
		Diagnostics.MaximumDeliveryLatencySeconds =
			Physical.Request.MaximumDeliveryLatencySeconds;
		Diagnostics.bLowLatency = Physical.Request.bLowLatency;
		Diagnostics.bNativeBatchingRequested =
			Physical.Request.bNativeBatchingRequested;
		Diagnostics.bNativeBatchingApplied =
			Physical.Request.bNativeBatchingApplied;
		bool bVisibleToOwner = OwnerIdentifier == nullptr;
		for (const TPair<FGuid, FSubscriptionEntry>& Subscription
			: Subscriptions)
		{
			const FSubscriptionEntry& Entry = Subscription.Value;
			if (Entry.PhysicalKey == Physical.Key
				&& (Entry.State == EOpenMobileSensorSubscriptionState::Accepted
					|| Entry.State ==
						EOpenMobileSensorSubscriptionState::Starting
					|| Entry.State ==
						EOpenMobileSensorSubscriptionState::Active))
			{
				++Diagnostics.SubscriberCount;
				bVisibleToOwner |= !OwnerIdentifier
					|| Entry.OwnerIdentifier == *OwnerIdentifier;
			}
		}
		if (bVisibleToOwner)
		{
			Streams.Add(MoveTemp(Diagnostics));
		}
	}
	return Streams;
}

TArray<FOpenMobileError>
FOpenMobileSensorsSubscriptionService::GetRecentErrors(
	const FGuid* OwnerIdentifier
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	TArray<FOpenMobileError> Result;
	for (const FRecentErrorEntry& Entry : RecentErrors)
	{
		if (!OwnerIdentifier || Entry.OwnerIdentifier == *OwnerIdentifier)
		{
			Result.Add(Entry.Report.Error);
		}
	}
	return Result;
}

TArray<FOpenMobileSensorErrorReport>
FOpenMobileSensorsSubscriptionService::GetRecentErrorReports(
	const FGuid* OwnerIdentifier,
	const FGuid* SubscriptionIdentifier
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	TArray<FOpenMobileSensorErrorReport> Result;
	for (const FRecentErrorEntry& Entry : RecentErrors)
	{
		if ((!OwnerIdentifier || Entry.OwnerIdentifier == *OwnerIdentifier)
			&& (!SubscriptionIdentifier
				|| Entry.Report.Context.SubscriptionIdentifier ==
					*SubscriptionIdentifier))
		{
			Result.Add(Entry.Report);
		}
	}
	return Result;
}

TArray<FOpenMobileSensorSubscriptionHandle>
FOpenMobileSensorsSubscriptionService::SelectSubscribersForSample(
	const FOpenMobileSensorIdentifier& Sensor,
	double TimestampSeconds
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	TArray<FOpenMobileSensorSubscriptionHandle> DueSubscribers;
	if (!FMath::IsFinite(TimestampSeconds) || TimestampSeconds < 0.0)
	{
		return DueSubscribers;
	}
	constexpr double DeliveryToleranceSeconds = 1.e-9;
	for (TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
	{
		FSubscriptionEntry& Entry = Pair.Value;
		if (Entry.State != EOpenMobileSensorSubscriptionState::Active
			|| Entry.Request.Sensor != Sensor)
		{
			continue;
		}
		Entry.NativeRecoveryAttemptCount = 0;
		Entry.NextRecoveryAttemptSeconds = 0.0;
		const double IntervalSeconds =
			1.0 / Entry.AppliedOptions.MaximumCallbackFrequencyHz;
		if (Entry.bHasDeliveredSample
			&& TimestampSeconds + DeliveryToleranceSeconds
				< Entry.LastDeliveryTimestampSeconds + IntervalSeconds)
		{
			continue;
		}
		Entry.bHasDeliveredSample = true;
		Entry.LastDeliveryTimestampSeconds = TimestampSeconds;
		DueSubscribers.Add(Entry.Handle);
	}
	return DueSubscribers;
}

void FOpenMobileSensorsSubscriptionService::
InvalidateForUnrecoverablePermissionLoss(EOpenMobileSensorType SensorType)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	TArray<TPair<FGuid, FOpenMobileSensorSubscriptionHandle>> Handles;
	for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
	{
		if (Pair.Value.Request.Sensor.Type == SensorType)
		{
			Handles.Emplace(Pair.Value.OwnerIdentifier, Pair.Value.Handle);
		}
	}
	for (const TPair<FGuid, FOpenMobileSensorSubscriptionHandle>& Pair : Handles)
	{
		StopSubscription(Pair.Key, Pair.Value);
	}
}

bool FOpenMobileSensorsSubscriptionService::FailPhysicalStreamFromBackend(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle,
	const FOpenMobileSensorOperationResult& Failure
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	if (!PhysicalStreamHandle.IsValid()
		|| !FOpenMobileSensorsBackendRegistry::IsTokenCurrent(Token))
	{
		return false;
	}
	FPhysicalStreamKey FailedKey;
	bool bFound = false;
	for (const TPair<FPhysicalStreamKey, FPhysicalStreamEntry>& Pair
		: PhysicalStreams)
	{
		if (Pair.Value.Handle == PhysicalStreamHandle
			&& Pair.Value.BackendToken.Generation == Token.Generation)
		{
			FailedKey = Pair.Key;
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		return false;
	}
	CancelFlushesForPhysicalStream(PhysicalStreamHandle);
	FPhysicalStreamEntry FailedPhysical = PhysicalStreams.FindChecked(
		FailedKey
	);
	if (FailedPhysical.Backend
		&& FOpenMobileSensorsBackendRegistry::IsBackendRegistered(
			FailedPhysical.Backend
		))
	{
		FailedPhysical.Backend->StopSensorStream(
			FailedPhysical.Handle
		);
	}
	PhysicalStreams.Remove(FailedKey);
	TArray<FGuid> FailedSubscriptions;
	for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
	{
		if (Pair.Value.PhysicalKey == FailedKey
			&& Pair.Value.BackendToken.Generation == Token.Generation
			&& Pair.Value.State != EOpenMobileSensorSubscriptionState::Stopped)
		{
			FailedSubscriptions.Add(Pair.Key);
		}
	}
	RecoverOrFailSubscriptions(
		FailedSubscriptions,
		Failure,
		FPlatformTime::Seconds()
	);
	return true;
}

FOnOpenMobileSensorSubscriptionServiceStateChanged&
FOpenMobileSensorsSubscriptionService::OnStateChanged()
{
	return OpenMobileSensorsSubscriptionServicePrivate::StateChangedEvent;
}

#if WITH_DEV_AUTOMATION_TESTS
int32 FOpenMobileSensorsSubscriptionService::
GetActiveSubscriptionCountForTests()
{
	check(IsInGameThread());
	return OpenMobileSensorsSubscriptionServicePrivate::Subscriptions.Num();
}

int32 FOpenMobileSensorsSubscriptionService::
GetActiveSubscriptionCountForTests(
	const FOpenMobileSensorIdentifier& Sensor
)
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	int32 Count = 0;
	for (const TPair<FGuid, FSubscriptionEntry>& Pair : Subscriptions)
	{
		if (Pair.Value.Request.Sensor == Sensor)
		{
			++Count;
		}
	}
	return Count;
}

int32 FOpenMobileSensorsSubscriptionService::
GetPhysicalStreamCountForTests()
{
	check(IsInGameThread());
	return OpenMobileSensorsSubscriptionServicePrivate::PhysicalStreams.Num();
}

void FOpenMobileSensorsSubscriptionService::
ProcessPendingBackendOperationsForTests()
{
	check(IsInGameThread());
	OpenMobileSensorsSubscriptionServicePrivate::
		ProcessPendingBackendOperations(FPlatformTime::Seconds());
}

void FOpenMobileSensorsSubscriptionService::
ProcessPendingBackendOperationsForTests(double NowSeconds)
{
	check(IsInGameThread());
	OpenMobileSensorsSubscriptionServicePrivate::
		ProcessPendingBackendOperations(NowSeconds);
}

void FOpenMobileSensorsSubscriptionService::ProcessFlushTimeoutsForTests(
	double NowSeconds
)
{
	check(IsInGameThread());
	OpenMobileSensorsSubscriptionServicePrivate::ProcessFlushTimeouts(
		NowSeconds
	);
}

void FOpenMobileSensorsSubscriptionService::SetSubscriptionStateForTests(
	const FOpenMobileSensorSubscriptionHandle& Handle,
	EOpenMobileSensorSubscriptionState State
)
{
	check(IsInGameThread());
	OpenMobileSensorsSubscriptionServicePrivate::SetState(
		Handle.GetIdentifier(),
		State
	);
}

void FOpenMobileSensorsSubscriptionService::RecordErrorReportForTests(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorOperationResult& Operation,
	const FOpenMobileSensorErrorContext& Context
)
{
	check(IsInGameThread());
	OpenMobileSensorsSubscriptionServicePrivate::RecordOperationFailure(
		OwnerIdentifier,
		Operation,
		Context
	);
}

void FOpenMobileSensorsSubscriptionService::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileSensorsSubscriptionServicePrivate;
	CancelPendingOperationsTick();
	CancelAllFlushes();
	Subscriptions.Reset();
	PhysicalStreams.Reset();
	RecentErrors.Reset();
	FOpenMobileSensorsSampleService::UnregisterAll();
	StateChangedEvent.Clear();
	NextHandleGeneration = 1;
	bShuttingDown = false;
	bApplicationActive = true;
	bApplicationInForeground = true;
	RegisterLifecycleDelegates();
}
#endif
