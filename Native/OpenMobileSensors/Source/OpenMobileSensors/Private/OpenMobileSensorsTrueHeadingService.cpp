#include "OpenMobileSensorsTrueHeadingService.h"

#include "OpenMobileSensorDeclination.h"
#include "Misc/DateTime.h"
#include "Misc/ScopeRWLock.h"

namespace OpenMobileSensorsTrueHeadingServicePrivate
{
constexpr double MaximumLocationAgeSeconds = 60.0;
constexpr double MaximumFutureToleranceSeconds = 5.0;
constexpr double MaximumHorizontalAccuracyMeters = 100.0;

struct FLocationState
{
	FOpenMobileSensorLocationInput Input;
	FOpenMobileSensorDeclinationResult Declination;
	double AgeAtCaptureSeconds = 0.0;
	double MonotonicCaptureSeconds = 0.0;
	EOpenMobileSensorFailureReason InputFailureReason =
		EOpenMobileSensorFailureReason::MissingLocationInput;
	uint64 Revision = 0;
	bool bHasRetainedInput = false;
};

FRWLock LocationStatesLock;
TMap<FGuid, FLocationState> LocationStates;
uint64 NextLocationRevision = 1;

uint64 TakeLocationRevisionLocked()
{
	const uint64 Revision = NextLocationRevision++;
	if (NextLocationRevision == 0)
	{
		NextLocationRevision = 1;
	}
	return Revision;
}

void ScrubSensitiveInput(
	FLocationState& State,
	EOpenMobileSensorFailureReason FailureReason
)
{
	State.Input = {};
	State.Declination = {};
	State.AgeAtCaptureSeconds = 0.0;
	State.MonotonicCaptureSeconds = 0.0;
	State.InputFailureReason = FailureReason;
	State.bHasRetainedInput = false;
}

void StoreUnavailableState(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorFailureReason FailureReason
)
{
	FLocationState State;
	ScrubSensitiveInput(State, FailureReason);
	FWriteScopeLock Lock(LocationStatesLock);
	State.Revision = TakeLocationRevisionLocked();
	LocationStates.Add(OwnerIdentifier, MoveTemp(State));
}

bool IsStructurallyValid(const FOpenMobileSensorLocationInput& Input)
{
	return FMath::IsFinite(Input.LatitudeDegrees)
		&& FMath::IsFinite(Input.LongitudeDegrees)
		&& FMath::IsFinite(Input.AltitudeMeters)
		&& FMath::IsFinite(Input.HorizontalAccuracyMeters)
		&& FMath::IsFinite(Input.TimestampSeconds)
		&& Input.LatitudeDegrees >= -90.0
		&& Input.LatitudeDegrees <= 90.0
		&& Input.LongitudeDegrees >= -180.0
		&& Input.LongitudeDegrees <= 180.0
		&& Input.AltitudeMeters >= -1000.0
		&& Input.AltitudeMeters <= 850000.0
		&& Input.HorizontalAccuracyMeters >= 0.0
		&& Input.TimestampSeconds > 0.0;
}

bool ToDecimalYear(double UnixSeconds, double& OutDecimalYear)
{
	if (!FMath::IsFinite(UnixSeconds)
		|| UnixSeconds < static_cast<double>(MIN_int64)
		|| UnixSeconds > static_cast<double>(MAX_int64))
	{
		return false;
	}
	const int64 WholeSeconds = FMath::FloorToInt64(UnixSeconds);
	const FDateTime Timestamp = FDateTime::FromUnixTimestamp(WholeSeconds);
	const FDateTime YearStart(Timestamp.GetYear(), 1, 1);
	const FDateTime NextYearStart(Timestamp.GetYear() + 1, 1, 1);
	const double ElapsedSeconds = (Timestamp - YearStart).GetTotalSeconds()
		+ (UnixSeconds - static_cast<double>(WholeSeconds));
	const double YearSeconds = (NextYearStart - YearStart).GetTotalSeconds();
	OutDecimalYear = static_cast<double>(Timestamp.GetYear())
		+ ElapsedSeconds / YearSeconds;
	return FMath::IsFinite(OutDecimalYear);
}

EOpenMobileSensorFailureReason GetUsableState(
	const FGuid& OwnerIdentifier,
	double CurrentMonotonicSeconds,
	FLocationState& OutState,
	double& OutLocationAgeSeconds
)
{
	OutState = {};
	OutLocationAgeSeconds = 0.0;
	if (!OwnerIdentifier.IsValid()
		|| !FMath::IsFinite(CurrentMonotonicSeconds)
		|| CurrentMonotonicSeconds < 0.0)
	{
		return EOpenMobileSensorFailureReason::InvalidRequest;
	}
	for (;;)
	{
		FLocationState Candidate;
		{
			FReadScopeLock Lock(LocationStatesLock);
			const FLocationState* State = LocationStates.Find(OwnerIdentifier);
			if (!State)
			{
				return EOpenMobileSensorFailureReason::MissingLocationInput;
			}
			if (!State->bHasRetainedInput)
			{
				return State->InputFailureReason;
			}
			Candidate = *State;
			OutLocationAgeSeconds = State->AgeAtCaptureSeconds
				+ FMath::Max(
					0.0,
					CurrentMonotonicSeconds
						- State->MonotonicCaptureSeconds
				);
			if (OutLocationAgeSeconds <= MaximumLocationAgeSeconds)
			{
				OutState = Candidate;
				return EOpenMobileSensorFailureReason::None;
			}
		}

		FWriteScopeLock Lock(LocationStatesLock);
		FLocationState* Current = LocationStates.Find(OwnerIdentifier);
		if (!Current)
		{
			return EOpenMobileSensorFailureReason::MissingLocationInput;
		}
		if (!Current->bHasRetainedInput)
		{
			return Current->InputFailureReason;
		}
		if (Current->Revision != Candidate.Revision)
		{
			continue;
		}
		ScrubSensitiveInput(
			*Current,
			EOpenMobileSensorFailureReason::StaleLocationInput
		);
		return EOpenMobileSensorFailureReason::StaleLocationInput;
	}
}
}

