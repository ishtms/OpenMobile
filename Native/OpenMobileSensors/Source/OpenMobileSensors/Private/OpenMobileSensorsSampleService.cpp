#include "OpenMobileSensorsSampleService.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "OpenMobileActivitySampleFilter.h"
#include "OpenMobileActivityTransitionTracker.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsBackendTypes.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorGravityEstimator.h"
#include "OpenMobileSensorHeadingQuality.h"
#include "OpenMobileSensorLinearAccelerationEstimator.h"
#include "OpenMobileSensorOrientationClassifier.h"
#include "OpenMobileSensorRelativeAltitudeEstimator.h"
#include "OpenMobileSensorFusionQuality.h"
#include "OpenMobileSensorCoordinates.h"
#include "OpenMobileSensorScreenRotationService.h"
#include "OpenMobileSensorSourcePolicy.h"
#include "OpenMobileSensorValidity.h"
#include "OpenMobileSensorVectorFilter.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsTrueHeadingService.h"
#include "OpenMobileStepCountSessionTracker.h"
#include "OpenMobileStepDetectionTracker.h"

namespace OpenMobileSensorsSampleServicePrivate
{
	constexpr int32 MaximumPendingAccuracyChanges = 16;
	constexpr int32 MaximumPendingCalibrationChanges = 16;
	constexpr double CalibrationEventCooldownSeconds = 30.0;

	enum class ELatestSampleFamily : uint8
	{
		None,
		Vector,
		Attitude,
		Scalar,
		Heading,
		Steps,
		Activity,
		Orientation,
		Proximity
	};

	template <typename SampleType>
	struct TFixedSampleRingBuffer
	{
		TArray<SampleType> Storage;
		int32 StartIndex = 0;
		int32 SampleCount = 0;
		int32 HighWaterMark = 0;
		int64 DroppedSamples = 0;

		void Initialize(int32 Capacity)
		{
			Storage.SetNum(Capacity);
			StartIndex = 0;
			SampleCount = 0;
			HighWaterMark = 0;
			DroppedSamples = 0;
		}

		void Clear()
		{
			Storage.Reset();
			StartIndex = 0;
			SampleCount = 0;
			HighWaterMark = 0;
			DroppedSamples = 0;
		}

		void Resize(
			int32 Capacity,
			EOpenMobileSensorOverflowPolicy OverflowPolicy
		)
		{
			if (Storage.Num() == Capacity)
			{
				return;
			}
			TArray<SampleType> NewStorage;
			NewStorage.SetNum(Capacity);
			const int32 KeptSamples = FMath::Min(SampleCount, Capacity);
			const int32 FirstKeptOffset =
				OverflowPolicy == EOpenMobileSensorOverflowPolicy::DropOldest
				? SampleCount - KeptSamples
				: 0;
			for (int32 Index = 0; Index < KeptSamples; ++Index)
			{
				const int32 SourceIndex =
					(StartIndex + FirstKeptOffset + Index) % Storage.Num();
				NewStorage[Index] = MoveTemp(Storage[SourceIndex]);
			}
			DroppedSamples += SampleCount - KeptSamples;
			Storage = MoveTemp(NewStorage);
			StartIndex = 0;
			SampleCount = KeptSamples;
			HighWaterMark = FMath::Max(HighWaterMark, SampleCount);
		}

		void Enqueue(
			const SampleType& Sample,
			EOpenMobileSensorOverflowPolicy OverflowPolicy
		)
		{
			if (Storage.IsEmpty())
			{
				return;
			}
			if (SampleCount == Storage.Num())
			{
				++DroppedSamples;
				if (OverflowPolicy ==
					EOpenMobileSensorOverflowPolicy::RejectNewest)
				{
					return;
				}
				Storage[StartIndex] = Sample;
				StartIndex = (StartIndex + 1) % Storage.Num();
				return;
			}
			const int32 WriteIndex =
				(StartIndex + SampleCount) % Storage.Num();
			Storage[WriteIndex] = Sample;
			++SampleCount;
			HighWaterMark = FMath::Max(HighWaterMark, SampleCount);
		}

		void Drain(int32 MaximumSamples, TArray<SampleType>& OutSamples)
		{
			const int32 ReturnedSamples =
				FMath::Min(MaximumSamples, SampleCount);
			OutSamples.Reset();
			OutSamples.Reserve(ReturnedSamples);
			for (int32 Index = 0; Index < ReturnedSamples; ++Index)
			{
				const int32 ReadIndex = (StartIndex + Index) % Storage.Num();
				OutSamples.Add(MoveTemp(Storage[ReadIndex]));
			}
			if (!Storage.IsEmpty())
			{
				StartIndex = (StartIndex + ReturnedSamples) % Storage.Num();
			}
			SampleCount -= ReturnedSamples;
		}

		double GetOldestTimestampSeconds() const
		{
			return SampleCount > 0
				? Storage[StartIndex].Header.TimestampSeconds
				: 0.0;
		}
	};

	struct FLatestSlot
	{
		FCriticalSection Mutex;
		FGuid OwnerIdentifier;
		FOpenMobileSensorSubscriptionHandle Handle;
		FOpenMobileSensorBackendStreamHandle PhysicalStreamHandle;
		FOpenMobileSensorIdentifier Sensor;
		FOpenMobileSensorIdentifier PhysicalSensor;
		uint64 BackendGeneration = 0;
		EOpenMobileSensorSubscriptionState State =
			EOpenMobileSensorSubscriptionState::Accepted;
		ELatestSampleFamily Family = ELatestSampleFamily::None;
		ELatestSampleFamily ExpectedFamily = ELatestSampleFamily::None;
		int64 NextSequence = 1;
		double LatestTimestampSeconds = 0.0;
		double StaleAfterSeconds = 1.0;
		double AppliedSampleFrequencyHz = 0.0;
		double MaximumCallbackFrequencyHz = 15.0;
		double MinimumScalarEventChange = 0.0;
		double LastScalarEventValue = 0.0;
		double LastCallbackTimeSeconds = 0.0;
		double RateIntervals[64] = {};
		double RateIntervalSum = 0.0;
		double RateIntervalSquareSum = 0.0;
		double LastRateTimestampSeconds = 0.0;
		double LastRateGapSeconds = 0.0;
		FVector GyroscopeAngularVelocities[64] = {};
		FVector GyroscopeAngularVelocitySum = FVector::ZeroVector;
		double GyroscopeAngularSpeedSquareSum = 0.0;
		double LastGyroscopeTimestampSeconds = 0.0;
		double LastGameThreadProcessingSeconds = 0.0;
		int32 RateIntervalStart = 0;
		int32 RateIntervalCount = 0;
		int32 GyroscopeSampleStart = 0;
		int32 GyroscopeSampleCount = 0;
		int32 EventHighWaterMark = 0;
		int64 EventDroppedSamples = 0;
		int32 PendingTimestampIssueFlags = 0;
		int32 MaximumPendingSamples = 128;
		EOpenMobileSensorDeliveryMode DeliveryMode =
			EOpenMobileSensorDeliveryMode::LatestValue;
		EOpenMobileSensorAccuracy MinimumCallbackAccuracy =
			EOpenMobileSensorAccuracy::Unknown;
		EOpenMobileSensorOverflowPolicy OverflowPolicy =
			EOpenMobileSensorOverflowPolicy::DropOldest;
		EOpenMobileSensorCoordinateSpace CoordinateSpace =
			EOpenMobileSensorCoordinateSpace::DeviceFixed;
		EOpenMobileAttitudeReferenceFrame AttitudeReferenceFrame =
			EOpenMobileAttitudeReferenceFrame::GameRelative;
		int32 AttitudeRepresentations = static_cast<int32>(
			EOpenMobileAttitudeRepresentation::Quaternion
		);
		FOpenMobileSensorRecenterState Recenter;
		FQuat LatestCanonicalAttitude = FQuat::Identity;
		FOpenMobileSensorFilterOptions FilterOptions;
		FOpenMobileSensorVectorFilter VectorFilter;
		FOpenMobileSensorGravityEstimator GravityEstimator;
		FOpenMobileSensorLinearAccelerationEstimator
			LinearAccelerationEstimator;
		FOpenMobileSensorRelativeAltitudeEstimator RelativeAltitudeEstimator;
		FOpenMobileStepCountSessionTracker StepCountSessionTracker;
		FOpenMobileStepDetectionTracker StepDetectionTracker;
		FOpenMobileActivitySampleFilter ActivityFilter;
		FOpenMobileActivityTransitionEventFilter ActivityTransitionFilter;
		FOpenMobileSensorOrientationClassifier OrientationClassifier;
		FOpenMobileSensorOrientationClassifierConfig OrientationConfig;
		bool bHasSample = false;
		bool bHasCallbackTime = false;
		bool bHasLastScalarEventValue = false;
		bool bHasRateTimestamp = false;
		bool bHasAccuracyState = false;
		bool bCalibrationNeeded = false;
		bool bCalibrationRequiredEventEmitted = false;
		bool bHasCalibrationRequiredEventTime = false;
		bool bHasSourceState = false;
		bool bHasCanonicalAttitude = false;
		bool bResettableStepCountSession = false;
		bool bPendingStatefulProcessingReset = false;
		int32 LastSourceFlags = 0;
		int64 NextAccuracySequence = 1;
		int64 NextCalibrationSequence = 1;
		double LastCalibrationRequiredEventSeconds = 0.0;
		FOpenMobileSensorAccuracySnapshot AccuracyState;
		FOpenMobileVectorSensorSample Vector;
		FOpenMobileAttitudeSensorSample Attitude;
		FOpenMobileScalarSensorSample Scalar;
		FOpenMobileHeadingSensorSample Heading;
		FOpenMobileStepsSensorSample Steps;
		FOpenMobileActivitySensorSample Activity;
		FOpenMobileOrientationSensorSample Orientation;
		FOpenMobileProximitySensorSample Proximity;
		TArray<FOpenMobileVectorSensorSample> PendingVector;
		TArray<FOpenMobileAttitudeSensorSample> PendingAttitude;
		TArray<FOpenMobileScalarSensorSample> PendingScalar;
		TArray<FOpenMobileHeadingSensorSample> PendingHeading;
		TArray<FOpenMobileStepsSensorSample> PendingSteps;
		TArray<FOpenMobileActivitySensorSample> PendingActivity;
		TArray<FOpenMobileOrientationSensorSample> PendingOrientation;
		TArray<FOpenMobileProximitySensorSample> PendingProximity;
		TArray<FOpenMobileSensorAccuracySnapshot> PendingAccuracyChanges;
		TArray<FOpenMobileSensorCalibrationEvent> PendingCalibrationChanges;
		TFixedSampleRingBuffer<FOpenMobileVectorSensorSample> BufferedVector;
		TFixedSampleRingBuffer<FOpenMobileAttitudeSensorSample> BufferedAttitude;
		TFixedSampleRingBuffer<FOpenMobileScalarSensorSample> BufferedScalar;
		TFixedSampleRingBuffer<FOpenMobileHeadingSensorSample> BufferedHeading;
		TFixedSampleRingBuffer<FOpenMobileStepsSensorSample> BufferedSteps;
		TFixedSampleRingBuffer<FOpenMobileActivitySensorSample> BufferedActivity;
		TFixedSampleRingBuffer<FOpenMobileOrientationSensorSample>
			BufferedOrientation;
		TFixedSampleRingBuffer<FOpenMobileProximitySensorSample>
			BufferedProximity;
	};

	FRWLock SlotsLock;
	TMap<
		FOpenMobileSensorSubscriptionHandle,
		TUniquePtr<FLatestSlot>
	> Slots;
	FCriticalSection EventTickerMutex;
	FTSTicker::FDelegateHandle EventTickerHandle;
	TAtomic<bool> bShuttingDown(false);
	FOnOpenMobileVectorSensorBatchReady VectorBatchEvent;
	FOnOpenMobileAttitudeSensorBatchReady AttitudeBatchEvent;
	FOnOpenMobileScalarSensorBatchReady ScalarBatchEvent;
	FOnOpenMobileHeadingSensorBatchReady HeadingBatchEvent;
	FOnOpenMobileStepsSensorBatchReady StepsBatchEvent;
	FOnOpenMobileActivitySensorBatchReady ActivityBatchEvent;
	FOnOpenMobileOrientationSensorBatchReady OrientationBatchEvent;
	FOnOpenMobileProximitySensorBatchReady ProximityBatchEvent;
	FOnOpenMobileSensorAccuracyChangedReady AccuracyChangedEvent;
	FOnOpenMobileSensorCalibrationChangedReady CalibrationChangedEvent;
	struct FBackendActivityTransitionTrackerState
	{
		uint64 BackendGeneration = 0;
		FOpenMobileActivityTransitionTracker Tracker;
	};
	FCriticalSection ActivityTransitionTrackersMutex;
	TMap<FGuid, FBackendActivityTransitionTrackerState>
		BackendActivityTransitionTrackers;
	FOpenMobileActivityTransitionTracker DirectActivityTransitionTracker;
	bool bDirectActivityTransitionTrackerConfigured = false;

	void EnsureEventTicker();

	void ResetRateStatistics(FLatestSlot& Slot)
	{
		Slot.RateIntervalSum = 0.0;
		Slot.RateIntervalSquareSum = 0.0;
		Slot.LastRateTimestampSeconds = 0.0;
		Slot.LastRateGapSeconds = 0.0;
		Slot.RateIntervalStart = 0;
		Slot.RateIntervalCount = 0;
		Slot.bHasRateTimestamp = false;
	}

	void ResetGyroscopeDriftStatistics(FLatestSlot& Slot)
	{
		Slot.GyroscopeAngularVelocitySum = FVector::ZeroVector;
		Slot.GyroscopeAngularSpeedSquareSum = 0.0;
		Slot.LastGyroscopeTimestampSeconds = 0.0;
		Slot.GyroscopeSampleStart = 0;
		Slot.GyroscopeSampleCount = 0;
	}

	void ResetDerivedEstimators(FLatestSlot& Slot)
	{
		Slot.GravityEstimator.Reset();
		Slot.LinearAccelerationEstimator.Reset();
		Slot.RelativeAltitudeEstimator.Reset();
		Slot.OrientationClassifier.Reset();
		Slot.ActivityFilter.Reset();
	}

	template <typename SampleType>
	void UpdateGyroscopeDriftStatistics(
		FLatestSlot& Slot,
		const SampleType& Sample
	)
	{
		static_cast<void>(Slot);
		static_cast<void>(Sample);
	}

	void UpdateGyroscopeDriftStatistics(
		FLatestSlot& Slot,
		const FOpenMobileVectorSensorSample& Sample
	)
	{
		if (Sample.Header.Sensor.Type != EOpenMobileSensorType::Gyroscope)
		{
			return;
		}
		const double LongGapSeconds = FMath::Max(
			5.0,
			Slot.AppliedSampleFrequencyHz > 0.0
				? 10.0 / Slot.AppliedSampleFrequencyHz
				: 5.0
		);
		if (Sample.Header.bStatefulProcessingReset
			|| (Slot.GyroscopeSampleCount > 0
				&& Sample.Header.TimestampSeconds -
					Slot.LastGyroscopeTimestampSeconds > LongGapSeconds))
		{
			ResetGyroscopeDriftStatistics(Slot);
		}
		if (Slot.GyroscopeSampleCount ==
			UE_ARRAY_COUNT(Slot.GyroscopeAngularVelocities))
		{
			const FVector& Removed = Slot.GyroscopeAngularVelocities[
				Slot.GyroscopeSampleStart
			];
			Slot.GyroscopeAngularVelocitySum -= Removed;
			Slot.GyroscopeAngularSpeedSquareSum -= Removed.SizeSquared();
			Slot.GyroscopeAngularVelocities[Slot.GyroscopeSampleStart] =
				Sample.Value;
			Slot.GyroscopeSampleStart =
				(Slot.GyroscopeSampleStart + 1)
				% UE_ARRAY_COUNT(Slot.GyroscopeAngularVelocities);
		}
		else
		{
			const int32 WriteIndex =
				(Slot.GyroscopeSampleStart + Slot.GyroscopeSampleCount)
				% UE_ARRAY_COUNT(Slot.GyroscopeAngularVelocities);
			Slot.GyroscopeAngularVelocities[WriteIndex] = Sample.Value;
			++Slot.GyroscopeSampleCount;
		}
		Slot.GyroscopeAngularVelocitySum += Sample.Value;
		Slot.GyroscopeAngularSpeedSquareSum += Sample.Value.SizeSquared();
		Slot.LastGyroscopeTimestampSeconds = Sample.Header.TimestampSeconds;
	}

