#include "OpenMobileSensorsSampleService.h"

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
		FOpenMobileSensorIdentifier Sensor;
		EOpenMobileSensorSubscriptionState State =
			EOpenMobileSensorSubscriptionState::Accepted;
		ELatestSampleFamily Family = ELatestSampleFamily::None;
		int64 NextSequence = 1;
		double LatestTimestampSeconds = 0.0;
		double StaleAfterSeconds = 1.0;
		bool bHasSample = false;
		FOpenMobileVectorSensorSample Vector;
		FOpenMobileAttitudeSensorSample Attitude;
		FOpenMobileScalarSensorSample Scalar;
		FOpenMobileHeadingSensorSample Heading;
		FOpenMobileStepsSensorSample Steps;
		FOpenMobileActivitySensorSample Activity;
		FOpenMobileOrientationSensorSample Orientation;
		FOpenMobileProximitySensorSample Proximity;
	};

	FRWLock SlotsLock;
	TMap<
		FOpenMobileSensorSubscriptionHandle,
		TUniquePtr<FLatestSlot>
	> Slots;

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
	void PublishSample(
		const SampleType& Sample,
		ELatestSampleFamily Family,
		SampleType FLatestSlot::* Member
	)
	{
		if (!Sample.Header.bValid
			|| !FMath::IsFinite(Sample.Header.TimestampSeconds)
			|| Sample.Header.TimestampSeconds < 0.0)
		{
			return;
		}
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
}

void FOpenMobileSensorsSampleService::Start()
{
	UnregisterAll();
}

void FOpenMobileSensorsSampleService::BeginShutdown()
{
	UnregisterAll();
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
	Slot->Sensor = Sensor;
	Slot->StaleAfterSeconds = GetStaleAfterSeconds(Options);
	FWriteScopeLock RegistryLock(SlotsLock);
	Slots.Add(Handle, MoveTemp(Slot));
}

void FOpenMobileSensorsSampleService::SetSubscriptionState(
	const FOpenMobileSensorSubscriptionHandle& Handle,
	EOpenMobileSensorSubscriptionState State
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
	(*SlotPointer)->State = State;
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

#define OPENMOBILE_IMPLEMENT_PUBLISH(MethodName, SampleType, FamilyName, Member) \
	void FOpenMobileSensorsSampleService::MethodName(const SampleType& Sample) \
	{ \
		OpenMobileSensorsSampleServicePrivate::PublishSample( \
			Sample, \
			OpenMobileSensorsSampleServicePrivate::ELatestSampleFamily::FamilyName, \
			&OpenMobileSensorsSampleServicePrivate::FLatestSlot::Member \
		); \
	}

OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishVector,
	FOpenMobileVectorSensorSample,
	Vector,
	Vector
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishAttitude,
	FOpenMobileAttitudeSensorSample,
	Attitude,
	Attitude
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishScalar,
	FOpenMobileScalarSensorSample,
	Scalar,
	Scalar
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishHeading,
	FOpenMobileHeadingSensorSample,
	Heading,
	Heading
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishSteps,
	FOpenMobileStepsSensorSample,
	Steps,
	Steps
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishActivity,
	FOpenMobileActivitySensorSample,
	Activity,
	Activity
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishOrientation,
	FOpenMobileOrientationSensorSample,
	Orientation,
	Orientation
)
OPENMOBILE_IMPLEMENT_PUBLISH(
	PublishProximity,
	FOpenMobileProximitySensorSample,
	Proximity,
	Proximity
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

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileSensorsSampleService::ResetForTests()
{
	UnregisterAll();
}
#endif