EOpenMobileSensorFailureReason
FOpenMobileSensorsTrueHeadingService::SetLocationInput(
	const FGuid& OwnerIdentifier,
	const FOpenMobileSensorLocationInput& LocationInput,
	double CurrentUnixSeconds,
	double CurrentMonotonicSeconds
)
{
	using namespace OpenMobileSensorsTrueHeadingServicePrivate;
	if (!OwnerIdentifier.IsValid()
		|| !IsStructurallyValid(LocationInput)
		|| !FMath::IsFinite(CurrentUnixSeconds)
		|| !FMath::IsFinite(CurrentMonotonicSeconds)
		|| CurrentUnixSeconds <= 0.0
		|| CurrentMonotonicSeconds < 0.0)
	{
		return EOpenMobileSensorFailureReason::InvalidRequest;
	}
	const double AgeSeconds = CurrentUnixSeconds
		- LocationInput.TimestampSeconds;
	if (AgeSeconds < -MaximumFutureToleranceSeconds)
	{
		return EOpenMobileSensorFailureReason::InvalidRequest;
	}
	if (AgeSeconds > MaximumLocationAgeSeconds)
	{
		StoreUnavailableState(
			OwnerIdentifier,
			EOpenMobileSensorFailureReason::StaleLocationInput
		);
		return EOpenMobileSensorFailureReason::StaleLocationInput;
	}
	if (LocationInput.HorizontalAccuracyMeters
		> MaximumHorizontalAccuracyMeters)
	{
		StoreUnavailableState(
			OwnerIdentifier,
			EOpenMobileSensorFailureReason::PoorLocationAccuracy
		);
		return EOpenMobileSensorFailureReason::PoorLocationAccuracy;
	}
	double DecimalYear = 0.0;
	FOpenMobileSensorDeclinationResult Declination;
	if (!ToDecimalYear(LocationInput.TimestampSeconds, DecimalYear)
		|| !FOpenMobileSensorDeclination::CalculateWMM2025(
			LocationInput.LatitudeDegrees,
			LocationInput.LongitudeDegrees,
			LocationInput.AltitudeMeters,
			DecimalYear,
			Declination
		))
	{
		StoreUnavailableState(
			OwnerIdentifier,
			EOpenMobileSensorFailureReason::DerivedInputUnavailable
		);
		return EOpenMobileSensorFailureReason::DerivedInputUnavailable;
	}
	FLocationState State;
	State.Input = LocationInput;
	State.Declination = Declination;
	State.AgeAtCaptureSeconds = FMath::Max(0.0, AgeSeconds);
	State.MonotonicCaptureSeconds = CurrentMonotonicSeconds;
	State.InputFailureReason = EOpenMobileSensorFailureReason::None;
	State.bHasRetainedInput = true;
	FWriteScopeLock Lock(LocationStatesLock);
	State.Revision = TakeLocationRevisionLocked();
	LocationStates.Add(OwnerIdentifier, MoveTemp(State));
	return EOpenMobileSensorFailureReason::None;
}

EOpenMobileSensorFailureReason
FOpenMobileSensorsTrueHeadingService::GetLocationInputState(
	const FGuid& OwnerIdentifier,
	double CurrentMonotonicSeconds
)
{
	using namespace OpenMobileSensorsTrueHeadingServicePrivate;
	FLocationState State;
	double LocationAgeSeconds = 0.0;
	return GetUsableState(
		OwnerIdentifier,
		CurrentMonotonicSeconds,
		State,
		LocationAgeSeconds
	);
}