	void UpdateRateStatistics(FLatestSlot& Slot, double TimestampSeconds)
	{
		if (!Slot.bHasRateTimestamp)
		{
			Slot.LastRateTimestampSeconds = TimestampSeconds;
			Slot.bHasRateTimestamp = true;
			return;
		}
		const double IntervalSeconds =
			TimestampSeconds - Slot.LastRateTimestampSeconds;
		if (IntervalSeconds <= 0.0)
		{
			ResetRateStatistics(Slot);
			return;
		}
		const double LongGapSeconds = FMath::Max(
			5.0,
			Slot.AppliedSampleFrequencyHz > 0.0
				? 10.0 / Slot.AppliedSampleFrequencyHz
				: 5.0
		);
		if (IntervalSeconds > LongGapSeconds)
		{
			ResetRateStatistics(Slot);
			Slot.LastRateTimestampSeconds = TimestampSeconds;
			Slot.bHasRateTimestamp = true;
			return;
		}
		if (Slot.RateIntervalCount == UE_ARRAY_COUNT(Slot.RateIntervals))
		{
			const double Removed =
				Slot.RateIntervals[Slot.RateIntervalStart];
			Slot.RateIntervalSum -= Removed;
			Slot.RateIntervalSquareSum -= Removed * Removed;
			Slot.RateIntervals[Slot.RateIntervalStart] = IntervalSeconds;
			Slot.RateIntervalStart =
				(Slot.RateIntervalStart + 1)
				% UE_ARRAY_COUNT(Slot.RateIntervals);
		}
		else
		{
			const int32 WriteIndex =
				(Slot.RateIntervalStart + Slot.RateIntervalCount)
				% UE_ARRAY_COUNT(Slot.RateIntervals);
			Slot.RateIntervals[WriteIndex] = IntervalSeconds;
			++Slot.RateIntervalCount;
		}
		Slot.RateIntervalSum += IntervalSeconds;
		Slot.RateIntervalSquareSum += IntervalSeconds * IntervalSeconds;
		Slot.LastRateTimestampSeconds = TimestampSeconds;
		Slot.LastRateGapSeconds = IntervalSeconds;
	}

	void RecordTimestampIssue(
		FLatestSlot& Slot,
		EOpenMobileSensorTimestampIssue Issue
	)
	{
		ResetRateStatistics(Slot);
		ResetDerivedEstimators(Slot);
		Slot.PendingTimestampIssueFlags |= static_cast<int32>(Issue);
		Slot.bPendingStatefulProcessingReset = true;
	}

	ELatestSampleFamily GetExpectedFamily(EOpenMobileSensorType SensorType)
	{
		switch (SensorType)
		{
		case EOpenMobileSensorType::Accelerometer:
		case EOpenMobileSensorType::AccelerometerUncalibrated:
		case EOpenMobileSensorType::Gyroscope:
		case EOpenMobileSensorType::GyroscopeUncalibrated:
		case EOpenMobileSensorType::Magnetometer:
		case EOpenMobileSensorType::MagnetometerUncalibrated:
		case EOpenMobileSensorType::Gravity:
		case EOpenMobileSensorType::LinearAcceleration:
			return ELatestSampleFamily::Vector;
		case EOpenMobileSensorType::Attitude:
			return ELatestSampleFamily::Attitude;
		case EOpenMobileSensorType::MagneticHeading:
		case EOpenMobileSensorType::TrueHeading:
			return ELatestSampleFamily::Heading;
		case EOpenMobileSensorType::BarometricPressure:
		case EOpenMobileSensorType::RelativeAltitude:
		case EOpenMobileSensorType::AbsoluteAltitude:
		case EOpenMobileSensorType::AmbientLight:
			return ELatestSampleFamily::Scalar;
		case EOpenMobileSensorType::StepCounter:
		case EOpenMobileSensorType::StepDetector:
		case EOpenMobileSensorType::Pedometer:
			return ELatestSampleFamily::Steps;
		case EOpenMobileSensorType::MotionActivity:
		case EOpenMobileSensorType::ActivityTransition:
			return ELatestSampleFamily::Activity;
		case EOpenMobileSensorType::PhysicalOrientation:
			return ELatestSampleFamily::Orientation;
		case EOpenMobileSensorType::Proximity:
			return ELatestSampleFamily::Proximity;
		case EOpenMobileSensorType::Unknown:
		default:
			return ELatestSampleFamily::None;
		}
	}

	double GetStaleAfterSeconds(
		const FOpenMobileSensorStreamOptions& Options
	)
	{
		if (!FMath::IsFinite(Options.CustomFrequencyHz)
			|| Options.CustomFrequencyHz <= 0.0)
		{
			return 1.0;
		}
		return FMath::Max(1.0, 3.0 / Options.CustomFrequencyHz);
	}

	void InitializeBufferedStorage(FLatestSlot& Slot)
	{
		switch (Slot.ExpectedFamily)
		{
		case ELatestSampleFamily::Vector:
			Slot.BufferedVector.Initialize(Slot.MaximumPendingSamples);
			break;
		case ELatestSampleFamily::Attitude:
			Slot.BufferedAttitude.Initialize(Slot.MaximumPendingSamples);
			break;
		case ELatestSampleFamily::Scalar:
			Slot.BufferedScalar.Initialize(Slot.MaximumPendingSamples);
			break;
		case ELatestSampleFamily::Heading:
			Slot.BufferedHeading.Initialize(Slot.MaximumPendingSamples);
			break;
		case ELatestSampleFamily::Steps:
			Slot.BufferedSteps.Initialize(Slot.MaximumPendingSamples);
			break;
		case ELatestSampleFamily::Activity:
			Slot.BufferedActivity.Initialize(Slot.MaximumPendingSamples);
			break;
		case ELatestSampleFamily::Orientation:
			Slot.BufferedOrientation.Initialize(Slot.MaximumPendingSamples);
			break;
		case ELatestSampleFamily::Proximity:
			Slot.BufferedProximity.Initialize(Slot.MaximumPendingSamples);
			break;
		case ELatestSampleFamily::None:
		default:
			break;
		}
	}

