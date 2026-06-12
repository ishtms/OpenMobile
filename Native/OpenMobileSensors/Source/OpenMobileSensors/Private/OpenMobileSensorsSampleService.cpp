#include "OpenMobileSensorsSampleService.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsBackendTypes.h"
#include "OpenMobileSensorsErrorMapper.h"

namespace OpenMobileSensorsSampleServicePrivate
{
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
		double LastCallbackTimeSeconds = 0.0;
		double RateIntervals[64] = {};
		double RateIntervalSum = 0.0;
		double RateIntervalSquareSum = 0.0;
		double LastRateTimestampSeconds = 0.0;
		double LastRateGapSeconds = 0.0;
		double LastGameThreadProcessingSeconds = 0.0;
		int32 RateIntervalStart = 0;
		int32 RateIntervalCount = 0;
		int32 EventHighWaterMark = 0;
		int64 EventDroppedSamples = 0;
		int32 PendingTimestampIssueFlags = 0;
		int32 MaximumPendingSamples = 128;
		EOpenMobileSensorDeliveryMode DeliveryMode =
			EOpenMobileSensorDeliveryMode::LatestValue;
		EOpenMobileSensorOverflowPolicy OverflowPolicy =
			EOpenMobileSensorOverflowPolicy::DropOldest;
		bool bHasSample = false;
		bool bHasCallbackTime = false;
		bool bHasRateTimestamp = false;
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
		Slot.PendingTimestampIssueFlags |= static_cast<int32>(Issue);
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

	template <typename SampleType>
	bool PublishSamples(
		const SampleType* Samples,
		int32 SampleCount,
		ELatestSampleFamily Family,
		SampleType FLatestSlot::* Member,
		TArray<SampleType> FLatestSlot::* PendingMember,
		TFixedSampleRingBuffer<SampleType> FLatestSlot::* BufferedMember,
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
					const SampleType& Sample = Samples[Index];
					if (Slot.Sensor != Sample.Header.Sensor)
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
					if (!Sample.Header.bValid)
					{
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
						Slot.PendingTimestampIssueFlags;
					Destination.Header.bStatefulProcessingReset =
						Slot.PendingTimestampIssueFlags != 0;
					Destination.Header.bUnitsNormalized = true;
					Slot.PendingTimestampIssueFlags = 0;
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
				&& Slot.DeliveryMode ==
					EOpenMobileSensorDeliveryMode::EventBatches
				&& HasPendingSamples(Slot))
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
					|| Slot.DeliveryMode !=
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
}

void FOpenMobileSensorsSampleService::BeginShutdown()
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	bShuttingDown.Store(true);
	CancelEventTicker();
	UnregisterAll();
	VectorBatchEvent.Clear();
	AttitudeBatchEvent.Clear();
	ScalarBatchEvent.Clear();
	HeadingBatchEvent.Clear();
	StepsBatchEvent.Clear();
	ActivityBatchEvent.Clear();
	OrientationBatchEvent.Clear();
	ProximityBatchEvent.Clear();
}

void FOpenMobileSensorsSampleService::RegisterSubscription(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorSubscriptionHandle& Handle,
	const FOpenMobileSensorIdentifier& Sensor,
	const FOpenMobileSensorStreamOptions& Options,
	uint64 BackendGeneration
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	if (bShuttingDown.Load()
		|| !OwnerIdentifier.IsValid()
		|| !Handle.IsValid()
		|| !Sensor.IsValid()
		|| BackendGeneration == 0)
	{
		return;
	}
	TUniquePtr<FLatestSlot> Slot = MakeUnique<FLatestSlot>();
	Slot->OwnerIdentifier = OwnerIdentifier;
	Slot->Handle = Handle;
	Slot->Sensor = Sensor;
	Slot->BackendGeneration = BackendGeneration;
	Slot->ExpectedFamily = GetExpectedFamily(Sensor.Type);
	Slot->StaleAfterSeconds = GetStaleAfterSeconds(Options);
	Slot->AppliedSampleFrequencyHz = Options.CustomFrequencyHz;
	Slot->MaximumCallbackFrequencyHz = Options.MaximumCallbackFrequencyHz;
	Slot->MaximumPendingSamples = FMath::Clamp(
		Options.BufferCapacitySamples,
		1,
		4096
	);
	Slot->DeliveryMode = Options.DeliveryMode;
	Slot->OverflowPolicy = Options.OverflowPolicy;
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
	(*SlotPointer)->StaleAfterSeconds = GetStaleAfterSeconds(Options);
	(*SlotPointer)->AppliedSampleFrequencyHz = Options.CustomFrequencyHz;
	ResetRateStatistics(**SlotPointer);
	(*SlotPointer)->MaximumCallbackFrequencyHz =
		Options.MaximumCallbackFrequencyHz;
	(*SlotPointer)->MaximumPendingSamples = FMath::Clamp(
		Options.BufferCapacitySamples,
		1,
		4096
	);
	(*SlotPointer)->DeliveryMode = Options.DeliveryMode;
	(*SlotPointer)->OverflowPolicy = Options.OverflowPolicy;
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
	PublishVector,
	FOpenMobileVectorSensorSample,
	Vector,
	Vector,
	PendingVector,
	BufferedVector
)
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
	PublishActivity,
	FOpenMobileActivitySensorSample,
	Activity,
	Activity,
	PendingActivity,
	BufferedActivity
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
	PublishVectorBatch,
	FOpenMobileVectorSensorBatch,
	FOpenMobileVectorSensorSample,
	Vector,
	Vector,
	PendingVector,
	BufferedVector
)
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
	PublishActivityBatch,
	FOpenMobileActivitySensorBatch,
	FOpenMobileActivitySensorSample,
	Activity,
	Activity,
	PendingActivity,
	BufferedActivity
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
	PublishVectorBatchFromBackend,
	FOpenMobileVectorSensorBatch,
	FOpenMobileVectorSensorSample,
	Vector,
	Vector,
	PendingVector,
	BufferedVector
)
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
	PublishActivityBatchFromBackend,
	FOpenMobileActivitySensorBatch,
	FOpenMobileActivitySensorSample,
	Activity,
	Activity,
	PendingActivity,
	BufferedActivity
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
	VectorBatchEvent.Clear();
	AttitudeBatchEvent.Clear();
	ScalarBatchEvent.Clear();
	HeadingBatchEvent.Clear();
	StepsBatchEvent.Clear();
	ActivityBatchEvent.Clear();
	OrientationBatchEvent.Clear();
	ProximityBatchEvent.Clear();
}
#endif