EOpenMobileSensorFailureReason
FOpenMobileSensorsTrueHeadingService::GetUsableLocationInput(
	const FGuid& OwnerIdentifier,
	double CurrentMonotonicSeconds,
	FOpenMobileSensorLocationInput& OutLocationInput,
	double& OutLocationAgeSeconds
)
{
	using namespace OpenMobileSensorsTrueHeadingServicePrivate;
	OutLocationInput = {};
	OutLocationAgeSeconds = 0.0;
	FLocationState State;
	const EOpenMobileSensorFailureReason Result = GetUsableState(
		OwnerIdentifier,
		CurrentMonotonicSeconds,
		State,
		OutLocationAgeSeconds
	);
	if (Result == EOpenMobileSensorFailureReason::None)
	{
		OutLocationInput = State.Input;
	}
	return Result;
}

EOpenMobileSensorFailureReason
FOpenMobileSensorsTrueHeadingService::ConvertMagneticHeading(
	const FGuid& OwnerIdentifier,
	double CurrentMonotonicSeconds,
	const FOpenMobileHeadingSensorSample& MagneticHeading,
	FOpenMobileHeadingSensorSample& OutTrueHeading
)
{
	using namespace OpenMobileSensorsTrueHeadingServicePrivate;
	OutTrueHeading = {};
	if (!MagneticHeading.Header.bValid
		|| MagneticHeading.Header.Sensor.Type !=
			EOpenMobileSensorType::MagneticHeading
		|| MagneticHeading.Reference !=
			EOpenMobileHeadingReference::MagneticNorth
		|| !FMath::IsFinite(MagneticHeading.HeadingDegrees)
		|| MagneticHeading.HeadingDegrees < 0.0
		|| MagneticHeading.HeadingDegrees >= 360.0
		|| (MagneticHeading.bHasAccuracyDegrees
			&& (!FMath::IsFinite(MagneticHeading.AccuracyDegrees)
				|| MagneticHeading.AccuracyDegrees < 0.0)))
	{
		return EOpenMobileSensorFailureReason::DerivedInputUnavailable;
	}
	FLocationState LocationState;
	double LocationAgeSeconds = 0.0;
	const EOpenMobileSensorFailureReason LocationResult =
		GetUsableState(
			OwnerIdentifier,
			CurrentMonotonicSeconds,
			LocationState,
			LocationAgeSeconds
		);
	if (LocationResult != EOpenMobileSensorFailureReason::None)
	{
		return LocationResult;
	}
	OutTrueHeading = MagneticHeading;
	OutTrueHeading.Header.Sensor.Type = EOpenMobileSensorType::TrueHeading;
	OutTrueHeading.HeadingDegrees = FMath::Fmod(
		MagneticHeading.HeadingDegrees
			+ LocationState.Declination.DeclinationDegrees + 360.0,
		360.0
	);
	OutTrueHeading.Reference = EOpenMobileHeadingReference::TrueNorth;
	OutTrueHeading.DeclinationSource =
		EOpenMobileHeadingDeclinationSource::WorldMagneticModel2025;
	OutTrueHeading.bHasDeclinationDegrees = true;
	OutTrueHeading.DeclinationDegrees =
		LocationState.Declination.DeclinationDegrees;
	OutTrueHeading.bHasLocationAgeSeconds = true;
	OutTrueHeading.LocationAgeSeconds = LocationAgeSeconds;
	OutTrueHeading.bHasAccuracyDegrees = true;
	OutTrueHeading.AccuracyDegrees = MagneticHeading.bHasAccuracyDegrees
		? FMath::Sqrt(
			FMath::Square(MagneticHeading.AccuracyDegrees)
			+ FMath::Square(
				LocationState.Declination.EstimatedErrorDegrees
			)
		)
		: LocationState.Declination.EstimatedErrorDegrees;
	OutTrueHeading.Header.bHasEstimatedError = true;
	OutTrueHeading.Header.EstimatedError = OutTrueHeading.AccuracyDegrees;
	OutTrueHeading.Header.SourceFlags &= ~(
		static_cast<int32>(EOpenMobileSensorSourceFlags::Raw)
		| static_cast<int32>(
			EOpenMobileSensorSourceFlags::CalibratedNative
		)
		| static_cast<int32>(EOpenMobileSensorSourceFlags::NativeFused)
		| static_cast<int32>(
			EOpenMobileSensorSourceFlags::MagneticNorthReferenced
		)
	);
	OutTrueHeading.Header.SourceFlags |= static_cast<int32>(
		EOpenMobileSensorSourceFlags::PluginDerived
	) | static_cast<int32>(
		EOpenMobileSensorSourceFlags::TrueNorthReferenced
	);
	return EOpenMobileSensorFailureReason::None;
}