	void ResizeBufferedStorage(FLatestSlot& Slot)
	{
		switch (Slot.ExpectedFamily)
		{
		case ELatestSampleFamily::Vector:
			Slot.BufferedVector.Resize(
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::Attitude:
			Slot.BufferedAttitude.Resize(
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::Scalar:
			Slot.BufferedScalar.Resize(
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::Heading:
			Slot.BufferedHeading.Resize(
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::Steps:
			Slot.BufferedSteps.Resize(
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::Activity:
			Slot.BufferedActivity.Resize(
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::Orientation:
			Slot.BufferedOrientation.Resize(
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::Proximity:
			Slot.BufferedProximity.Resize(
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::None:
		default:
			break;
		}
	}

	void ClearBufferedStorage(FLatestSlot& Slot)
	{
		Slot.BufferedVector.Clear();
		Slot.BufferedAttitude.Clear();
		Slot.BufferedScalar.Clear();
		Slot.BufferedHeading.Clear();
		Slot.BufferedSteps.Clear();
		Slot.BufferedActivity.Clear();
		Slot.BufferedOrientation.Clear();
		Slot.BufferedProximity.Clear();
	}

	template <typename SampleType>
	void ConfigurePendingEvents(
		TArray<SampleType>& Pending,
		int32 Capacity,
		EOpenMobileSensorOverflowPolicy OverflowPolicy
	)
	{
		if (Pending.Num() > Capacity)
		{
			const int32 RemovedSamples = Pending.Num() - Capacity;
			if (OverflowPolicy == EOpenMobileSensorOverflowPolicy::DropOldest)
			{
				Pending.RemoveAt(0, RemovedSamples, EAllowShrinking::No);
			}
			else
			{
				Pending.RemoveAt(Capacity, RemovedSamples, EAllowShrinking::No);
			}
		}
		Pending.Reserve(Capacity);
	}

	void ConfigureEventStorage(FLatestSlot& Slot)
	{
		switch (Slot.ExpectedFamily)
		{
		case ELatestSampleFamily::Vector:
			ConfigurePendingEvents(
				Slot.PendingVector,
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::Attitude:
			ConfigurePendingEvents(
				Slot.PendingAttitude,
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::Scalar:
			ConfigurePendingEvents(
				Slot.PendingScalar,
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::Heading:
			ConfigurePendingEvents(
				Slot.PendingHeading,
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::Steps:
			ConfigurePendingEvents(
				Slot.PendingSteps,
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::Activity:
			ConfigurePendingEvents(
				Slot.PendingActivity,
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::Orientation:
			ConfigurePendingEvents(
				Slot.PendingOrientation,
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::Proximity:
			ConfigurePendingEvents(
				Slot.PendingProximity,
				Slot.MaximumPendingSamples,
				Slot.OverflowPolicy
			);
			break;
		case ELatestSampleFamily::None:
		default:
			break;
		}
	}

	void ClearPendingEvents(FLatestSlot& Slot)
	{
		Slot.PendingVector.Reset();
		Slot.PendingAttitude.Reset();
		Slot.PendingScalar.Reset();
		Slot.PendingHeading.Reset();
		Slot.PendingSteps.Reset();
		Slot.PendingActivity.Reset();
		Slot.PendingOrientation.Reset();
		Slot.PendingProximity.Reset();
		Slot.EventHighWaterMark = 0;
		Slot.EventDroppedSamples = 0;
		Slot.bHasLastScalarEventValue = false;
	}

	template <typename SampleType>
	bool MeetsEventThreshold(
		const FLatestSlot& Slot,
		const SampleType& Sample
	)
	{
		static_cast<void>(Slot);
		static_cast<void>(Sample);
		return true;
	}

	bool MeetsEventThreshold(
		const FLatestSlot& Slot,
		const FOpenMobileScalarSensorSample& Sample
	)
	{
		return Slot.MinimumScalarEventChange <= 0.0
			|| !Slot.bHasLastScalarEventValue
			|| Sample.Header.bStatefulProcessingReset
			|| FMath::Abs(Sample.Value - Slot.LastScalarEventValue) >=
				Slot.MinimumScalarEventChange;
	}

	template <typename SampleType>
	void RecordAcceptedEventSample(
		FLatestSlot& Slot,
		const SampleType& Sample
	)
	{
		static_cast<void>(Slot);
		static_cast<void>(Sample);
	}

	void RecordAcceptedEventSample(
		FLatestSlot& Slot,
		const FOpenMobileScalarSensorSample& Sample
	)
	{
		Slot.LastScalarEventValue = Sample.Value;
		Slot.bHasLastScalarEventValue = true;
	}

	template <typename SampleType>
	bool EnqueueEventSample(
		FLatestSlot& Slot,
		const SampleType& Sample,
		TArray<SampleType> FLatestSlot::* PendingMember
	)
	{
		if (Slot.DeliveryMode != EOpenMobileSensorDeliveryMode::EventBatches)
		{
			return false;
		}
		if (!FOpenMobileSensorHeadingQuality::MeetsMinimum(
			Sample.Header.Accuracy,
			Slot.MinimumCallbackAccuracy
		))
		{
			return false;
		}
		if (!MeetsEventThreshold(Slot, Sample))
		{
			return false;
		}
		TArray<SampleType>& Pending = Slot.*PendingMember;
		if (Pending.Num() >= Slot.MaximumPendingSamples)
		{
			++Slot.EventDroppedSamples;
			if (Slot.OverflowPolicy ==
				EOpenMobileSensorOverflowPolicy::RejectNewest)
			{
				return false;
			}
			Pending.RemoveAt(0, 1, EAllowShrinking::No);
		}
		Pending.Add(Sample);
		RecordAcceptedEventSample(Slot, Sample);
		Slot.EventHighWaterMark = FMath::Max(
			Slot.EventHighWaterMark,
			Pending.Num()
		);
		return true;
	}

	template <typename SampleType>
	void EnqueueBufferedSample(
		FLatestSlot& Slot,
		const SampleType& Sample,
		TFixedSampleRingBuffer<SampleType> FLatestSlot::* BufferedMember
	)
	{
		if (Slot.DeliveryMode == EOpenMobileSensorDeliveryMode::Buffered)
		{
			(Slot.*BufferedMember).Enqueue(Sample, Slot.OverflowPolicy);
		}
	}

	void NormalizeHeaderAccuracy(FOpenMobileSensorSampleHeader& Header)
	{
		if (Header.bHasEstimatedError
			&& (!FMath::IsFinite(Header.EstimatedError)
				|| Header.EstimatedError < 0.0))
		{
			Header.bHasEstimatedError = false;
		}
	}

	template <typename SampleType>
	void NormalizeSampleAccuracyMetadata(SampleType& Sample)
	{
		NormalizeHeaderAccuracy(Sample.Header);
	}

	void NormalizeSampleAccuracyMetadata(
		FOpenMobileScalarSensorSample& Sample
	)
	{
		if (Sample.Header.Sensor.Type !=
			EOpenMobileSensorType::AbsoluteAltitude)
		{
			NormalizeHeaderAccuracy(Sample.Header);
			return;
		}
		FOpenMobileAbsoluteAltitudeMetadata& Metadata =
			Sample.AbsoluteAltitude;
		if (Metadata.bHasVerticalAccuracy
			&& FMath::IsFinite(Metadata.VerticalAccuracyMeters)
			&& Metadata.VerticalAccuracyMeters >= 0.0)
		{
			Sample.Header.bHasEstimatedError = true;
			Sample.Header.EstimatedError = Metadata.VerticalAccuracyMeters;
		}
		else
		{
			Metadata.bHasVerticalAccuracy = false;
			Metadata.VerticalAccuracyMeters = 0.0;
			Sample.Header.bHasEstimatedError = false;
			Sample.Header.EstimatedError = 0.0;
		}
	}

	void NormalizeSampleAccuracyMetadata(
		FOpenMobileHeadingSensorSample& Sample
	)
	{
		if (Sample.bHasAccuracyDegrees)
		{
			if (FMath::IsFinite(Sample.AccuracyDegrees)
				&& Sample.AccuracyDegrees >= 0.0)
			{
				Sample.Header.bHasEstimatedError = true;
				Sample.Header.EstimatedError = Sample.AccuracyDegrees;
			}
			else
			{
				Sample.bHasAccuracyDegrees = false;
			}
		}
		Sample.Header.bCalibrationRequired |= Sample.bCalibrationRequired;
		NormalizeHeaderAccuracy(Sample.Header);
	}

	template <typename SampleType>
	void SyncSampleAccuracyMetadata(SampleType& Sample)
	{
		static_cast<void>(Sample);
	}

	void SyncSampleAccuracyMetadata(FOpenMobileScalarSensorSample& Sample)
	{
		if (Sample.Header.Sensor.Type !=
			EOpenMobileSensorType::AbsoluteAltitude)
		{
			return;
		}
		Sample.AbsoluteAltitude.bHasVerticalAccuracy =
			Sample.Header.bHasEstimatedError;
		Sample.AbsoluteAltitude.VerticalAccuracyMeters =
			Sample.Header.bHasEstimatedError
				? Sample.Header.EstimatedError
				: 0.0;
	}

	void SyncSampleAccuracyMetadata(FOpenMobileHeadingSensorSample& Sample)
	{
		Sample.bCalibrationRequired =
			Sample.Header.bCalibrationRequired;
		Sample.bHasAccuracyDegrees = Sample.Header.bHasEstimatedError;
		if (Sample.Header.bHasEstimatedError)
		{
			Sample.AccuracyDegrees = Sample.Header.EstimatedError;
		}
	}

	template <typename SampleType>
	void NormalizeFusionMetadata(SampleType& Sample)
	{
		static_cast<void>(Sample);
	}

	void NormalizeFusionMetadata(FOpenMobileAttitudeSensorSample& Sample)
	{
		if (Sample.Header.Fusion.Quality ==
				EOpenMobileSensorFusionQuality::Unknown
			&& Sample.FusionQuality !=
				EOpenMobileSensorFusionQuality::Unknown)
		{
			Sample.Header.Fusion.Quality = Sample.FusionQuality;
		}
		const bool bNativeFused = (Sample.Header.SourceFlags
			& static_cast<int32>(
				EOpenMobileSensorSourceFlags::NativeFused
			)) != 0;
		const bool bDegraded = Sample.Header.bCalibrationRequired
			|| Sample.Header.Accuracy ==
				EOpenMobileSensorAccuracy::Unreliable
			|| Sample.Header.Accuracy == EOpenMobileSensorAccuracy::Low;
		if (bNativeFused
			&& !Sample.Header.Fusion.bHasNativeQualityReport
			&& (Sample.Header.Accuracy !=
					EOpenMobileSensorAccuracy::Unknown
				|| Sample.Header.bCalibrationRequired))
		{
			Sample.Header.Fusion.bHasNativeQualityReport = true;
			Sample.Header.Fusion.NativeQuality = bDegraded
				? EOpenMobileSensorFusionQuality::Degraded
				: EOpenMobileSensorFusionQuality::Nominal;
		}
		if (bDegraded)
		{
			Sample.Header.Fusion.Quality =
				EOpenMobileSensorFusionQuality::Degraded;
		}
		else if (Sample.Header.Fusion.Quality ==
				EOpenMobileSensorFusionQuality::Unknown
			&& Sample.Header.Fusion.bHasNativeQualityReport)
		{
			Sample.Header.Fusion.Quality =
				Sample.Header.Fusion.NativeQuality;
		}
		for (const EOpenMobileSensorType Sensor : Sample.ContributingSensors)
		{
			const int64 SensorMask =
				UOpenMobileSensorQualityLibrary::MakeInputMask(Sensor);
			Sample.Header.Fusion.ExpectedInputMask |= SensorMask;
			Sample.Header.Fusion.ContributingInputMask |= SensorMask;
		}
		Sample.FusionQuality = Sample.Header.Fusion.Quality;
	}

	void NormalizeFusionMetadata(FOpenMobileHeadingSensorSample& Sample)
	{
		FOpenMobileSensorHeadingQuality::Normalize(Sample);
	}

	template <typename SampleType>
	bool ValidateSourceAndFusion(SampleType& Sample)
	{
		NormalizeFusionMetadata(Sample);
		return FOpenMobileSensorSourcePolicy::ValidateSourceFlags(
				Sample.Header.SourceFlags)
			&& FOpenMobileSensorFusionQualityEvaluator::ValidateContext(
				Sample.Header.Fusion);
	}

	void ApplySourceTransition(
		FLatestSlot& Slot,
		FOpenMobileSensorSampleHeader& Header
	)
	{
		const bool bSourceChanged = Slot.bHasSourceState
			&& Slot.LastSourceFlags != Header.SourceFlags;
		Header.bSourceChanged |= bSourceChanged;
		if (bSourceChanged)
		{
			Slot.bPendingStatefulProcessingReset = true;
		}
		Slot.LastSourceFlags = Header.SourceFlags;
		Slot.bHasSourceState = true;
	}

	template <typename SampleType>
	bool ApplySubscriptionFilters(FLatestSlot& Slot, SampleType& Sample)
	{
		static_cast<void>(Slot);
		static_cast<void>(Sample);
		return true;
	}

	bool ApplySubscriptionFilters(
		FLatestSlot& Slot,
		FOpenMobileVectorSensorSample& Sample
	)
	{
		return Slot.VectorFilter.Apply(Slot.FilterOptions, Sample);
	}

	bool ApplySubscriptionFilters(
		FLatestSlot& Slot,
		FOpenMobileActivitySensorSample& Sample
	)
	{
		if (Slot.Sensor.Type == EOpenMobileSensorType::ActivityTransition)
		{
			return Slot.ActivityTransitionFilter.Process(Sample);
		}
		return Slot.ActivityFilter.Process(Sample);
	}

	template <typename SampleType>
	bool ShouldResetAfterFilterRejection(const SampleType& Sample)
	{
		static_cast<void>(Sample);
		return true;
	}

	bool ShouldResetAfterFilterRejection(
		const FOpenMobileActivitySensorSample& Sample
	)
	{
		static_cast<void>(Sample);
		return false;
	}

	template <typename SampleType>
	bool IsDistinctTransitionAtSameTimestamp(
		const FLatestSlot& Slot,
		const SampleType& Sample
	)
	{
		static_cast<void>(Slot);
		static_cast<void>(Sample);
		return false;
	}

	bool IsDistinctTransitionAtSameTimestamp(
		const FLatestSlot& Slot,
		const FOpenMobileActivitySensorSample& Sample
	)
	{
		return Slot.Sensor.Type == EOpenMobileSensorType::ActivityTransition
			&& Sample.Transition != EOpenMobileActivityTransition::None
			&& (Sample.Activity != Slot.Activity.Activity
				|| Sample.Transition != Slot.Activity.Transition);
	}

	bool IsMagneticCalibrationSensor(EOpenMobileSensorType SensorType)
	{
		return SensorType == EOpenMobileSensorType::Magnetometer
			|| SensorType == EOpenMobileSensorType::MagneticHeading
			|| SensorType == EOpenMobileSensorType::TrueHeading;
	}

	FText GetCalibrationGuidance(
		EOpenMobileSensorCalibrationState State,
		EOpenMobileSensorCalibrationReason Reason
	)
	{
		if (State == EOpenMobileSensorCalibrationState::Resolved)
		{
			return NSLOCTEXT(
				"OpenMobileSensors",
				"CalibrationQualityRecovered",
				"Sensor calibration quality has recovered."
			);
		}
		if (Reason ==
			EOpenMobileSensorCalibrationReason::MagneticInterference)
		{
			return NSLOCTEXT(
				"OpenMobileSensors",
				"CalibrationMagneticInterference",
				"Move away from magnetic interference and keep using the sensor."
			);
		}
		return NSLOCTEXT(
			"OpenMobileSensors",
			"CalibrationNativeRequirement",
			"Sensor calibration is needed. Follow your device guidance."
		);
	}

	void QueueCalibrationChange(
		FLatestSlot& Slot,
		EOpenMobileSensorCalibrationState State,
		EOpenMobileSensorCalibrationReason Reason,
		EOpenMobileSensorAccuracy Accuracy,
		double TimestampSeconds
	)
	{
		if (Slot.PendingCalibrationChanges.Num() >=
			MaximumPendingCalibrationChanges)
		{
			Slot.PendingCalibrationChanges.RemoveAt(
				0,
				1,
				EAllowShrinking::No
			);
		}
		FOpenMobileSensorCalibrationEvent& Event =
			Slot.PendingCalibrationChanges.AddDefaulted_GetRef();
		Event.Sensor = Slot.Sensor;
		Event.State = State;
		Event.Reason = Reason;
		Event.Accuracy = Accuracy;
		Event.Guidance = GetCalibrationGuidance(State, Reason);
		Event.TimestampSeconds = TimestampSeconds;
		Event.Sequence = Slot.NextCalibrationSequence++;
	}

	bool ObserveCalibration(
		FLatestSlot& Slot,
		const FOpenMobileSensorAccuracySnapshot& Report
	)
	{
		const bool bNativeRequirement = Report.bCalibrationRequired;
		const bool bMagneticInterference =
			IsMagneticCalibrationSensor(Slot.Sensor.Type)
			&& Report.Accuracy == EOpenMobileSensorAccuracy::Unreliable;
		const bool bNeedsCalibration =
			bNativeRequirement || bMagneticInterference;
		if (!bNeedsCalibration)
		{
			if (!Slot.bCalibrationNeeded)
			{
				return false;
			}
			Slot.bCalibrationNeeded = false;
			if (!Slot.bCalibrationRequiredEventEmitted)
			{
				return false;
			}
			Slot.bCalibrationRequiredEventEmitted = false;
			QueueCalibrationChange(
				Slot,
				EOpenMobileSensorCalibrationState::Resolved,
				EOpenMobileSensorCalibrationReason::QualityRecovered,
				Report.Accuracy,
				Report.TimestampSeconds
			);
			return true;
		}
		if (!Slot.bCalibrationNeeded)
		{
			Slot.bCalibrationNeeded = true;
			Slot.bCalibrationRequiredEventEmitted = false;
		}
		if (Slot.bCalibrationRequiredEventEmitted)
		{
			return false;
		}
		if (Slot.bHasCalibrationRequiredEventTime
			&& Report.TimestampSeconds <
				Slot.LastCalibrationRequiredEventSeconds
					+ CalibrationEventCooldownSeconds)
		{
			return false;
		}
		const EOpenMobileSensorCalibrationReason Reason = bNativeRequirement
			? EOpenMobileSensorCalibrationReason::NativeRequirement
			: EOpenMobileSensorCalibrationReason::MagneticInterference;
		QueueCalibrationChange(
			Slot,
			EOpenMobileSensorCalibrationState::Required,
			Reason,
			Report.Accuracy,
			Report.TimestampSeconds
		);
		Slot.bCalibrationRequiredEventEmitted = true;
		Slot.bHasCalibrationRequiredEventTime = true;
		Slot.LastCalibrationRequiredEventSeconds = Report.TimestampSeconds;
		return true;
	}

	bool ObserveAccuracy(
		FLatestSlot& Slot,
		const FOpenMobileSensorAccuracySnapshot& Report
	)
	{
		if (!FMath::IsFinite(Report.TimestampSeconds)
			|| Report.TimestampSeconds < 0.0
			|| (Slot.bHasAccuracyState
				&& Report.TimestampSeconds <
					Slot.AccuracyState.TimestampSeconds))
		{
			return false;
		}
		FOpenMobileSensorAccuracySnapshot Normalized = Report;
		Normalized.Sensor = Slot.Sensor;
		if (Normalized.bHasEstimatedError
			&& (!FMath::IsFinite(Normalized.EstimatedError)
				|| Normalized.EstimatedError < 0.0))
		{
			Normalized.bHasEstimatedError = false;
		}
		const bool bMaterialChange = !Slot.bHasAccuracyState
			|| Slot.AccuracyState.Accuracy != Normalized.Accuracy
			|| Slot.AccuracyState.bCalibrationRequired !=
				Normalized.bCalibrationRequired;
		if (bMaterialChange)
		{
			Normalized.Sequence = Slot.NextAccuracySequence++;
		}
		else
		{
			Normalized.Sequence = Slot.AccuracyState.Sequence;
		}
		Slot.AccuracyState = Normalized;
		Slot.bHasAccuracyState = true;
		const bool bQueuedCalibration = ObserveCalibration(Slot, Normalized);
		if (!bMaterialChange)
		{
			return bQueuedCalibration;
		}
		if (Slot.PendingAccuracyChanges.Num() >=
			MaximumPendingAccuracyChanges)
		{
			Slot.PendingAccuracyChanges.RemoveAt(
				0,
				1,
				EAllowShrinking::No
			);
		}
		Slot.PendingAccuracyChanges.Add(Normalized);
		return true;
	}

	template <typename SampleType>
	bool HasPerSampleAccuracy(const SampleType& Sample)
	{
		static_cast<void>(Sample);
		return false;
	}

	bool HasPerSampleAccuracy(const FOpenMobileScalarSensorSample& Sample)
	{
		return Sample.Header.Sensor.Type ==
			EOpenMobileSensorType::AbsoluteAltitude;
	}

	template <typename SampleType>
	bool ApplyAccuracyState(FLatestSlot& Slot, SampleType& Sample)
	{
		NormalizeSampleAccuracyMetadata(Sample);
		const bool bHasExplicitAccuracy = HasPerSampleAccuracy(Sample)
			|| !Slot.bHasAccuracyState
			|| Sample.Header.Accuracy != EOpenMobileSensorAccuracy::Unknown
			|| Sample.Header.bCalibrationRequired
			|| Sample.Header.bHasEstimatedError;
		bool bQueuedChange = false;
		if (bHasExplicitAccuracy)
		{
			FOpenMobileSensorAccuracySnapshot Report;
			Report.Sensor = Slot.Sensor;
			Report.Accuracy = Sample.Header.Accuracy;
			Report.bCalibrationRequired =
				Sample.Header.bCalibrationRequired;
			Report.bHasEstimatedError =
				Sample.Header.bHasEstimatedError;
			Report.EstimatedError = Sample.Header.EstimatedError;
			Report.TimestampSeconds = Sample.Header.TimestampSeconds;
			bQueuedChange = ObserveAccuracy(Slot, Report);
		}
		if (Slot.bHasAccuracyState)
		{
			Sample.Header.Accuracy = Slot.AccuracyState.Accuracy;
			Sample.Header.bCalibrationRequired =
				Slot.AccuracyState.bCalibrationRequired;
			Sample.Header.bHasEstimatedError =
				Slot.AccuracyState.bHasEstimatedError;
			if (Slot.AccuracyState.bHasEstimatedError)
			{
				Sample.Header.EstimatedError =
					Slot.AccuracyState.EstimatedError;
			}
		}
		SyncSampleAccuracyMetadata(Sample);
		return bQueuedChange;
	}

	template <typename SampleType>
	bool PrepareSampleForSlot(FLatestSlot& Slot, SampleType& Sample)
	{
		return Slot.Sensor == Sample.Header.Sensor;
	}

	bool PrepareSampleForSlot(
		FLatestSlot& Slot,
		FOpenMobileAttitudeSensorSample& Sample
	)
	{
		if (Slot.Sensor != Sample.Header.Sensor
			|| Slot.AttitudeReferenceFrame != Sample.ReferenceFrame)
		{
			return false;
		}
		Sample.bHasEulerDegrees = (Slot.AttitudeRepresentations
			& static_cast<int32>(
				EOpenMobileAttitudeRepresentation::EulerAngles
			)) != 0;
		Sample.bHasRotationMatrix = (Slot.AttitudeRepresentations
			& static_cast<int32>(
				EOpenMobileAttitudeRepresentation::RotationMatrix
			)) != 0;
		Sample.EulerDegrees = FRotator::ZeroRotator;
		Sample.RotationMatrix = {};
		return true;
	}

	bool PrepareSampleForSlot(
		FLatestSlot& Slot,
		FOpenMobileHeadingSensorSample& Sample
	)
	{
		if (Slot.Sensor == Sample.Header.Sensor)
		{
			if (Slot.Sensor.Type != EOpenMobileSensorType::TrueHeading)
			{
				return true;
			}
			return FOpenMobileSensorsTrueHeadingService::AnnotateNativeHeading(
				Slot.OwnerIdentifier,
				FPlatformTime::Seconds(),
				Sample
			) == EOpenMobileSensorFailureReason::None;
		}
		if (Slot.Sensor.Type != EOpenMobileSensorType::TrueHeading
			|| Sample.Header.Sensor.Type !=
				EOpenMobileSensorType::MagneticHeading)
		{
			return false;
		}
		FOpenMobileHeadingSensorSample Derived;
		if (FOpenMobileSensorsTrueHeadingService::ConvertMagneticHeading(
			Slot.OwnerIdentifier,
			FPlatformTime::Seconds(),
			Sample,
			Derived
		) != EOpenMobileSensorFailureReason::None)
		{
			return false;
		}
		Sample = MoveTemp(Derived);
		return true;
	}

	bool PrepareSampleForSlot(
		FLatestSlot& Slot,
		FOpenMobileScalarSensorSample& Sample
	)
	{
		if (Slot.Sensor.Type != EOpenMobileSensorType::RelativeAltitude)
		{
			return Slot.Sensor == Sample.Header.Sensor;
		}
		if (Sample.Header.Sensor.Type !=
				EOpenMobileSensorType::RelativeAltitude
			&& Sample.Header.Sensor.Type !=
				EOpenMobileSensorType::BarometricPressure)
		{
			return false;
		}
		FOpenMobileScalarSensorSample RelativeAltitude;
		if (!Slot.RelativeAltitudeEstimator.Process(
			Sample,
			Slot.Sensor,
			RelativeAltitude
		))
		{
			Slot.RelativeAltitudeEstimator.Reset();
			return false;
		}
		Sample = MoveTemp(RelativeAltitude);
		return true;
	}

	bool PrepareSampleForSlot(
		FLatestSlot& Slot,
		FOpenMobileStepsSensorSample& Sample
	)
	{
		if (Slot.Sensor.Type == EOpenMobileSensorType::StepDetector)
		{
			FOpenMobileStepsSensorSample Event;
			if (!Slot.StepDetectionTracker.Process(Sample, Event))
			{
				return false;
			}
			Sample = MoveTemp(Event);
			return true;
		}
		if (!Slot.bResettableStepCountSession)
		{
			return Slot.Sensor == Sample.Header.Sensor;
		}
		if (Sample.Header.Sensor.Type != EOpenMobileSensorType::StepCounter)
		{
			return false;
		}
		FOpenMobileStepsSensorSample SessionSample;
		if (!Slot.StepCountSessionTracker.Process(Sample, SessionSample))
		{
			return false;
		}
		Sample = MoveTemp(SessionSample);
		return true;
	}

	bool PrepareSampleForSlot(
		FLatestSlot& Slot,
		FOpenMobileActivitySensorSample& Sample
	)
	{
		if (Slot.Sensor.Type == EOpenMobileSensorType::ActivityTransition)
		{
			return Sample.Transition != EOpenMobileActivityTransition::None
				&& (Sample.Header.Sensor.Type ==
						EOpenMobileSensorType::MotionActivity
					|| Sample.Header.Sensor.Type ==
						EOpenMobileSensorType::ActivityTransition);
		}
		return Slot.Sensor == Sample.Header.Sensor;
	}

	template <typename SampleType>
	void ApplyPostValidationTransforms(FLatestSlot& Slot, SampleType& Sample)
	{
		static_cast<void>(Slot);
		static_cast<void>(Sample);
	}

	void ApplyPostValidationTransforms(
		FLatestSlot& Slot,
		FOpenMobileAttitudeSensorSample& Sample
	)
	{
		Sample.Quaternion.Normalize();
		Slot.LatestCanonicalAttitude = Sample.Quaternion;
		Slot.bHasCanonicalAttitude = true;
		if (Slot.Recenter.bApplied)
		{
			Sample.Quaternion =
				Slot.Recenter.InverseReference * Sample.Quaternion;
			Sample.Quaternion.Normalize();
		}
		if (Slot.CoordinateSpace ==
			EOpenMobileSensorCoordinateSpace::DeviceFixed)
		{
			FOpenMobileSensorCoordinateConverter::
				UpdateEulerAndRotationMatrix(Sample);
		}
	}

	bool PrepareSampleForSlot(
		FLatestSlot& Slot,
		FOpenMobileVectorSensorSample& Sample
	)
	{
		if (Slot.Sensor == Sample.Header.Sensor)
		{
			return true;
		}
		if (Sample.Header.Sensor.Type !=
				EOpenMobileSensorType::Accelerometer
			|| !ValidateSourceAndFusion(Sample))
		{
			ResetDerivedEstimators(Slot);
			return false;
		}
		FOpenMobileVectorSensorSample Derived;
		if (Slot.Sensor.Type == EOpenMobileSensorType::Gravity)
		{
			if (!FOpenMobileSensorValidity::
					IsEligibleForStatefulProcessing(Sample)
				|| !Slot.GravityEstimator.Process(
					Sample,
					Slot.Sensor,
					Derived
				))
			{
				Slot.GravityEstimator.Reset();
				return false;
			}
		}
		else if (Slot.Sensor.Type ==
			EOpenMobileSensorType::LinearAcceleration)
		{
			if (!Slot.LinearAccelerationEstimator.Process(
				Sample,
				Slot.Sensor,
				Derived
			))
			{
				Slot.LinearAccelerationEstimator.Reset();
				return false;
			}
		}
		else
		{
			return false;
		}
		Sample = MoveTemp(Derived);
		return true;
	}

	template <typename SampleType>
	bool PublishSamples(
		const SampleType* Samples,
		int32 SampleCount,
		ELatestSampleFamily Family,
		SampleType FLatestSlot::* Member,
		TArray<SampleType> FLatestSlot::* PendingMember,
		TFixedSampleRingBuffer<SampleType> FLatestSlot::* BufferedMember,
		uint64 RequiredBackendGeneration,
		const FOpenMobileSensorBackendStreamHandle* RequiredPhysicalStream,
		EOpenMobileSensorType RequiredLogicalSensorType =
			EOpenMobileSensorType::Unknown
	)
	{
		if (bShuttingDown.Load()
			|| SampleCount < 0
			|| SampleCount > 4096)
		{
			return false;
		}
		bool bQueuedEvent = false;
		bool bMatchedStream = false;
		{
			FReadScopeLock RegistryLock(SlotsLock);
			for (TPair<
				FOpenMobileSensorSubscriptionHandle,
				TUniquePtr<FLatestSlot>
			>& Pair : Slots)
			{
				FLatestSlot& Slot = *Pair.Value;
				FScopeLock SlotLock(&Slot.Mutex);
				if (Slot.State != EOpenMobileSensorSubscriptionState::Active
					|| Slot.ExpectedFamily != Family
					|| (RequiredLogicalSensorType !=
							EOpenMobileSensorType::Unknown
						&& Slot.Sensor.Type != RequiredLogicalSensorType)
					|| (RequiredBackendGeneration != 0
						&& (Slot.BackendGeneration != RequiredBackendGeneration
							|| !RequiredPhysicalStream
							|| Slot.PhysicalStreamHandle !=
								*RequiredPhysicalStream)))
				{
					continue;
				}
				bMatchedStream = true;
				for (int32 Index = 0; Index < SampleCount; ++Index)
				{
					SampleType Sample = Samples[Index];
					if (Slot.PhysicalSensor != Sample.Header.Sensor)
					{
						continue;
					}
					if (!FMath::IsFinite(Sample.Header.TimestampSeconds)
						|| Sample.Header.TimestampSeconds < 0.0)
					{
						RecordTimestampIssue(
							Slot,
							EOpenMobileSensorTimestampIssue::Invalid
						);
						continue;
					}
					const bool bDistinctTransitionAtSameTimestamp =
						Slot.bHasSample
						&& Sample.Header.TimestampSeconds ==
							Slot.LatestTimestampSeconds
						&& IsDistinctTransitionAtSameTimestamp(
							Slot,
							Sample
						);
					if (Slot.bHasSample
						&& Sample.Header.TimestampSeconds <=
							Slot.LatestTimestampSeconds
						&& !bDistinctTransitionAtSameTimestamp)
					{
						RecordTimestampIssue(
							Slot,
							Sample.Header.TimestampSeconds ==
								Slot.LatestTimestampSeconds
							? EOpenMobileSensorTimestampIssue::Duplicate
							: EOpenMobileSensorTimestampIssue::Backward
						);
						continue;
					}
					bQueuedEvent |= ApplyAccuracyState(Slot, Sample);
					if (!PrepareSampleForSlot(Slot, Sample))
					{
						Slot.bPendingStatefulProcessingReset = true;
						continue;
					}
					if (!ValidateSourceAndFusion(Sample))
					{
						ResetDerivedEstimators(Slot);
						Slot.bPendingStatefulProcessingReset = true;
						continue;
					}
					if (!FOpenMobileSensorValidity::
						IsEligibleForStatefulProcessing(Sample))
					{
						ResetDerivedEstimators(Slot);
						Slot.bPendingStatefulProcessingReset = true;
						continue;
					}
					ApplyPostValidationTransforms(Slot, Sample);
					ApplySourceTransition(Slot, Sample.Header);
					Sample.Header.TimestampIssueFlags =
						Slot.PendingTimestampIssueFlags;
					Sample.Header.bStatefulProcessingReset |=
						Slot.bPendingStatefulProcessingReset
						|| Slot.PendingTimestampIssueFlags != 0;
					UpdateGyroscopeDriftStatistics(Slot, Sample);
					if (!ApplySubscriptionFilters(Slot, Sample))
					{
						if (ShouldResetAfterFilterRejection(Sample))
						{
							Slot.bPendingStatefulProcessingReset = true;
						}
						continue;
					}
					UpdateRateStatistics(
						Slot,
						Sample.Header.TimestampSeconds
					);
					SampleType& Destination = Slot.*Member;
					Destination = Sample;
					Destination.Header.Sensor = Slot.Sensor;
					Destination.Header.Sequence = Slot.NextSequence++;
					Destination.Header.GameThreadReceiptSeconds = 0.0;
					Destination.Header.bHasGameThreadReceiptTime = false;
					Destination.Header.TimestampIssueFlags =
						Sample.Header.TimestampIssueFlags;
					Destination.Header.bStatefulProcessingReset =
						Sample.Header.bStatefulProcessingReset;
					Destination.Header.bUnitsNormalized = true;
					Destination.Header.bCoordinatesNormalized = true;
					Destination.Header.CoordinateSpace =
						EOpenMobileSensorCoordinateSpace::DeviceFixed;
					FOpenMobileSensorsScreenRotationService::ApplyToSample(
						Slot.OwnerIdentifier,
						Slot.CoordinateSpace,
						Destination
					);
					Slot.PendingTimestampIssueFlags = 0;
					Slot.bPendingStatefulProcessingReset = false;
					Slot.LatestTimestampSeconds =
						Sample.Header.TimestampSeconds;
					Slot.Family = Family;
					Slot.bHasSample = true;
					bQueuedEvent |= EnqueueEventSample(
						Slot,
						Destination,
						PendingMember
					);
					EnqueueBufferedSample(Slot, Destination, BufferedMember);
				}
			}
		}
		if (bQueuedEvent)
		{
			EnsureEventTicker();
		}
		return RequiredBackendGeneration == 0 || bMatchedStream;
	}

	bool HasMatchingActivityTransitionSubscription(
		uint64 RequiredBackendGeneration,
		const FOpenMobileSensorBackendStreamHandle* RequiredPhysicalStream
	)
	{
		FReadScopeLock RegistryLock(SlotsLock);
		for (const TPair<
			FOpenMobileSensorSubscriptionHandle,
			TUniquePtr<FLatestSlot>
		>& Pair : Slots)
		{
			FLatestSlot& Slot = *Pair.Value;
			FScopeLock SlotLock(&Slot.Mutex);
			if (Slot.State == EOpenMobileSensorSubscriptionState::Active
				&& Slot.Sensor.Type ==
					EOpenMobileSensorType::ActivityTransition
				&& Slot.PhysicalSensor.Type ==
					EOpenMobileSensorType::MotionActivity
				&& (RequiredBackendGeneration == 0
					|| (RequiredPhysicalStream
						&& Slot.BackendGeneration ==
							RequiredBackendGeneration
						&& Slot.PhysicalStreamHandle ==
							*RequiredPhysicalStream)))
			{
				return true;
			}
		}
		return false;
	}

	bool BuildDerivedActivityTransitions(
		const FOpenMobileActivitySensorSample* Samples,
		int32 SampleCount,
		uint64 RequiredBackendGeneration,
		const FOpenMobileSensorBackendStreamHandle* RequiredPhysicalStream,
		FOpenMobileActivitySensorBatch& OutBatch
	)
	{
		OutBatch.Samples.Reset();
		if (!HasMatchingActivityTransitionSubscription(
			RequiredBackendGeneration,
			RequiredPhysicalStream
		))
		{
			return false;
		}
		FScopeLock Lock(&ActivityTransitionTrackersMutex);
		FOpenMobileActivityTransitionTracker* Tracker = nullptr;
		if (RequiredBackendGeneration == 0)
		{
			if (!bDirectActivityTransitionTrackerConfigured)
			{
				DirectActivityTransitionTracker.Configure(0.25);
				bDirectActivityTransitionTrackerConfigured = true;
			}
			Tracker = &DirectActivityTransitionTracker;
		}
		else if (RequiredPhysicalStream)
		{
			FBackendActivityTransitionTrackerState& State =
				BackendActivityTransitionTrackers.FindOrAdd(
					RequiredPhysicalStream->Identifier
				);
			if (State.BackendGeneration != RequiredBackendGeneration)
			{
				State.BackendGeneration = RequiredBackendGeneration;
				State.Tracker.Configure(0.25);
			}
			Tracker = &State.Tracker;
		}
		if (!Tracker)
		{
			return false;
		}
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			if (Samples[Index].Header.Sensor.Type !=
				EOpenMobileSensorType::MotionActivity)
			{
				continue;
			}
			FOpenMobileActivitySensorBatch Derived;
			if (Tracker->Process(Samples[Index], Derived))
			{
				OutBatch.Samples.Append(MoveTemp(Derived.Samples));
			}
		}
		return !OutBatch.Samples.IsEmpty();
	}

	void ResetActivityTransitionTrackers()
	{
		FScopeLock Lock(&ActivityTransitionTrackersMutex);
		BackendActivityTransitionTrackers.Reset();
		DirectActivityTransitionTracker.Reset();
		bDirectActivityTransitionTrackerConfigured = false;
	}

	bool PublishActivitySamples(
		const FOpenMobileActivitySensorSample* Samples,
		int32 SampleCount,
		uint64 RequiredBackendGeneration,
		const FOpenMobileSensorBackendStreamHandle* RequiredPhysicalStream
	)
	{
		if (bShuttingDown.Load()
			|| SampleCount < 0
			|| SampleCount > 4096)
		{
			return false;
		}
		TArray<FOpenMobileActivitySensorSample> Classifications;
		TArray<FOpenMobileActivitySensorSample> NativeTransitions;
		Classifications.Reserve(SampleCount);
		NativeTransitions.Reserve(SampleCount);
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			if (Samples[Index].Header.Sensor.Type ==
				EOpenMobileSensorType::MotionActivity)
			{
				Classifications.Add(Samples[Index]);
			}
			else if (Samples[Index].Header.Sensor.Type ==
				EOpenMobileSensorType::ActivityTransition)
			{
				NativeTransitions.Add(Samples[Index]);
			}
		}
		bool bPublished = false;
		if (!Classifications.IsEmpty())
		{
			bPublished |= PublishSamples(
				Classifications.GetData(),
				Classifications.Num(),
				ELatestSampleFamily::Activity,
				&FLatestSlot::Activity,
				&FLatestSlot::PendingActivity,
				&FLatestSlot::BufferedActivity,
				RequiredBackendGeneration,
				RequiredPhysicalStream,
				EOpenMobileSensorType::MotionActivity
			);
			FOpenMobileActivitySensorBatch Derived;
			if (BuildDerivedActivityTransitions(
				Classifications.GetData(),
				Classifications.Num(),
				RequiredBackendGeneration,
				RequiredPhysicalStream,
				Derived
			))
			{
				bPublished |= PublishSamples(
					Derived.Samples.GetData(),
					Derived.Samples.Num(),
					ELatestSampleFamily::Activity,
					&FLatestSlot::Activity,
					&FLatestSlot::PendingActivity,
					&FLatestSlot::BufferedActivity,
					RequiredBackendGeneration,
					RequiredPhysicalStream,
					EOpenMobileSensorType::ActivityTransition
				);
			}
		}
		if (!NativeTransitions.IsEmpty())
		{
			bPublished |= PublishSamples(
				NativeTransitions.GetData(),
				NativeTransitions.Num(),
				ELatestSampleFamily::Activity,
				&FLatestSlot::Activity,
				&FLatestSlot::PendingActivity,
				&FLatestSlot::BufferedActivity,
				RequiredBackendGeneration,
				RequiredPhysicalStream,
				EOpenMobileSensorType::ActivityTransition
			);
		}
		return bPublished;
	}

	bool PublishDerivedOrientationSamples(
		const FOpenMobileVectorSensorSample* Samples,
		int32 SampleCount,
		uint64 RequiredBackendGeneration,
		const FOpenMobileSensorBackendStreamHandle* RequiredPhysicalStream
	)
	{
		if (bShuttingDown.Load()
			|| SampleCount < 0
			|| SampleCount > 4096)
		{
			return false;
		}
		bool bQueuedEvent = false;
		bool bMatchedStream = false;
		{
			FReadScopeLock RegistryLock(SlotsLock);
			for (TPair<
				FOpenMobileSensorSubscriptionHandle,
				TUniquePtr<FLatestSlot>
			>& Pair : Slots)
			{
				FLatestSlot& Slot = *Pair.Value;
				FScopeLock SlotLock(&Slot.Mutex);
				if (Slot.State != EOpenMobileSensorSubscriptionState::Active
					|| Slot.ExpectedFamily !=
						ELatestSampleFamily::Orientation
					|| Slot.Sensor.Type !=
						EOpenMobileSensorType::PhysicalOrientation
					|| (RequiredBackendGeneration != 0
						&& (Slot.BackendGeneration !=
								RequiredBackendGeneration
							|| !RequiredPhysicalStream
							|| Slot.PhysicalStreamHandle !=
								*RequiredPhysicalStream)))
				{
					continue;
				}
				bMatchedStream = true;
				for (int32 Index = 0; Index < SampleCount; ++Index)
				{
					FOpenMobileVectorSensorSample Sample = Samples[Index];
					if (Slot.PhysicalSensor != Sample.Header.Sensor)
					{
						continue;
					}
					if (!FMath::IsFinite(Sample.Header.TimestampSeconds)
						|| Sample.Header.TimestampSeconds < 0.0)
					{
						RecordTimestampIssue(
							Slot,
							EOpenMobileSensorTimestampIssue::Invalid
						);
						continue;
					}
					if (Slot.bHasSample
						&& Sample.Header.TimestampSeconds <=
							Slot.LatestTimestampSeconds)
					{
						RecordTimestampIssue(
							Slot,
							Sample.Header.TimestampSeconds ==
								Slot.LatestTimestampSeconds
							? EOpenMobileSensorTimestampIssue::Duplicate
							: EOpenMobileSensorTimestampIssue::Backward
						);
						continue;
					}
					bQueuedEvent |= ApplyAccuracyState(Slot, Sample);
					if (!ValidateSourceAndFusion(Sample)
						|| !FOpenMobileSensorValidity::
							IsEligibleForStatefulProcessing(Sample))
					{
						ResetDerivedEstimators(Slot);
						Slot.bPendingStatefulProcessingReset = true;
						continue;
					}
					FOpenMobileVectorSensorSample Gravity = Sample;
					if (Sample.Header.Sensor.Type ==
						EOpenMobileSensorType::Accelerometer)
					{
						FOpenMobileSensorIdentifier GravitySensor;
						GravitySensor.Type = EOpenMobileSensorType::Gravity;
						GravitySensor.InstanceId = Slot.Sensor.InstanceId;
						if (!Slot.GravityEstimator.Process(
							Sample,
							GravitySensor,
							Gravity
						))
						{
							ResetDerivedEstimators(Slot);
							Slot.bPendingStatefulProcessingReset = true;
							continue;
						}
					}
					else if (Sample.Header.Sensor.Type !=
						EOpenMobileSensorType::Gravity)
					{
						continue;
					}
					FOpenMobileOrientationSensorSample Derived;
					if (!Slot.OrientationClassifier.Process(
						Gravity.Value,
						Gravity.Header.TimestampSeconds,
						Slot.OrientationConfig,
						Derived
					))
					{
						ResetDerivedEstimators(Slot);
						Slot.bPendingStatefulProcessingReset = true;
						continue;
					}
					const bool bClassifierReset =
						Derived.Header.bStatefulProcessingReset;
					Derived.Header = Gravity.Header;
					Derived.Header.Sensor = Slot.Sensor;
					constexpr int32 OverlayFlags =
						static_cast<int32>(
							EOpenMobileSensorSourceFlags::Mock
						)
						| static_cast<int32>(
							EOpenMobileSensorSourceFlags::Replay
						);
					Derived.Header.SourceFlags = static_cast<int32>(
						EOpenMobileSensorSourceFlags::PluginDerived
					) | (Gravity.Header.SourceFlags & OverlayFlags);
					Derived.Header.bStatefulProcessingReset |=
						bClassifierReset
						|| Slot.bPendingStatefulProcessingReset;
					Derived.Header.TimestampIssueFlags =
						Slot.PendingTimestampIssueFlags;
					Derived.Header.bUnitsNormalized = true;
					Derived.Header.bCoordinatesNormalized = true;
					Derived.Header.CoordinateSpace =
						EOpenMobileSensorCoordinateSpace::DeviceFixed;
					ApplySourceTransition(Slot, Derived.Header);
					UpdateRateStatistics(
						Slot,
						Derived.Header.TimestampSeconds
					);
					Slot.Orientation = Derived;
					Slot.Orientation.Header.Sequence = Slot.NextSequence++;
					Slot.Orientation.Header.GameThreadReceiptSeconds = 0.0;
					Slot.Orientation.Header.bHasGameThreadReceiptTime = false;
					FOpenMobileSensorsScreenRotationService::ApplyToSample(
						Slot.OwnerIdentifier,
						Slot.CoordinateSpace,
						Slot.Orientation
					);
					Slot.PendingTimestampIssueFlags = 0;
					Slot.bPendingStatefulProcessingReset = false;
					Slot.LatestTimestampSeconds =
						Derived.Header.TimestampSeconds;
					Slot.Family = ELatestSampleFamily::Orientation;
					Slot.bHasSample = true;
					bQueuedEvent |= EnqueueEventSample(
						Slot,
						Slot.Orientation,
						&FLatestSlot::PendingOrientation
					);
					EnqueueBufferedSample(
						Slot,
						Slot.Orientation,
						&FLatestSlot::BufferedOrientation
					);
				}
			}
		}
		if (bQueuedEvent)
		{
			EnsureEventTicker();
		}
		return RequiredBackendGeneration == 0 || bMatchedStream;
	}

	bool PublishAccuracySnapshot(
		const FOpenMobileSensorAccuracySnapshot& Snapshot,
		uint64 RequiredBackendGeneration,
		const FOpenMobileSensorBackendStreamHandle* RequiredPhysicalStream
	)
	{
		if (bShuttingDown.Load()
			|| !Snapshot.Sensor.IsValid()
			|| !FMath::IsFinite(Snapshot.TimestampSeconds)
			|| Snapshot.TimestampSeconds < 0.0)
		{
			return false;
		}
		bool bMatchedStream = false;
		bool bQueuedEvent = false;
		{
			FReadScopeLock RegistryLock(SlotsLock);
			for (TPair<
				FOpenMobileSensorSubscriptionHandle,
				TUniquePtr<FLatestSlot>
			>& Pair : Slots)
			{
				FLatestSlot& Slot = *Pair.Value;
				FScopeLock SlotLock(&Slot.Mutex);
				const bool bMatchesSensor = Slot.Sensor == Snapshot.Sensor
					|| (RequiredBackendGeneration != 0
						&& Slot.PhysicalSensor == Snapshot.Sensor);
				if (Slot.State != EOpenMobileSensorSubscriptionState::Active
					|| !bMatchesSensor
					|| (RequiredBackendGeneration != 0
						&& (Slot.BackendGeneration !=
								RequiredBackendGeneration
							|| !RequiredPhysicalStream
							|| Slot.PhysicalStreamHandle !=
								*RequiredPhysicalStream)))
				{
					continue;
				}
				bMatchedStream = true;
				bQueuedEvent |= ObserveAccuracy(Slot, Snapshot);
			}
		}
		if (bQueuedEvent)
		{
			EnsureEventTicker();
		}
		return bMatchedStream;
	}

	EOpenMobileSensorReadStatus GetUnavailableStatus(
		EOpenMobileSensorSubscriptionState State
	)
	{
		switch (State)
		{
		case EOpenMobileSensorSubscriptionState::Paused:
			return EOpenMobileSensorReadStatus::Paused;
		case EOpenMobileSensorSubscriptionState::Stopping:
		case EOpenMobileSensorSubscriptionState::Stopped:
		case EOpenMobileSensorSubscriptionState::Failed:
		case EOpenMobileSensorSubscriptionState::Invalid:
			return EOpenMobileSensorReadStatus::Stopped;
		case EOpenMobileSensorSubscriptionState::Accepted:
		case EOpenMobileSensorSubscriptionState::Starting:
		case EOpenMobileSensorSubscriptionState::Active:
		default:
			return EOpenMobileSensorReadStatus::NoSample;
		}
	}

	template <typename SampleType>
	bool ReadLatestSample(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int64 LastSeenSequence,
		double NowSeconds,
		FOpenMobileSensorReadResult& OutResult,
		SampleType& OutSample,
		ELatestSampleFamily Family,
		SampleType FLatestSlot::* Member
	)
	{
		OutResult = {};
		OutSample = {};
		FReadScopeLock RegistryLock(SlotsLock);
		const TUniquePtr<FLatestSlot>* SlotPointer = Slots.Find(Handle);
		if (!OwnerIdentifier.IsValid()
			|| !SlotPointer
			|| (*SlotPointer)->OwnerIdentifier != OwnerIdentifier)
		{
			OutResult.Status = EOpenMobileSensorReadStatus::InvalidHandle;
			OutResult.Error = FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::InvalidHandle
			).Error;
			return false;
		}

		FLatestSlot& Slot = **SlotPointer;
		FScopeLock SlotLock(&Slot.Mutex);
		if (Slot.State == EOpenMobileSensorSubscriptionState::Stopping
			|| Slot.State == EOpenMobileSensorSubscriptionState::Stopped
			|| Slot.State == EOpenMobileSensorSubscriptionState::Failed)
		{
			OutResult.Status = EOpenMobileSensorReadStatus::Stopped;
			return false;
		}
		if (!Slot.bHasSample || Slot.Family != Family)
		{
			OutResult.Status = GetUnavailableStatus(Slot.State);
			return false;
		}

		OutSample = Slot.*Member;
		const FOpenMobileSensorSampleHeader& Header = OutSample.Header;
		const double SafeNowSeconds = FMath::IsFinite(NowSeconds)
			? NowSeconds
			: Header.TimestampSeconds;
		OutResult.SampleAgeSeconds = FMath::Max(
			0.0,
			SafeNowSeconds - Header.TimestampSeconds
		);
		OutResult.Sequence = Header.Sequence;
		OutResult.bHasNewerSample = Header.Sequence > LastSeenSequence;
		OutResult.bSampleValid = Header.bValid;
		OutResult.Accuracy = Header.Accuracy;
		OutResult.SourceFlags = Header.SourceFlags;
		if (Slot.State == EOpenMobileSensorSubscriptionState::Paused)
		{
			OutResult.Status = EOpenMobileSensorReadStatus::Paused;
		}
		else if (OutResult.SampleAgeSeconds > Slot.StaleAfterSeconds)
		{
			OutResult.Status = EOpenMobileSensorReadStatus::Stale;
		}
		else
		{
			OutResult.Status = EOpenMobileSensorReadStatus::Valid;
		}
		return true;
	}

	template <typename SampleType, typename BatchType>
	bool DrainBufferedSamples(
		const FGuid& OwnerIdentifier,
		const FOpenMobileSensorSubscriptionHandle& Handle,
		int32 MaximumSamples,
		FOpenMobileSensorBufferReadResult& OutResult,
		BatchType& OutBatch,
		ELatestSampleFamily ExpectedFamily,
		TFixedSampleRingBuffer<SampleType> FLatestSlot::* BufferedMember
	)
	{
		OutResult = {};
		OutBatch = {};
		if (MaximumSamples < 1 || MaximumSamples > 4096)
		{
			OutResult.Operation = FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::InvalidRequest
			);
			return false;
		}
		FReadScopeLock RegistryLock(SlotsLock);
		const TUniquePtr<FLatestSlot>* SlotPointer = Slots.Find(Handle);
		if (!OwnerIdentifier.IsValid() || !Handle.IsValid())
		{
			OutResult.Operation = FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::InvalidHandle
			);
			return false;
		}
		if (!SlotPointer || (*SlotPointer)->OwnerIdentifier != OwnerIdentifier)
		{
			OutResult.Operation = FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::StaleHandle
			);
			return false;
		}
		FLatestSlot& Slot = **SlotPointer;
		FScopeLock SlotLock(&Slot.Mutex);
		if (Slot.DeliveryMode != EOpenMobileSensorDeliveryMode::Buffered
			|| Slot.ExpectedFamily != ExpectedFamily)
		{
			OutResult.Operation = FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::InvalidRequest
			);
			return false;
		}
		TFixedSampleRingBuffer<SampleType>& Buffer = Slot.*BufferedMember;
		Buffer.Drain(MaximumSamples, OutBatch.Samples);
		OutResult.Operation.Code = EOpenMobileSensorResultCode::Success;
		OutResult.ReturnedSamples = OutBatch.Samples.Num();
		OutResult.DroppedSamples = Buffer.DroppedSamples;
		OutResult.BufferHighWaterMark = Buffer.HighWaterMark;
		return true;
	}

	template <typename BatchType>
	struct TEventDelivery
	{
		FGuid OwnerIdentifier;
		FOpenMobileSensorSubscriptionHandle Handle;
		BatchType Batch;
	};

	struct FAccuracyDelivery
	{
		FGuid OwnerIdentifier;
		FOpenMobileSensorSubscriptionHandle Handle;
		FOpenMobileSensorAccuracySnapshot Snapshot;
	};

	struct FCalibrationDelivery
	{
		FGuid OwnerIdentifier;
		FOpenMobileSensorSubscriptionHandle Handle;
		FOpenMobileSensorCalibrationEvent Event;
	};

	bool HasPendingSamples(const FLatestSlot& Slot)
	{
		return !Slot.PendingVector.IsEmpty()
			|| !Slot.PendingAttitude.IsEmpty()
			|| !Slot.PendingScalar.IsEmpty()
			|| !Slot.PendingHeading.IsEmpty()
			|| !Slot.PendingSteps.IsEmpty()
			|| !Slot.PendingActivity.IsEmpty()
			|| !Slot.PendingOrientation.IsEmpty()
			|| !Slot.PendingProximity.IsEmpty();
	}

	int32 GetPluginSampleCount(const FLatestSlot& Slot)
	{
		if (Slot.DeliveryMode == EOpenMobileSensorDeliveryMode::EventBatches)
		{
			return Slot.PendingVector.Num()
				+ Slot.PendingAttitude.Num()
				+ Slot.PendingScalar.Num()
				+ Slot.PendingHeading.Num()
				+ Slot.PendingSteps.Num()
				+ Slot.PendingActivity.Num()
				+ Slot.PendingOrientation.Num()
				+ Slot.PendingProximity.Num();
		}
		if (Slot.DeliveryMode == EOpenMobileSensorDeliveryMode::Buffered)
		{
			return Slot.BufferedVector.SampleCount
				+ Slot.BufferedAttitude.SampleCount
				+ Slot.BufferedScalar.SampleCount
				+ Slot.BufferedHeading.SampleCount
				+ Slot.BufferedSteps.SampleCount
				+ Slot.BufferedActivity.SampleCount
				+ Slot.BufferedOrientation.SampleCount
				+ Slot.BufferedProximity.SampleCount;
		}
		return 0;
	}

	int32 GetPluginHighWaterMark(const FLatestSlot& Slot)
	{
		if (Slot.DeliveryMode == EOpenMobileSensorDeliveryMode::EventBatches)
		{
			return Slot.EventHighWaterMark;
		}
		if (Slot.DeliveryMode != EOpenMobileSensorDeliveryMode::Buffered)
		{
			return 0;
		}
		switch (Slot.ExpectedFamily)
		{
		case ELatestSampleFamily::Vector:
			return Slot.BufferedVector.HighWaterMark;
		case ELatestSampleFamily::Attitude:
			return Slot.BufferedAttitude.HighWaterMark;
		case ELatestSampleFamily::Scalar:
			return Slot.BufferedScalar.HighWaterMark;
		case ELatestSampleFamily::Heading:
			return Slot.BufferedHeading.HighWaterMark;
		case ELatestSampleFamily::Steps:
			return Slot.BufferedSteps.HighWaterMark;
		case ELatestSampleFamily::Activity:
			return Slot.BufferedActivity.HighWaterMark;
		case ELatestSampleFamily::Orientation:
			return Slot.BufferedOrientation.HighWaterMark;
		case ELatestSampleFamily::Proximity:
			return Slot.BufferedProximity.HighWaterMark;
		case ELatestSampleFamily::None:
		default:
			return 0;
		}
	}

	int64 GetPluginDroppedSamples(const FLatestSlot& Slot)
	{
		if (Slot.DeliveryMode == EOpenMobileSensorDeliveryMode::EventBatches)
		{
			return Slot.EventDroppedSamples;
		}
		if (Slot.DeliveryMode != EOpenMobileSensorDeliveryMode::Buffered)
		{
			return 0;
		}
		switch (Slot.ExpectedFamily)
		{
		case ELatestSampleFamily::Vector:
			return Slot.BufferedVector.DroppedSamples;
		case ELatestSampleFamily::Attitude:
			return Slot.BufferedAttitude.DroppedSamples;
		case ELatestSampleFamily::Scalar:
			return Slot.BufferedScalar.DroppedSamples;
		case ELatestSampleFamily::Heading:
			return Slot.BufferedHeading.DroppedSamples;
		case ELatestSampleFamily::Steps:
			return Slot.BufferedSteps.DroppedSamples;
		case ELatestSampleFamily::Activity:
			return Slot.BufferedActivity.DroppedSamples;
		case ELatestSampleFamily::Orientation:
			return Slot.BufferedOrientation.DroppedSamples;
		case ELatestSampleFamily::Proximity:
			return Slot.BufferedProximity.DroppedSamples;
		case ELatestSampleFamily::None:
		default:
			return 0;
		}
	}

	double GetOldestPluginTimestampSeconds(const FLatestSlot& Slot)
	{
		if (Slot.DeliveryMode == EOpenMobileSensorDeliveryMode::EventBatches)
		{
			switch (Slot.ExpectedFamily)
			{
			case ELatestSampleFamily::Vector:
				return Slot.PendingVector.IsEmpty()
					? 0.0 : Slot.PendingVector[0].Header.TimestampSeconds;
			case ELatestSampleFamily::Attitude:
				return Slot.PendingAttitude.IsEmpty()
					? 0.0 : Slot.PendingAttitude[0].Header.TimestampSeconds;
			case ELatestSampleFamily::Scalar:
				return Slot.PendingScalar.IsEmpty()
					? 0.0 : Slot.PendingScalar[0].Header.TimestampSeconds;
			case ELatestSampleFamily::Heading:
				return Slot.PendingHeading.IsEmpty()
					? 0.0 : Slot.PendingHeading[0].Header.TimestampSeconds;
			case ELatestSampleFamily::Steps:
				return Slot.PendingSteps.IsEmpty()
					? 0.0 : Slot.PendingSteps[0].Header.TimestampSeconds;
			case ELatestSampleFamily::Activity:
				return Slot.PendingActivity.IsEmpty()
					? 0.0 : Slot.PendingActivity[0].Header.TimestampSeconds;
			case ELatestSampleFamily::Orientation:
				return Slot.PendingOrientation.IsEmpty()
					? 0.0 : Slot.PendingOrientation[0].Header.TimestampSeconds;
			case ELatestSampleFamily::Proximity:
				return Slot.PendingProximity.IsEmpty()
					? 0.0 : Slot.PendingProximity[0].Header.TimestampSeconds;
			case ELatestSampleFamily::None:
			default:
				return 0.0;
			}
		}
		if (Slot.DeliveryMode != EOpenMobileSensorDeliveryMode::Buffered)
		{
			return 0.0;
		}
		switch (Slot.ExpectedFamily)
		{
		case ELatestSampleFamily::Vector:
			return Slot.BufferedVector.GetOldestTimestampSeconds();
		case ELatestSampleFamily::Attitude:
			return Slot.BufferedAttitude.GetOldestTimestampSeconds();
		case ELatestSampleFamily::Scalar:
			return Slot.BufferedScalar.GetOldestTimestampSeconds();
		case ELatestSampleFamily::Heading:
			return Slot.BufferedHeading.GetOldestTimestampSeconds();
		case ELatestSampleFamily::Steps:
			return Slot.BufferedSteps.GetOldestTimestampSeconds();
		case ELatestSampleFamily::Activity:
			return Slot.BufferedActivity.GetOldestTimestampSeconds();
		case ELatestSampleFamily::Orientation:
			return Slot.BufferedOrientation.GetOldestTimestampSeconds();
		case ELatestSampleFamily::Proximity:
			return Slot.BufferedProximity.GetOldestTimestampSeconds();
		case ELatestSampleFamily::None:
		default:
			return 0.0;
		}
	}

	template <typename SampleType, typename BatchType>
	void GatherDelivery(
		FLatestSlot& Slot,
		TArray<SampleType> FLatestSlot::* PendingMember,
		double ReceiptTimeSeconds,
		TArray<TEventDelivery<BatchType>>& OutDeliveries
	)
	{
		TArray<SampleType>& Pending = Slot.*PendingMember;
		if (Pending.IsEmpty())
		{
			return;
		}
		TEventDelivery<BatchType>& Delivery = OutDeliveries.AddDefaulted_GetRef();
		Delivery.OwnerIdentifier = Slot.OwnerIdentifier;
		Delivery.Handle = Slot.Handle;
		Delivery.Batch.Samples = MoveTemp(Pending);
		for (SampleType& Sample : Delivery.Batch.Samples)
		{
			Sample.Header.GameThreadReceiptSeconds = ReceiptTimeSeconds;
			Sample.Header.bHasGameThreadReceiptTime = true;
		}
		Pending.Reset();
	}

	bool HasPendingEvents()
	{
		FReadScopeLock RegistryLock(SlotsLock);
		for (const TPair<
			FOpenMobileSensorSubscriptionHandle,
			TUniquePtr<FLatestSlot>
		>& Pair : Slots)
		{
			FLatestSlot& Slot = *Pair.Value;
			FScopeLock SlotLock(&Slot.Mutex);
			if (Slot.State == EOpenMobileSensorSubscriptionState::Active
				&& (!Slot.PendingAccuracyChanges.IsEmpty()
					|| !Slot.PendingCalibrationChanges.IsEmpty()
					|| (Slot.DeliveryMode ==
							EOpenMobileSensorDeliveryMode::EventBatches
						&& HasPendingSamples(Slot))))
			{
				return true;
			}
		}
		return false;
	}

	void DrainPendingEvents(double NowSeconds)
	{
		check(IsInGameThread());
#if !UE_BUILD_SHIPPING
		const double ProcessingStartSeconds = FPlatformTime::Seconds();
#endif
		const double SafeNowSeconds = FMath::IsFinite(NowSeconds)
			? NowSeconds
			: FPlatformTime::Seconds();
		TArray<TEventDelivery<FOpenMobileVectorSensorBatch>> VectorDeliveries;
		TArray<TEventDelivery<FOpenMobileAttitudeSensorBatch>>
			AttitudeDeliveries;
		TArray<TEventDelivery<FOpenMobileScalarSensorBatch>> ScalarDeliveries;
		TArray<TEventDelivery<FOpenMobileHeadingSensorBatch>> HeadingDeliveries;
		TArray<TEventDelivery<FOpenMobileStepsSensorBatch>> StepsDeliveries;
		TArray<TEventDelivery<FOpenMobileActivitySensorBatch>> ActivityDeliveries;
		TArray<TEventDelivery<FOpenMobileOrientationSensorBatch>>
			OrientationDeliveries;
		TArray<TEventDelivery<FOpenMobileProximitySensorBatch>>
			ProximityDeliveries;
		TArray<FAccuracyDelivery> AccuracyDeliveries;
		TArray<FCalibrationDelivery> CalibrationDeliveries;
		{
			FReadScopeLock RegistryLock(SlotsLock);
			for (TPair<
				FOpenMobileSensorSubscriptionHandle,
				TUniquePtr<FLatestSlot>
			>& Pair : Slots)
			{
				FLatestSlot& Slot = *Pair.Value;
				FScopeLock SlotLock(&Slot.Mutex);
				if (Slot.State != EOpenMobileSensorSubscriptionState::Active)
				{
					continue;
				}
				for (FOpenMobileSensorAccuracySnapshot& Snapshot
					: Slot.PendingAccuracyChanges)
				{
					FAccuracyDelivery& Delivery =
						AccuracyDeliveries.AddDefaulted_GetRef();
					Delivery.OwnerIdentifier = Slot.OwnerIdentifier;
					Delivery.Handle = Slot.Handle;
					Delivery.Snapshot = MoveTemp(Snapshot);
				}
				Slot.PendingAccuracyChanges.Reset();
				for (FOpenMobileSensorCalibrationEvent& Event
					: Slot.PendingCalibrationChanges)
				{
					FCalibrationDelivery& Delivery =
						CalibrationDeliveries.AddDefaulted_GetRef();
					Delivery.OwnerIdentifier = Slot.OwnerIdentifier;
					Delivery.Handle = Slot.Handle;
					Delivery.Event = MoveTemp(Event);
				}
				Slot.PendingCalibrationChanges.Reset();
				if (Slot.DeliveryMode !=
						EOpenMobileSensorDeliveryMode::EventBatches
					|| !HasPendingSamples(Slot))
				{
					continue;
				}
				const double CallbackIntervalSeconds =
					1.0 / Slot.MaximumCallbackFrequencyHz;
				if (Slot.bHasCallbackTime
					&& SafeNowSeconds + 1.e-9 <
						Slot.LastCallbackTimeSeconds
							+ CallbackIntervalSeconds)
				{
					continue;
				}
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingVector,
					SafeNowSeconds,
					VectorDeliveries
				);
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingAttitude,
					SafeNowSeconds,
					AttitudeDeliveries
				);
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingScalar,
					SafeNowSeconds,
					ScalarDeliveries
				);
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingHeading,
					SafeNowSeconds,
					HeadingDeliveries
				);
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingSteps,
					SafeNowSeconds,
					StepsDeliveries
				);
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingActivity,
					SafeNowSeconds,
					ActivityDeliveries
				);
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingOrientation,
					SafeNowSeconds,
					OrientationDeliveries
				);
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingProximity,
					SafeNowSeconds,
					ProximityDeliveries
				);
				Slot.bHasCallbackTime = true;
				Slot.LastCallbackTimeSeconds = SafeNowSeconds;
			}
		}

		for (const FAccuracyDelivery& Delivery : AccuracyDeliveries)
		{
			AccuracyChangedEvent.Broadcast(
				Delivery.OwnerIdentifier,
				Delivery.Handle,
				Delivery.Snapshot
			);
		}
		for (const FCalibrationDelivery& Delivery : CalibrationDeliveries)
		{
			CalibrationChangedEvent.Broadcast(
				Delivery.OwnerIdentifier,
				Delivery.Handle,
				Delivery.Event
			);
		}
		for (const TEventDelivery<FOpenMobileVectorSensorBatch>& Delivery
			: VectorDeliveries)
		{
			VectorBatchEvent.Broadcast(
				Delivery.OwnerIdentifier,
				Delivery.Handle,
				Delivery.Batch
			);
		}
		for (const TEventDelivery<FOpenMobileAttitudeSensorBatch>& Delivery
			: AttitudeDeliveries)
		{
			AttitudeBatchEvent.Broadcast(
				Delivery.OwnerIdentifier,
				Delivery.Handle,
				Delivery.Batch
			);
		}
		for (const TEventDelivery<FOpenMobileScalarSensorBatch>& Delivery
			: ScalarDeliveries)
		{
			ScalarBatchEvent.Broadcast(
				Delivery.OwnerIdentifier,
				Delivery.Handle,
				Delivery.Batch
			);
		}
		for (const TEventDelivery<FOpenMobileHeadingSensorBatch>& Delivery
			: HeadingDeliveries)
		{
			HeadingBatchEvent.Broadcast(
				Delivery.OwnerIdentifier,
				Delivery.Handle,
				Delivery.Batch
			);
		}
		for (const TEventDelivery<FOpenMobileStepsSensorBatch>& Delivery
			: StepsDeliveries)
		{
			StepsBatchEvent.Broadcast(
				Delivery.OwnerIdentifier,
				Delivery.Handle,
				Delivery.Batch
			);
		}
		for (const TEventDelivery<FOpenMobileActivitySensorBatch>& Delivery
			: ActivityDeliveries)
		{
			ActivityBatchEvent.Broadcast(
				Delivery.OwnerIdentifier,
				Delivery.Handle,
				Delivery.Batch
			);
		}
		for (const TEventDelivery<FOpenMobileOrientationSensorBatch>& Delivery
			: OrientationDeliveries)
		{
			OrientationBatchEvent.Broadcast(
				Delivery.OwnerIdentifier,
				Delivery.Handle,
				Delivery.Batch
			);
		}
		for (const TEventDelivery<FOpenMobileProximitySensorBatch>& Delivery
			: ProximityDeliveries)
		{
			ProximityBatchEvent.Broadcast(
				Delivery.OwnerIdentifier,
				Delivery.Handle,
				Delivery.Batch
			);
		}
#if !UE_BUILD_SHIPPING
		const double ProcessingSeconds = FMath::Max(
			0.0,
			FPlatformTime::Seconds() - ProcessingStartSeconds
		);
		FReadScopeLock RegistryLock(SlotsLock);
		for (TPair<
			FOpenMobileSensorSubscriptionHandle,
			TUniquePtr<FLatestSlot>
		>& Pair : Slots)
		{
			FLatestSlot& Slot = *Pair.Value;
			FScopeLock SlotLock(&Slot.Mutex);
			if (Slot.bHasCallbackTime
				&& Slot.LastCallbackTimeSeconds == SafeNowSeconds)
			{
				Slot.LastGameThreadProcessingSeconds = ProcessingSeconds;
			}
		}
#endif
	}

