#include "OpenMobileSensorsSampleService.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
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

	struct FLatestSlot
	{
		FCriticalSection Mutex;
		FGuid OwnerIdentifier;
		FOpenMobileSensorSubscriptionHandle Handle;
		FOpenMobileSensorIdentifier Sensor;
		EOpenMobileSensorSubscriptionState State =
			EOpenMobileSensorSubscriptionState::Accepted;
		ELatestSampleFamily Family = ELatestSampleFamily::None;
		int64 NextSequence = 1;
		double LatestTimestampSeconds = 0.0;
		double StaleAfterSeconds = 1.0;
		double MaximumCallbackFrequencyHz = 15.0;
		double LastCallbackTimeSeconds = 0.0;
		int32 MaximumPendingSamples = 128;
		EOpenMobileSensorDeliveryMode DeliveryMode =
			EOpenMobileSensorDeliveryMode::LatestValue;
		EOpenMobileSensorOverflowPolicy OverflowPolicy =
			EOpenMobileSensorOverflowPolicy::DropOldest;
		bool bHasSample = false;
		bool bHasCallbackTime = false;
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
	};

	FRWLock SlotsLock;
	TMap<
		FOpenMobileSensorSubscriptionHandle,
		TUniquePtr<FLatestSlot>
	> Slots;
	FCriticalSection EventTickerMutex;
	FTSTicker::FDelegateHandle EventTickerHandle;
	FOnOpenMobileVectorSensorBatchReady VectorBatchEvent;
	FOnOpenMobileAttitudeSensorBatchReady AttitudeBatchEvent;
	FOnOpenMobileScalarSensorBatchReady ScalarBatchEvent;
	FOnOpenMobileHeadingSensorBatchReady HeadingBatchEvent;
	FOnOpenMobileStepsSensorBatchReady StepsBatchEvent;
	FOnOpenMobileActivitySensorBatchReady ActivityBatchEvent;
	FOnOpenMobileOrientationSensorBatchReady OrientationBatchEvent;
	FOnOpenMobileProximitySensorBatchReady ProximityBatchEvent;

	void EnsureEventTicker();

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
			if (Slot.OverflowPolicy ==
				EOpenMobileSensorOverflowPolicy::RejectNewest)
			{
				return false;
			}
			Pending.RemoveAt(0, 1, EAllowShrinking::No);
		}
		Pending.Add(Sample);
		return true;
	}

	template <typename SampleType>
	void PublishSample(
		const SampleType& Sample,
		ELatestSampleFamily Family,
		SampleType FLatestSlot::* Member,
		TArray<SampleType> FLatestSlot::* PendingMember
	)
	{
		if (!Sample.Header.bValid
			|| !FMath::IsFinite(Sample.Header.TimestampSeconds)
			|| Sample.Header.TimestampSeconds < 0.0)
		{
			return;
		}
		bool bQueuedEvent = false;
		{
			FReadScopeLock RegistryLock(SlotsLock);
			for (TPair<
				FOpenMobileSensorSubscriptionHandle,
				TUniquePtr<FLatestSlot>
			>& Pair : Slots)
			{
				FLatestSlot& Slot = *Pair.Value;
				if (Slot.Sensor != Sample.Header.Sensor)
				{
					continue;
				}
				FScopeLock SlotLock(&Slot.Mutex);
				if (Slot.State != EOpenMobileSensorSubscriptionState::Active
					|| (Slot.bHasSample
						&& Sample.Header.TimestampSeconds <
							Slot.LatestTimestampSeconds))
				{
					continue;
				}
				SampleType& Destination = Slot.*Member;
				Destination = Sample;
				Destination.Header.Sensor = Slot.Sensor;
				Destination.Header.Sequence = Slot.NextSequence++;
				Slot.LatestTimestampSeconds = Sample.Header.TimestampSeconds;
				Slot.Family = Family;
				Slot.bHasSample = true;
				bQueuedEvent |= EnqueueEventSample(
					Slot,
					Destination,
					PendingMember
				);
			}
		}
		if (bQueuedEvent)
		{
			EnsureEventTicker();
		}
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

	template <typename SampleType, typename BatchType>
	void GatherDelivery(
		FLatestSlot& Slot,
		TArray<SampleType> FLatestSlot::* PendingMember,
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
					VectorDeliveries
				);
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingAttitude,
					AttitudeDeliveries
				);
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingScalar,
					ScalarDeliveries
				);
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingHeading,
					HeadingDeliveries
				);
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingSteps,
					StepsDeliveries
				);
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingActivity,
					ActivityDeliveries
				);
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingOrientation,
					OrientationDeliveries
				);
				GatherDelivery(
					Slot,
					&FLatestSlot::PendingProximity,
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
	OpenMobileSensorsSampleServicePrivate::CancelEventTicker();
	UnregisterAll();
}