EOpenMobileSensorFailureReason
FOpenMobileSensorsTrueHeadingService::AnnotateNativeHeading(
	const FGuid& OwnerIdentifier,
	double CurrentMonotonicSeconds,
	FOpenMobileHeadingSensorSample& NativeHeading
)
{
	if (NativeHeading.Header.Sensor.Type !=
			EOpenMobileSensorType::TrueHeading
		|| NativeHeading.Reference != EOpenMobileHeadingReference::TrueNorth)
	{
		return EOpenMobileSensorFailureReason::DerivedInputUnavailable;
	}
	FOpenMobileSensorLocationInput LocationInput;
	double LocationAgeSeconds = 0.0;
	const EOpenMobileSensorFailureReason LocationResult =
		GetUsableLocationInput(
			OwnerIdentifier,
			CurrentMonotonicSeconds,
			LocationInput,
			LocationAgeSeconds
		);
	if (LocationResult != EOpenMobileSensorFailureReason::None)
	{
		return LocationResult;
	}
	NativeHeading.DeclinationSource =
		EOpenMobileHeadingDeclinationSource::NativePlatform;
	NativeHeading.bHasDeclinationDegrees = false;
	NativeHeading.bHasLocationAgeSeconds = true;
	NativeHeading.LocationAgeSeconds = LocationAgeSeconds;
	NativeHeading.Header.SourceFlags &= ~static_cast<int32>(
		EOpenMobileSensorSourceFlags::MagneticNorthReferenced
	);
	NativeHeading.Header.SourceFlags |= static_cast<int32>(
		EOpenMobileSensorSourceFlags::TrueNorthReferenced
	);
	return EOpenMobileSensorFailureReason::None;
}

void FOpenMobileSensorsTrueHeadingService::RemoveOwner(
	const FGuid& OwnerIdentifier
)
{
	using namespace OpenMobileSensorsTrueHeadingServicePrivate;
	FWriteScopeLock Lock(LocationStatesLock);
	LocationStates.Remove(OwnerIdentifier);
}

bool FOpenMobileSensorsTrueHeadingService::ClearLocationInput(
	const FGuid& OwnerIdentifier
)
{
	using namespace OpenMobileSensorsTrueHeadingServicePrivate;
	if (!OwnerIdentifier.IsValid())
	{
		return false;
	}
	FWriteScopeLock Lock(LocationStatesLock);
	return LocationStates.Remove(OwnerIdentifier) > 0;
}

bool FOpenMobileSensorsTrueHeadingService::HasAnyLocationInput(
	double CurrentMonotonicSeconds
)
{
	using namespace OpenMobileSensorsTrueHeadingServicePrivate;
	if (!FMath::IsFinite(CurrentMonotonicSeconds)
		|| CurrentMonotonicSeconds < 0.0)
	{
		return false;
	}
	FWriteScopeLock Lock(LocationStatesLock);
	bool bHasUsableInput = false;
	for (TPair<FGuid, FLocationState>& Pair : LocationStates)
	{
		FLocationState& State = Pair.Value;
		if (!State.bHasRetainedInput)
		{
			continue;
		}
		const double AgeSeconds = State.AgeAtCaptureSeconds
			+ FMath::Max(
				0.0,
				CurrentMonotonicSeconds - State.MonotonicCaptureSeconds
			);
		if (AgeSeconds <= MaximumLocationAgeSeconds)
		{
			bHasUsableInput = true;
			continue;
		}
		ScrubSensitiveInput(
			State,
			EOpenMobileSensorFailureReason::StaleLocationInput
		);
	}
	return bHasUsableInput;
}

double FOpenMobileSensorsTrueHeadingService::GetMaximumLocationAgeSeconds()
{
	return OpenMobileSensorsTrueHeadingServicePrivate::
		MaximumLocationAgeSeconds;
}

double FOpenMobileSensorsTrueHeadingService::
GetMaximumHorizontalAccuracyMeters()
{
	return OpenMobileSensorsTrueHeadingServicePrivate::
		MaximumHorizontalAccuracyMeters;
}

#if WITH_DEV_AUTOMATION_TESTS
bool FOpenMobileSensorsTrueHeadingService::
HasRetainedLocationInputForTests(const FGuid& OwnerIdentifier)
{
	using namespace OpenMobileSensorsTrueHeadingServicePrivate;
	FReadScopeLock Lock(LocationStatesLock);
	const FLocationState* State = LocationStates.Find(OwnerIdentifier);
	return State && State->bHasRetainedInput;
}

void FOpenMobileSensorsTrueHeadingService::ResetForTests()
{
	using namespace OpenMobileSensorsTrueHeadingServicePrivate;
	FWriteScopeLock Lock(LocationStatesLock);
	LocationStates.Reset();
	NextLocationRevision = 1;
}
#endif