	bool TickPendingEvents(float DeltaSeconds)
	{
		static_cast<void>(DeltaSeconds);
		DrainPendingEvents(FPlatformTime::Seconds());
		FScopeLock TickerLock(&EventTickerMutex);
		if (HasPendingEvents())
		{
			return true;
		}
		EventTickerHandle.Reset();
		return false;
	}

	void EnsureEventTicker()
	{
		FScopeLock TickerLock(&EventTickerMutex);
		if (!EventTickerHandle.IsValid())
		{
			EventTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateStatic(&TickPendingEvents)
			);
		}
	}

	void CancelEventTicker()
	{
		FScopeLock TickerLock(&EventTickerMutex);
		if (EventTickerHandle.IsValid())
		{
			FTSTicker::RemoveTicker(EventTickerHandle);
			EventTickerHandle.Reset();
		}
	}
}

void FOpenMobileSensorsSampleService::Start()
{
	OpenMobileSensorsSampleServicePrivate::bShuttingDown.Store(false);
	OpenMobileSensorsSampleServicePrivate::CancelEventTicker();
	UnregisterAll();
	OpenMobileSensorsSampleServicePrivate::ResetActivityTransitionTrackers();
}

void FOpenMobileSensorsSampleService::BeginShutdown()
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	bShuttingDown.Store(true);
	CancelEventTicker();
	UnregisterAll();
	ResetActivityTransitionTrackers();
	VectorBatchEvent.Clear();
	AttitudeBatchEvent.Clear();
	ScalarBatchEvent.Clear();
	HeadingBatchEvent.Clear();
	StepsBatchEvent.Clear();
	ActivityBatchEvent.Clear();
	OrientationBatchEvent.Clear();
	ProximityBatchEvent.Clear();
	AccuracyChangedEvent.Clear();
	CalibrationChangedEvent.Clear();
}