void FOpenMobileSensorsSampleService::BeginShutdown()
{
	using namespace OpenMobileSensorsSampleServicePrivate;
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
	const FOpenMobileSensorStreamOptions& Options
)
{
	using namespace OpenMobileSensorsSampleServicePrivate;
	if (!OwnerIdentifier.IsValid() || !Handle.IsValid() || !Sensor.IsValid())
	{
		return;
	}
	TUniquePtr<FLatestSlot> Slot = MakeUnique<FLatestSlot>();
	Slot->OwnerIdentifier = OwnerIdentifier;
	Slot->Handle = Handle;
	Slot->Sensor = Sensor;
	Slot->StaleAfterSeconds = GetStaleAfterSeconds(Options);
	Slot->MaximumCallbackFrequencyHz = Options.MaximumCallbackFrequencyHz;
	Slot->MaximumPendingSamples = FMath::Clamp(
		Options.BufferCapacitySamples,
		1,
		4096
	);
	Slot->DeliveryMode = Options.DeliveryMode;
	Slot->OverflowPolicy = Options.OverflowPolicy;
	FWriteScopeLock RegistryLock(SlotsLock);
	Slots.Add(Handle, MoveTemp(Slot));
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
	(*SlotPointer)->StaleAfterSeconds = GetStaleAfterSeconds(Options);
	(*SlotPointer)->MaximumCallbackFrequencyHz =
		Options.MaximumCallbackFrequencyHz;
	(*SlotPointer)->MaximumPendingSamples = FMath::Clamp(
		Options.BufferCapacitySamples,
		1,
		4096
	);
	(*SlotPointer)->DeliveryMode = Options.DeliveryMode;
	(*SlotPointer)->OverflowPolicy = Options.OverflowPolicy;
	if (Options.DeliveryMode != EOpenMobileSensorDeliveryMode::EventBatches)
	{
		(*SlotPointer)->PendingVector.Reset();
		(*SlotPointer)->PendingAttitude.Reset();
		(*SlotPointer)->PendingScalar.Reset();
		(*SlotPointer)->PendingHeading.Reset();
		(*SlotPointer)->PendingSteps.Reset();
		(*SlotPointer)->PendingActivity.Reset();
		(*SlotPointer)->PendingOrientation.Reset();
		(*SlotPointer)->PendingProximity.Reset();
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

#define OPENMOBILE_IMPLEMENT_PUBLISH( \
	MethodName, SampleType, FamilyName, Member, PendingMember \
) \
	void FOpenMobileSensorsSampleService::MethodName(const SampleType& Sample) \
	{ \
		OpenMobileSensorsSampleServicePrivate::PublishSample( \
			Sample, \
			OpenMobileSensorsSampleServicePrivate::ELatestSampleFamily::FamilyName, \
			&OpenMobileSensorsSampleServicePrivate::FLatestSlot::Member, \
			&OpenMobileSensorsSampleServicePrivate::FLatestSlot::PendingMember \
		); \
	}

OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishVector,
	FOpenMobileVectorSensorSample,
	Vector,
	Vector,
	PendingVector
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishAttitude,
	FOpenMobileAttitudeSensorSample,
	Attitude,
	Attitude,
	PendingAttitude
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishScalar,
	FOpenMobileScalarSensorSample,
	Scalar,
	Scalar,
	PendingScalar
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishHeading,
	FOpenMobileHeadingSensorSample,
	Heading,
	Heading,
	PendingHeading
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishSteps,
	FOpenMobileStepsSensorSample,
	Steps,
	Steps,
	PendingSteps
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishActivity,
	FOpenMobileActivitySensorSample,
	Activity,
	Activity,
	PendingActivity
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishOrientation,
	FOpenMobileOrientationSensorSample,
	Orientation,
	Orientation,
	PendingOrientation
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishProximity,
	FOpenMobileProximitySensorSample,
	Proximity,
	Proximity,
	PendingProximity
)

#undef OPENMOBILE_IMPLEMENT_PUBLISH

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