void FOpenMobileSensorsSampleService::RegisterSubscription(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	const FOpenMobileSensorIdentifier& Sensor,
	const FOpenMobileSensorIdentifier& PhysicalSensor,
	const FOpenMobileSensorStreamOptions& Options,
	bool bResettableStepCountSession,
	uint64 BackendGeneration
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	if (bShuttingDown.Load()
		|| !OwnerIdentifier.IsValid()
		|| !Handle.IsValid()
		|| !Sensor.IsValid()
		|| !PhysicalSensor.IsValid()
		|| BackendGeneration == 0)
	{
		return;
	}
	TUniquePtr<FLatestSlot> Slot = MakeUnique<FLatestSlot>();
	Slot->OwnerIdentifier = OwnerIdentifier;
	Slot->Handle = Handle;
	Slot->Sensor = Sensor;
	Slot->PhysicalSensor = PhysicalSensor;
	Slot->BackendGeneration = BackendGeneration;
	Slot->ExpectedFamily = GetExpectedFamily(Sensor.Type);
	Slot->StaleAfterSeconds = GetStaleAfterSeconds(Options);
	Slot->AppliedSampleFrequencyHz = Options.CustomFrequencyHz;
	Slot->MaximumCallbackFrequencyHz = Options.MaximumCallbackFrequencyHz;
	Slot->MinimumScalarEventChange = Options.MinimumScalarEventChange;
	Slot->MaximumPendingSamples = FMath::Clamp(
		Options.BufferCapacitySamples,
		1,
		4096
	);
	Slot->DeliveryMode = Options.DeliveryMode;
	Slot->MinimumCallbackAccuracy = Options.MinimumCallbackAccuracy;
	Slot->OverflowPolicy = Options.OverflowPolicy;
	Slot->CoordinateSpace = Options.CoordinateSpace;
	Slot->AttitudeReferenceFrame = Options.AttitudeReferenceFrame;
	Slot->AttitudeRepresentations = Options.AttitudeRepresentations;
	Slot->FilterOptions = Options.Filters;
	FOpenMobileActivityFilterConfig ActivityConfig;
	ActivityConfig.MinimumConfidence = Options.MinimumActivityConfidence;
	ActivityConfig.MinimumStableDurationSeconds =
		Options.MinimumActivityStableDurationSeconds;
	Slot->ActivityFilter.Configure(ActivityConfig);
	Slot->ActivityTransitionFilter.Configure(
		Options.MinimumActivityConfidence
	);
	Slot->bResettableStepCountSession = bResettableStepCountSession;
	if (Sensor.Type == EOpenMobileSensorType::StepDetector)
	{
		Slot->StepDetectionTracker.Initialize(Handle.GetIdentifier());
	}
	if (bResettableStepCountSession)
	{
		Slot->StepCountSessionTracker.Initialize(Handle.GetIdentifier());
	}
	if (const UOpenMobileSensorsSettings* Settings =
		GetDefault<UOpenMobileSensorsSettings>())
	{
		if (FMath::IsFinite(
			Settings->PhysicalOrientationFaceAngleDegrees
		))
		{
			Slot->OrientationConfig.FaceAngleDegrees = FMath::Clamp(
				Settings->PhysicalOrientationFaceAngleDegrees,
				5.0,
				40.0
			);
		}
		if (FMath::IsFinite(
			Settings->PhysicalOrientationHysteresisDegrees
		))
		{
			Slot->OrientationConfig.HysteresisDegrees = FMath::Clamp(
				Settings->PhysicalOrientationHysteresisDegrees,
				0.0,
				15.0
			);
		}
		if (FMath::IsFinite(
			Settings->PhysicalOrientationTransitionDebounceSeconds
		))
		{
			Slot->OrientationConfig.TransitionDebounceSeconds =
				FMath::Clamp(
					Settings->
						PhysicalOrientationTransitionDebounceSeconds,
					0.0,
					2.0
				);
		}
	}
	Slot->PendingAccuracyChanges.Reserve(MaximumPendingAccuracyChanges);
	Slot->PendingCalibrationChanges.Reserve(
		MaximumPendingCalibrationChanges
	);
	if (Slot->DeliveryMode == EOpenMobileSensorDeliveryMode::Buffered)
	{
		InitializeBufferedStorage(*Slot);
	}
	else if (Slot->DeliveryMode == EOpenMobileSensorDeliveryMode::EventBatches)
	{
		ConfigureEventStorage(*Slot);
	}
	FWriteScopeLock RegistryLock(SlotsLock);
	Slots.Add(Handle, MoveTemp(Slot));
}

void FOpenMobileSensorsSampleService::SetPhysicalStreamHandle(
	const FOpenMobileSensorSubscriptionHandle& Handle,
	const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	if (!PhysicalStreamHandle.IsValid())
	{
		return;
	}
	FReadScopeLock RegistryLock(SlotsLock);
	const TUniquePtr<FLatestSlot>* SlotPointer = Slots.Find(Handle);
	if (!SlotPointer)
	{
		return;
	}
	FScopeLock SlotLock(&(*SlotPointer)->Mutex);
	(*SlotPointer)->PhysicalStreamHandle = PhysicalStreamHandle;
}

void FOpenMobileSensorsSampleService::SetSubscriptionState(
	const FOpenMobileSensorSubscriptionHandle& Handle,
	EOpenMobileSensorSubscriptionState State
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	bool bSchedulePendingEvents = false;
	{
		FReadScopeLock RegistryLock(SlotsLock);
		const TUniquePtr<FLatestSlot>* SlotPointer = Slots.Find(Handle);
		if (!SlotPointer)
		{
			return;
		}
		FScopeLock SlotLock(&(*SlotPointer)->Mutex);
		(*SlotPointer)->State = State;
		if (State != EOpenMobileSensorSubscriptionState::Active)
		{
			ResetRateStatistics(**SlotPointer);
			ResetGyroscopeDriftStatistics(**SlotPointer);
			(*SlotPointer)->VectorFilter.Reset();
			ResetDerivedEstimators(**SlotPointer);
			(*SlotPointer)->bHasLastScalarEventValue = false;
		}
		bSchedulePendingEvents =
			State == EOpenMobileSensorSubscriptionState::Active
			&& (*SlotPointer)->DeliveryMode ==
				EOpenMobileSensorDeliveryMode::EventBatches
			&& HasPendingSamples(**SlotPointer);
	}
	if (bSchedulePendingEvents)
	{
		EnsureEventTicker();
	}
}

void FOpenMobileSensorsSampleService::UpdateSubscriptionOptions(
	const FOpenMobileSensorSubscriptionHandle& Handle,
	const FOpenMobileSensorStreamOptions& Options
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	FReadScopeLock RegistryLock(SlotsLock);
	const TUniquePtr<FLatestSlot>* SlotPointer = Slots.Find(Handle);
	if (!SlotPointer)
	{
		return;
	}
	FScopeLock SlotLock(&(*SlotPointer)->Mutex);
	const EOpenMobileSensorDeliveryMode PreviousDeliveryMode =
		(*SlotPointer)->DeliveryMode;
	const bool bReferenceFrameChanged =
		(*SlotPointer)->AttitudeReferenceFrame !=
			Options.AttitudeReferenceFrame;
	const bool bScalarThresholdChanged =
		(*SlotPointer)->MinimumScalarEventChange !=
			Options.MinimumScalarEventChange;
	(*SlotPointer)->StaleAfterSeconds = GetStaleAfterSeconds(Options);
	(*SlotPointer)->AppliedSampleFrequencyHz = Options.CustomFrequencyHz;
	ResetRateStatistics(**SlotPointer);
	ResetGyroscopeDriftStatistics(**SlotPointer);
	ResetDerivedEstimators(**SlotPointer);
	(*SlotPointer)->MaximumCallbackFrequencyHz =
		Options.MaximumCallbackFrequencyHz;
	(*SlotPointer)->MinimumScalarEventChange =
		Options.MinimumScalarEventChange;
	if (bScalarThresholdChanged)
	{
		(*SlotPointer)->bHasLastScalarEventValue = false;
	}
	(*SlotPointer)->MaximumPendingSamples = FMath::Clamp(
		Options.BufferCapacitySamples,
		1,
		4096
	);
	(*SlotPointer)->DeliveryMode = Options.DeliveryMode;
	(*SlotPointer)->MinimumCallbackAccuracy =
		Options.MinimumCallbackAccuracy;
	(*SlotPointer)->OverflowPolicy = Options.OverflowPolicy;
	(*SlotPointer)->CoordinateSpace = Options.CoordinateSpace;
	(*SlotPointer)->AttitudeReferenceFrame = Options.AttitudeReferenceFrame;
	(*SlotPointer)->AttitudeRepresentations =
		Options.AttitudeRepresentations;
	(*SlotPointer)->FilterOptions = Options.Filters;
	(*SlotPointer)->VectorFilter.Reset();
	FOpenMobileActivityFilterConfig ActivityConfig;
	ActivityConfig.MinimumConfidence = Options.MinimumActivityConfidence;
	ActivityConfig.MinimumStableDurationSeconds =
		Options.MinimumActivityStableDurationSeconds;
	(*SlotPointer)->ActivityFilter.Configure(ActivityConfig);
	(*SlotPointer)->ActivityTransitionFilter.Configure(
		Options.MinimumActivityConfidence
	);
	if (bReferenceFrameChanged)
	{
		(*SlotPointer)->Recenter = {};
		(*SlotPointer)->LatestCanonicalAttitude = FQuat::Identity;
		(*SlotPointer)->bHasCanonicalAttitude = false;
		(*SlotPointer)->bHasSample = false;
		(*SlotPointer)->Family = ELatestSampleFamily::None;
		(*SlotPointer)->LatestTimestampSeconds = 0.0;
		(*SlotPointer)->bPendingStatefulProcessingReset = true;
		ClearPendingEvents(**SlotPointer);
		ClearBufferedStorage(**SlotPointer);
	}
	if (Options.DeliveryMode == EOpenMobileSensorDeliveryMode::Buffered)
	{
		if (PreviousDeliveryMode == EOpenMobileSensorDeliveryMode::Buffered)
		{
			ResizeBufferedStorage(**SlotPointer);
		}
		else
		{
			InitializeBufferedStorage(**SlotPointer);
		}
	}
	else
	{
		ClearBufferedStorage(**SlotPointer);
	}
	if (Options.DeliveryMode == EOpenMobileSensorDeliveryMode::EventBatches)
	{
		ConfigureEventStorage(**SlotPointer);
	}
	else
	{
		ClearPendingEvents(**SlotPointer);
	}
}

void FOpenMobileSensorsSampleService::UnregisterSubscription(
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	FWriteScopeLock RegistryLock(SlotsLock);
	Slots.Remove(Handle);
}

void FOpenMobileSensorsSampleService::UnregisterAll()
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	FWriteScopeLock RegistryLock(SlotsLock);
	Slots.Reset();
}

bool FOpenMobileSensorsSampleService::GetRateDiagnostics(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	FOpenMobileSensorRateDiagnostics& OutRate
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	OutRate = {};
	if (!OwnerIdentifier.IsValid() || !Handle.IsValid())
	{
		return false;
	}
	FReadScopeLock RegistryLock(SlotsLock);
	const TUniquePtr<FLatestSlot>* SlotPointer = Slots.Find(Handle);
	if (!SlotPointer)
	{
		return false;
	}
	FScopeLock SlotLock(&(*SlotPointer)->Mutex);
	const FLatestSlot& Slot = **SlotPointer;
	if (Slot.OwnerIdentifier != OwnerIdentifier || Slot.Handle != Handle)
	{
		return false;
	}
	OutRate.SampleCount = Slot.bHasRateTimestamp
		? Slot.RateIntervalCount + 1
		: 0;
	OutRate.LastGapSeconds = Slot.LastRateGapSeconds;
	if (Slot.RateIntervalCount > 0 && Slot.RateIntervalSum > 0.0)
	{
		const double MeanIntervalSeconds =
			Slot.RateIntervalSum / Slot.RateIntervalCount;
		double IntervalVariance = 0.0;
		for (int32 Index = 0; Index < Slot.RateIntervalCount; ++Index)
		{
			const double Difference =
				Slot.RateIntervals[
					(Slot.RateIntervalStart + Index)
					% UE_ARRAY_COUNT(Slot.RateIntervals)
				] - MeanIntervalSeconds;
			IntervalVariance += Difference * Difference;
		}
		OutRate.MeanFrequencyHz = 1.0 / MeanIntervalSeconds;
		OutRate.IntervalJitterSeconds = FMath::Sqrt(
			IntervalVariance / Slot.RateIntervalCount
		);
	}
	return true;
}

bool FOpenMobileSensorsSampleService::GetDeliveryDiagnostics(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	double NowSeconds,
	FOpenMobileSensorStreamDiagnostics& OutDiagnostics
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	if (!OwnerIdentifier.IsValid()
		|| !Handle.IsValid()
		|| !FMath::IsFinite(NowSeconds))
	{
		return false;
	}
	FReadScopeLock RegistryLock(SlotsLock);
	const TUniquePtr<FLatestSlot>* SlotPointer = Slots.Find(Handle);
	if (!SlotPointer)
	{
		return false;
	}
	FScopeLock SlotLock(&(*SlotPointer)->Mutex);
	const FLatestSlot& Slot = **SlotPointer;
	if (Slot.OwnerIdentifier != OwnerIdentifier || Slot.Handle != Handle)
	{
		return false;
	}
	OutDiagnostics.GyroscopeDrift = {};
	if (Slot.Sensor.Type == EOpenMobileSensorType::Gyroscope
		&& Slot.GyroscopeSampleCount > 0)
	{
		OutDiagnostics.GyroscopeDrift.SampleCount = Slot.GyroscopeSampleCount;
		OutDiagnostics.GyroscopeDrift.
			MeanAngularVelocityRadiansPerSecond =
			Slot.GyroscopeAngularVelocitySum / Slot.GyroscopeSampleCount;
		OutDiagnostics.GyroscopeDrift.
			RootMeanSquareAngularSpeedRadiansPerSecond = FMath::Sqrt(
				FMath::Max(
					0.0,
					Slot.GyroscopeAngularSpeedSquareSum /
						Slot.GyroscopeSampleCount
				)
			);
	}
	OutDiagnostics.QueueDepth = GetPluginSampleCount(Slot);
	OutDiagnostics.BufferHighWaterMark = GetPluginHighWaterMark(Slot);
	OutDiagnostics.DroppedSamples = GetPluginDroppedSamples(Slot);
	OutDiagnostics.LatestSampleAgeSeconds = Slot.bHasSample
		? FMath::Max(0.0, NowSeconds - Slot.LatestTimestampSeconds)
		: 0.0;
	const double OldestTimestampSeconds =
		GetOldestPluginTimestampSeconds(Slot);
	OutDiagnostics.QueueDelaySeconds =
		OutDiagnostics.QueueDepth > 0 && OldestTimestampSeconds > 0.0
		? FMath::Max(0.0, NowSeconds - OldestTimestampSeconds)
		: 0.0;
#if !UE_BUILD_SHIPPING
	OutDiagnostics.GameThreadProcessingSeconds =
		Slot.LastGameThreadProcessingSeconds;
#endif
	return true;
}

bool FOpenMobileSensorsSampleService::FlushPluginSamples(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	int32& OutSampleCount
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	check(IsInGameThread());
	OutSampleCount = 0;
	bool bDrainEvents = false;
	{
		FReadScopeLock RegistryLock(SlotsLock);
		const TUniquePtr<FLatestSlot>* SlotPointer = Slots.Find(Handle);
		if (!OwnerIdentifier.IsValid()
			|| !Handle.IsValid()
			|| !SlotPointer)
		{
			return false;
		}
		FScopeLock SlotLock(&(*SlotPointer)->Mutex);
		FLatestSlot& Slot = **SlotPointer;
		if (Slot.OwnerIdentifier != OwnerIdentifier || Slot.Handle != Handle)
		{
			return false;
		}
		OutSampleCount = GetPluginSampleCount(Slot);
		bDrainEvents =
			Slot.DeliveryMode == EOpenMobileSensorDeliveryMode::EventBatches
			&& OutSampleCount > 0;
		if (bDrainEvents)
		{
			Slot.bHasCallbackTime = false;
		}
	}
	if (bDrainEvents)
	{
		DrainPendingEvents(FPlatformTime::Seconds());
	}
	return true;
}

bool FOpenMobileSensorsSampleService::RecenterAttitude(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	EOpenMobileSensorRecenterMode Mode
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	FReadScopeLock RegistryLock(SlotsLock);
	const TUniquePtr<FLatestSlot>* SlotPointer = Slots.Find(Handle);
	if (!OwnerIdentifier.IsValid() || !Handle.IsValid() || !SlotPointer)
	{
		return false;
	}
	FScopeLock SlotLock(&(*SlotPointer)->Mutex);
	FLatestSlot& Slot = **SlotPointer;
	if (Slot.OwnerIdentifier != OwnerIdentifier
		|| Slot.Handle != Handle
		|| Slot.Sensor.Type != EOpenMobileSensorType::Attitude)
	{
		return false;
	}
	if (Mode == EOpenMobileSensorRecenterMode::Clear)
	{
		Slot.Recenter = {};
	}
	else
	{
		if (!Slot.bHasCanonicalAttitude)
		{
			return false;
		}
		FQuat InverseReference;
		if (Mode == EOpenMobileSensorRecenterMode::FullAttitude)
		{
			InverseReference = Slot.LatestCanonicalAttitude.Inverse();
		}
		else if (Mode == EOpenMobileSensorRecenterMode::YawOnly)
		{
			InverseReference = FRotator(
				0.0,
				-Slot.LatestCanonicalAttitude.Rotator().Yaw,
				0.0
			).Quaternion();
		}
		else
		{
			return false;
		}
		InverseReference.Normalize();
		Slot.Recenter.bApplied = true;
		Slot.Recenter.Mode = Mode;
		Slot.Recenter.InverseReference = InverseReference;
	}
	Slot.bHasSample = false;
	Slot.Family = ELatestSampleFamily::None;
	Slot.LatestTimestampSeconds = 0.0;
	Slot.bPendingStatefulProcessingReset = true;
	ClearPendingEvents(Slot);
	ClearBufferedStorage(Slot);
	return true;
}

bool FOpenMobileSensorsSampleService::GetAttitudeRecenterState(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	FOpenMobileSensorRecenterState& OutState
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	OutState = {};
	FReadScopeLock RegistryLock(SlotsLock);
	const TUniquePtr<FLatestSlot>* SlotPointer = Slots.Find(Handle);
	if (!OwnerIdentifier.IsValid() || !Handle.IsValid() || !SlotPointer)
	{
		return false;
	}
	FScopeLock SlotLock(&(*SlotPointer)->Mutex);
	const FLatestSlot& Slot = **SlotPointer;
	if (Slot.OwnerIdentifier != OwnerIdentifier
		|| Slot.Handle != Handle
		|| Slot.Sensor.Type != EOpenMobileSensorType::Attitude)
	{
		return false;
	}
	OutState = Slot.Recenter;
	return true;
}

bool FOpenMobileSensorsSampleService::RecenterRelativeAltitude(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	FReadScopeLock RegistryLock(SlotsLock);
	const TUniquePtr<FLatestSlot>* SlotPointer = Slots.Find(Handle);
	if (!OwnerIdentifier.IsValid() || !Handle.IsValid() || !SlotPointer)
	{
		return false;
	}
	FScopeLock SlotLock(&(*SlotPointer)->Mutex);
	FLatestSlot& Slot = **SlotPointer;
	if (Slot.OwnerIdentifier != OwnerIdentifier
		|| Slot.Handle != Handle
		|| Slot.Sensor.Type != EOpenMobileSensorType::RelativeAltitude
		|| Slot.State != EOpenMobileSensorSubscriptionState::Active)
	{
		return false;
	}
	Slot.RelativeAltitudeEstimator.Reset();
	Slot.bHasSample = false;
	Slot.Family = ELatestSampleFamily::None;
	Slot.LatestTimestampSeconds = 0.0;
	Slot.bPendingStatefulProcessingReset = true;
	ClearPendingEvents(Slot);
	ClearBufferedStorage(Slot);
	return true;
}

bool FOpenMobileSensorsSampleService::ResetStepCountSession(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	FReadScopeLock RegistryLock(SlotsLock);
	const TUniquePtr<FLatestSlot>* SlotPointer = Slots.Find(Handle);
	if (!OwnerIdentifier.IsValid() || !Handle.IsValid() || !SlotPointer)
	{
		return false;
	}
	FScopeLock SlotLock(&(*SlotPointer)->Mutex);
	FLatestSlot& Slot = **SlotPointer;
	if (Slot.OwnerIdentifier != OwnerIdentifier
		|| Slot.Handle != Handle
		|| !Slot.bResettableStepCountSession
		|| Slot.Sensor.Type != EOpenMobileSensorType::StepCounter
		|| Slot.State != EOpenMobileSensorSubscriptionState::Active)
	{
		return false;
	}
	Slot.StepCountSessionTracker.ResetBaseline();
	Slot.bHasSample = false;
	Slot.Family = ELatestSampleFamily::None;
	Slot.LatestTimestampSeconds = 0.0;
	Slot.bPendingStatefulProcessingReset = true;
	ClearPendingEvents(Slot);
	ClearBufferedStorage(Slot);
	return true;
}

#define OPENMOBILE_IMPLEMENT_PUBLISH( \
	MethodName, SampleType, FamilyName, Member, PendingMember, BufferedMember \
) \
	void FOpenMobileSensorsSampleService::MethodName(const SampleType& Sample) \
	{ \
		OpenMobileSensorsSampleServicePrivate::PublishSamples( \
			&Sample, \
			1, \
			OpenMobileSensorsSampleServicePrivate::ELatestSampleFamily::FamilyName, \
			&OpenMobileSensorsSampleServicePrivate::FLatestSlot::Member, \
			&OpenMobileSensorsSampleServicePrivate::FLatestSlot::PendingMember, \
			&OpenMobileSensorsSampleServicePrivate::FLatestSlot::BufferedMember, \
			0, \
			nullptr \
		); \
	}

OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishAttitude,
	FOpenMobileAttitudeSensorSample,
	Attitude,
	Attitude,
	PendingAttitude,
	BufferedAttitude
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishScalar,
	FOpenMobileScalarSensorSample,
	Scalar,
	Scalar,
	PendingScalar,
	BufferedScalar
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishHeading,
	FOpenMobileHeadingSensorSample,
	Heading,
	Heading,
	PendingHeading,
	BufferedHeading
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishSteps,
	FOpenMobileStepsSensorSample,
	Steps,
	Steps,
	PendingSteps,
	BufferedSteps
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishOrientation,
	FOpenMobileOrientationSensorSample,
	Orientation,
	Orientation,
	PendingOrientation,
	BufferedOrientation
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishProximity,
	FOpenMobileProximitySensorSample,
	Proximity,
	Proximity,
	PendingProximity,
	BufferedProximity
)

#undef OPENMOBILE_IMPLEMENT_PUBLISH

void FOpenMobileSensorsSampleService::PublishActivity(
	const FOpenMobileActivitySensorSample& Sample
)
{
	OpenMobileSensorsSampleServicePrivate::PublishActivitySamples(
		&Sample,
		1,
		0,
		nullptr
	);
}

void FOpenMobileSensorsSampleService::PublishVector(
	const FOpenMobileVectorSensorSample& Sample
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	PublishSamples(
		&Sample,
		1,
		ELatestSampleFamily::Vector,
		&FLatestSlot::Vector,
		&FLatestSlot::PendingVector,
		&FLatestSlot::BufferedVector,
		0,
		nullptr
	);
	PublishDerivedOrientationSamples(&Sample, 1, 0, nullptr);
}

bool FOpenMobileSensorsSampleService::PublishAccuracy(
	const FOpenMobileSensorAccuracySnapshot& Snapshot
)
{
	return OpenMobileSensorsSampleServicePrivate::PublishAccuracySnapshot(
		Snapshot,
		0,
		nullptr
	);
}

#define OPENMOBILE_IMPLEMENT_PUBLISH_BATCH( \
	MethodName, BatchType, SampleType, FamilyName, Member, \
	PendingMember, BufferedMember \
) \
	bool FOpenMobileSensorsSampleService::MethodName(const BatchType& Batch) \
	{ \
		return OpenMobileSensorsSampleServicePrivate::PublishSamples< \
			SampleType \
		>( \
			Batch.Samples.GetData(), \
			Batch.Samples.Num(), \
			OpenMobileSensorsSampleServicePrivate::ELatestSampleFamily::FamilyName, \
			&OpenMobileSensorsSampleServicePrivate::FLatestSlot::Member, \
			&OpenMobileSensorsSampleServicePrivate::FLatestSlot::PendingMember, \
			&OpenMobileSensorsSampleServicePrivate::FLatestSlot::BufferedMember, \
			0, \
			nullptr \
		); \
	}

OPENMOBILE_IMPLEMENT_PUBLISH_BATCH(
	PublishAttitudeBatch,
	FOpenMobileAttitudeSensorBatch,
	FOpenMobileAttitudeSensorSample,
	Attitude,
	Attitude,
	PendingAttitude,
	BufferedAttitude
)
OPENMOBILE_IMPLEMENT_PUBLISH_BATCH(
	PublishScalarBatch,
	FOpenMobileScalarSensorBatch,
	FOpenMobileScalarSensorSample,
	Scalar,
	Scalar,
	PendingScalar,
	BufferedScalar
)
OPENMOBILE_IMPLEMENT_PUBLISH_BATCH(
	PublishHeadingBatch,
	FOpenMobileHeadingSensorBatch,
	FOpenMobileHeadingSensorSample,
	Heading,
	Heading,
	PendingHeading,
	BufferedHeading
)
OPENMOBILE_IMPLEMENT_PUBLISH_BATCH(
	PublishStepsBatch,
	FOpenMobileStepsSensorBatch,
	FOpenMobileStepsSensorSample,
	Steps,
	Steps,
	PendingSteps,
	BufferedSteps
)
OPENMOBILE_IMPLEMENT_PUBLISH_BATCH(
	PublishOrientationBatch,
	FOpenMobileOrientationSensorBatch,
	FOpenMobileOrientationSensorSample,
	Orientation,
	Orientation,
	PendingOrientation,
	BufferedOrientation
)
OPENMOBILE_IMPLEMENT_PUBLISH_BATCH(
	PublishProximityBatch,
	FOpenMobileProximitySensorBatch,
	FOpenMobileProximitySensorSample,
	Proximity,
	Proximity,
	PendingProximity,
	BufferedProximity
)

#undef OPENMOBILE_IMPLEMENT_PUBLISH_BATCH

bool FOpenMobileSensorsSampleService::PublishActivityBatch(
	const FOpenMobileActivitySensorBatch& Batch
)
{
	return OpenMobileSensorsSampleServicePrivate::PublishActivitySamples(
		Batch.Samples.GetData(),
		Batch.Samples.Num(),
		0,
		nullptr
	);
}

bool FOpenMobileSensorsSampleService::PublishVectorBatch(
	const FOpenMobileVectorSensorBatch& Batch
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	const bool bPublishedVector = PublishSamples(
		Batch.Samples.GetData(),
		Batch.Samples.Num(),
		ELatestSampleFamily::Vector,
		&FLatestSlot::Vector,
		&FLatestSlot::PendingVector,
		&FLatestSlot::BufferedVector,
		0,
		nullptr
	);
	const bool bPublishedOrientation = PublishDerivedOrientationSamples(
		Batch.Samples.GetData(),
		Batch.Samples.Num(),
		0,
		nullptr
	);
	return bPublishedVector || bPublishedOrientation;
}

#define OPENMOBILE_IMPLEMENT_BACKEND_PUBLISH_BATCH( \
	MethodName, BatchType, SampleType, FamilyName, Member, \
	PendingMember, BufferedMember \
) \
	bool FOpenMobileSensorsSampleService::MethodName( \
		const FOpenMobileSensorsBackendToken& Token, \
		const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle, \
		const BatchType& Batch \
	) \
	{ \
		if (!FOpenMobileSensorsBackendRegistry::IsTokenCurrent(Token) \
			|| !PhysicalStreamHandle.IsValid()) \
		{ \
			return false; \
		} \
		return OpenMobileSensorsSampleServicePrivate::PublishSamples< \
			SampleType \
		>( \
			Batch.Samples.GetData(), \
			Batch.Samples.Num(), \
			OpenMobileSensorsSampleServicePrivate::ELatestSampleFamily::FamilyName, \
			&OpenMobileSensorsSampleServicePrivate::FLatestSlot::Member, \
			&OpenMobileSensorsSampleServicePrivate::FLatestSlot::PendingMember, \
			&OpenMobileSensorsSampleServicePrivate::FLatestSlot::BufferedMember, \
			Token.Generation, \
			&PhysicalStreamHandle \
		); \
	}

OPENMOBILE_IMPLEMENT_BACKEND_PUBLISH_BATCH(
	PublishAttitudeBatchFromBackend,
	FOpenMobileAttitudeSensorBatch,
	FOpenMobileAttitudeSensorSample,
	Attitude,
	Attitude,
	PendingAttitude,
	BufferedAttitude
)
OPENMOBILE_IMPLEMENT_BACKEND_PUBLISH_BATCH(
	PublishScalarBatchFromBackend,
	FOpenMobileScalarSensorBatch,
	FOpenMobileScalarSensorSample,
	Scalar,
	Scalar,
	PendingScalar,
	BufferedScalar
)
OPENMOBILE_IMPLEMENT_BACKEND_PUBLISH_BATCH(
	PublishHeadingBatchFromBackend,
	FOpenMobileHeadingSensorBatch,
	FOpenMobileHeadingSensorSample,
	Heading,
	Heading,
	PendingHeading,
	BufferedHeading
)
OPENMOBILE_IMPLEMENT_BACKEND_PUBLISH_BATCH(
	PublishStepsBatchFromBackend,
	FOpenMobileStepsSensorBatch,
	FOpenMobileStepsSensorSample,
	Steps,
	Steps,
	PendingSteps,
	BufferedSteps
)
OPENMOBILE_IMPLEMENT_BACKEND_PUBLISH_BATCH(
	PublishOrientationBatchFromBackend,
	FOpenMobileOrientationSensorBatch,
	FOpenMobileOrientationSensorSample,
	Orientation,
	Orientation,
	PendingOrientation,
	BufferedOrientation
)
OPENMOBILE_IMPLEMENT_BACKEND_PUBLISH_BATCH(
	PublishProximityBatchFromBackend,
	FOpenMobileProximitySensorBatch,
	FOpenMobileProximitySensorSample,
	Proximity,
	Proximity,
	PendingProximity,
	BufferedProximity
)

#undef OPENMOBILE_IMPLEMENT_BACKEND_PUBLISH_BATCH

bool FOpenMobileSensorsSampleService::PublishActivityBatchFromBackend(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle,
	const FOpenMobileActivitySensorBatch& Batch
)
{
	if (!FOpenMobileSensorsBackendRegistry::IsTokenCurrent(Token)
		|| !PhysicalStreamHandle.IsValid())
	{
		return false;
	}
	return OpenMobileSensorsSampleServicePrivate::PublishActivitySamples(
		Batch.Samples.GetData(),
		Batch.Samples.Num(),
		Token.Generation,
		&PhysicalStreamHandle
	);
}

bool FOpenMobileSensorsSampleService::PublishVectorBatchFromBackend(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle,
	const FOpenMobileVectorSensorBatch& Batch
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	if (!FOpenMobileSensorsBackendRegistry::IsTokenCurrent(Token)
		|| !PhysicalStreamHandle.IsValid())
	{
		return false;
	}
	const bool bPublishedVector = PublishSamples(
		Batch.Samples.GetData(),
		Batch.Samples.Num(),
		ELatestSampleFamily::Vector,
		&FLatestSlot::Vector,
		&FLatestSlot::PendingVector,
		&FLatestSlot::BufferedVector,
		Token.Generation,
		&PhysicalStreamHandle
	);
	const bool bPublishedOrientation = PublishDerivedOrientationSamples(
		Batch.Samples.GetData(),
		Batch.Samples.Num(),
		Token.Generation,
		&PhysicalStreamHandle
	);
	return bPublishedVector || bPublishedOrientation;
}

bool FOpenMobileSensorsSampleService::PublishAccuracyFromBackend(
	const FOpenMobileSensorsBackendToken& Token,
	const FOpenMobileSensorBackendStreamHandle& PhysicalStreamHandle,
	const FOpenMobileSensorAccuracySnapshot& Snapshot
)
{
	if (!FOpenMobileSensorsBackendRegistry::IsTokenCurrent(Token)
		|| !PhysicalStreamHandle.IsValid())
	{
		return false;
	}
	return OpenMobileSensorsSampleServicePrivate::PublishAccuracySnapshot(
		Snapshot,
		Token.Generation,
		&PhysicalStreamHandle
	);
}

#define OPENMOBILE_IMPLEMENT_READ(MethodName, SampleType, FamilyName, Member) \
	bool FOpenMobileSensorsSampleService::MethodName( \
		const FGuid& OwnerIdentifier, \
		const FOpenMobileSensorSubscriptionHandle& Handle, \
		int64 LastSeenSequence, \
		double NowSeconds, \
		FOpenMobileSensorReadResult& OutResult, \
		SampleType& OutSample \
	) \
	{ \
		return OpenMobileSensorsSampleServicePrivate::ReadLatestSample( \
			OwnerIdentifier, \
			Handle, \
			LastSeenSequence, \
			NowSeconds, \
			OutResult, \
			OutSample, \
			OpenMobileSensorsSampleServicePrivate::ELatestSampleFamily::FamilyName, \
			&OpenMobileSensorsSampleServicePrivate::FLatestSlot::Member \
		); \
	}

OPENMOBILE_IMPLEMENT_READ(
	ReadLatestVector,
	FOpenMobileVectorSensorSample,
	Vector,
	Vector
)
OPENMOBILE_IMPLEMENT_READ(
	ReadLatestAttitude,
	FOpenMobileAttitudeSensorSample,
	Attitude,
	Attitude
)
OPENMOBILE_IMPLEMENT_READ(
	ReadLatestScalar,
	FOpenMobileScalarSensorSample,
	Scalar,
	Scalar
)
OPENMOBILE_IMPLEMENT_READ(
	ReadLatestHeading,
	FOpenMobileHeadingSensorSample,
	Heading,
	Heading
)
OPENMOBILE_IMPLEMENT_READ(
	ReadLatestSteps,
	FOpenMobileStepsSensorSample,
	Steps,
	Steps
)
OPENMOBILE_IMPLEMENT_READ(
	ReadLatestActivity,
	FOpenMobileActivitySensorSample,
	Activity,
	Activity
)
OPENMOBILE_IMPLEMENT_READ(
	ReadLatestOrientation,
	FOpenMobileOrientationSensorSample,
	Orientation,
	Orientation
)
OPENMOBILE_IMPLEMENT_READ(
	ReadLatestProximity,
	FOpenMobileProximitySensorSample,
	Proximity,
	Proximity
)

#undef OPENMOBILE_IMPLEMENT_READ

#define OPENMOBILE_IMPLEMENT_BUFFERED_DRAIN( \
	MethodName, SampleType, BatchType, FamilyName, BufferedMember \
) \
	bool FOpenMobileSensorsSampleService::MethodName( \
		const FGuid& OwnerIdentifier, \
		const FOpenMobileSensorSubscriptionHandle& Handle, \
		int32 MaximumSamples, \
		FOpenMobileSensorBufferReadResult& OutResult, \
		BatchType& OutBatch \
	) \
	{ \
		return OpenMobileSensorsSampleServicePrivate::DrainBufferedSamples< \
			SampleType \
		>( \
			OwnerIdentifier, \
			Handle, \
			MaximumSamples, \
			OutResult, \
			OutBatch, \
			OpenMobileSensorsSampleServicePrivate::ELatestSampleFamily::FamilyName, \
			&OpenMobileSensorsSampleServicePrivate::FLatestSlot::BufferedMember \
		); \
	}

OPENMOBILE_IMPLEMENT_BUFFERED_DRAIN(
	DrainBufferedVector,
	FOpenMobileVectorSensorSample,
	FOpenMobileVectorSensorBatch,
	Vector,
	BufferedVector
)
OPENMOBILE_IMPLEMENT_BUFFERED_DRAIN(
	DrainBufferedAttitude,
	FOpenMobileAttitudeSensorSample,
	FOpenMobileAttitudeSensorBatch,
	Attitude,
	BufferedAttitude
)
OPENMOBILE_IMPLEMENT_BUFFERED_DRAIN(
	DrainBufferedScalar,
	FOpenMobileScalarSensorSample,
	FOpenMobileScalarSensorBatch,
	Scalar,
	BufferedScalar
)
OPENMOBILE_IMPLEMENT_BUFFERED_DRAIN(
	DrainBufferedHeading,
	FOpenMobileHeadingSensorSample,
	FOpenMobileHeadingSensorBatch,
	Heading,
	BufferedHeading
)
OPENMOBILE_IMPLEMENT_BUFFERED_DRAIN(
	DrainBufferedSteps,
	FOpenMobileStepsSensorSample,
	FOpenMobileStepsSensorBatch,
	Steps,
	BufferedSteps
)
OPENMOBILE_IMPLEMENT_BUFFERED_DRAIN(
	DrainBufferedActivity,
	FOpenMobileActivitySensorSample,
	FOpenMobileActivitySensorBatch,
	Activity,
	BufferedActivity
)
OPENMOBILE_IMPLEMENT_BUFFERED_DRAIN(
	DrainBufferedOrientation,
	FOpenMobileOrientationSensorSample,
	FOpenMobileOrientationSensorBatch,
	Orientation,
	BufferedOrientation
)
OPENMOBILE_IMPLEMENT_BUFFERED_DRAIN(
	DrainBufferedProximity,
	FOpenMobileProximitySensorSample,
	FOpenMobileProximitySensorBatch,
	Proximity,
	BufferedProximity
)

#undef OPENMOBILE_IMPLEMENT_BUFFERED_DRAIN

FOnOpenMobileVectorSensorBatchReady&
FOpenMobileSensorsSampleService::OnVectorBatch()
{
	return OpenMobileSensorsSampleServicePrivate::VectorBatchEvent;
}

FOnOpenMobileAttitudeSensorBatchReady&
FOpenMobileSensorsSampleService::OnAttitudeBatch()
{
	return OpenMobileSensorsSampleServicePrivate::AttitudeBatchEvent;
}

FOnOpenMobileScalarSensorBatchReady&
FOpenMobileSensorsSampleService::OnScalarBatch()
{
	return OpenMobileSensorsSampleServicePrivate::ScalarBatchEvent;
}

FOnOpenMobileHeadingSensorBatchReady&
FOpenMobileSensorsSampleService::OnHeadingBatch()
{
	return OpenMobileSensorsSampleServicePrivate::HeadingBatchEvent;
}

FOnOpenMobileStepsSensorBatchReady&
FOpenMobileSensorsSampleService::OnStepsBatch()
{
	return OpenMobileSensorsSampleServicePrivate::StepsBatchEvent;
}

FOnOpenMobileActivitySensorBatchReady&
FOpenMobileSensorsSampleService::OnActivityBatch()
{
	return OpenMobileSensorsSampleServicePrivate::ActivityBatchEvent;
}

FOnOpenMobileOrientationSensorBatchReady&
FOpenMobileSensorsSampleService::OnOrientationBatch()
{
	return OpenMobileSensorsSampleServicePrivate::OrientationBatchEvent;
}

FOnOpenMobileProximitySensorBatchReady&
FOpenMobileSensorsSampleService::OnProximityBatch()
{
	return OpenMobileSensorsSampleServicePrivate::ProximityBatchEvent;
}

FOnOpenMobileSensorAccuracyChangedReady&
FOpenMobileSensorsSampleService::OnAccuracyChanged()
{
	return OpenMobileSensorsSampleServicePrivate::AccuracyChangedEvent;
}

FOnOpenMobileSensorCalibrationChangedReady&
FOpenMobileSensorsSampleService::OnCalibrationChanged()
{
	return OpenMobileSensorsSampleServicePrivate::CalibrationChangedEvent;
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileSensorsSampleService::DrainPendingEventsForTests(
	double NowSeconds
)
{
	OpenMobileSensorsSampleServicePrivate::CancelEventTicker();
	OpenMobileSensorsSampleServicePrivate::DrainPendingEvents(NowSeconds);
}

void FOpenMobileSensorsSampleService::ResetForTests()
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	bShuttingDown.Store(false);
	CancelEventTicker();
	UnregisterAll();
	ResetActivityTransitionTrackers();
	VectorBatchEvent.Clear();
	AttitudeBatchEvent.Clear();
	ScalarBatchEvent.Clear();
	HeadingBatchEvent.Clear();
	StepsBatchEvent.Clear();
	ActivityBatchEvent.Clear();
	OrientationBatchEvent.Clear();
	ProximityBatchEvent.Clear();
	AccuracyChangedEvent.Clear();
	CalibrationChangedEvent.Clear();
}
#endif
