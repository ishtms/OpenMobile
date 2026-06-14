#include "OpenMobileSensorScreenRotation.h"

#include "OpenMobileSensorScreenRotationService.h"

#include "Misc/ScopeRWLock.h"
#include "OpenMobileSensorCoordinates.h"

namespace OpenMobileSensorScreenRotationPrivate
{
	constexpr int32 MaximumRotationHistory = 16;
	FRWLock RotationLock;
	TMap<FGuid, TArray<FOpenMobileSensorScreenRotationSnapshot>>
		RotationHistories;
	int64 NextRotationSequence = 1;

	bool IsValidRotation(EOpenMobileSensorScreenRotation Rotation)
	{
		switch (Rotation)
		{
		case EOpenMobileSensorScreenRotation::Rotation0:
		case EOpenMobileSensorScreenRotation::Rotation90:
		case EOpenMobileSensorScreenRotation::Rotation180:
		case EOpenMobileSensorScreenRotation::Rotation270:
			return true;
		default:
			return false;
		}
	}

	bool ResolveHistory(
		const TArray<FOpenMobileSensorScreenRotationSnapshot>& History,
		double SampleTimestampSeconds,
		FOpenMobileSensorScreenRotationSnapshot& OutSnapshot
	)
	{
		for (int32 Index = History.Num() - 1; Index >= 0; --Index)
		{
			if (History[Index].TimestampSeconds <= SampleTimestampSeconds)
			{
				OutSnapshot = History[Index];
				return true;
			}
		}
		return false;
	}

	void ApplySnapshot(
		FOpenMobileSensorSampleHeader& Header,
		const FOpenMobileSensorScreenRotationSnapshot& Snapshot
	)
	{
		Header.CoordinateSpace = EOpenMobileSensorCoordinateSpace::CurrentScreen;
		Header.ScreenRotation = Snapshot.Rotation;
		Header.ScreenRotationTimestampSeconds = Snapshot.TimestampSeconds;
		Header.ScreenRotationSequence = Snapshot.Sequence;
		Header.bNaturalOrientationLandscape =
			Snapshot.bNaturalOrientationLandscape;
	}

	double RotationDegrees(EOpenMobileSensorScreenRotation Rotation)
	{
		return static_cast<double>(static_cast<uint8>(Rotation)) * 90.0;
	}

	void ResolveAndApply(
		const FGuid& OwnerIdentifier,
		FOpenMobileSensorSampleHeader& Header,
		FOpenMobileSensorScreenRotationSnapshot& OutSnapshot
	)
	{
		FOpenMobileSensorsScreenRotationService::ResolveRotation(
			OwnerIdentifier,
			Header.TimestampSeconds,
			OutSnapshot
		);
		ApplySnapshot(Header, OutSnapshot);
	}
}

bool FOpenMobileSensorsScreenRotationService::
CaptureApplicationWindowRotation(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorScreenRotation Rotation,
	double TimestampSeconds,
	bool bNaturalOrientationLandscape
)
{
	using namespace OpenMobileSensorScreenRotationPrivate;
	if (!IsValidRotation(Rotation)
		|| !FMath::IsFinite(TimestampSeconds)
		|| TimestampSeconds < 0.0)
	{
		return false;
	}
	FWriteScopeLock Lock(RotationLock);
	TArray<FOpenMobileSensorScreenRotationSnapshot>& History =
		RotationHistories.FindOrAdd(OwnerIdentifier);
	if (!History.IsEmpty()
		&& TimestampSeconds < History.Last().TimestampSeconds)
	{
		return false;
	}
	FOpenMobileSensorScreenRotationSnapshot Snapshot;
	Snapshot.Rotation = Rotation;
	Snapshot.TimestampSeconds = TimestampSeconds;
	Snapshot.Sequence = NextRotationSequence++;
	Snapshot.bNaturalOrientationLandscape = bNaturalOrientationLandscape;
	History.Add(Snapshot);
	if (History.Num() > MaximumRotationHistory)
	{
		History.RemoveAt(
			0,
			History.Num() - MaximumRotationHistory,
			EAllowShrinking::No
		);
	}
	return true;
}

bool FOpenMobileSensorsScreenRotationService::ResolveRotation(
	const FGuid& OwnerIdentifier,
	double SampleTimestampSeconds,
	FOpenMobileSensorScreenRotationSnapshot& OutSnapshot
)
{
	using namespace OpenMobileSensorScreenRotationPrivate;
	OutSnapshot = {};
	if (!FMath::IsFinite(SampleTimestampSeconds)
		|| SampleTimestampSeconds < 0.0)
	{
		return false;
	}
	FReadScopeLock Lock(RotationLock);
	if (const TArray<FOpenMobileSensorScreenRotationSnapshot>* History =
		RotationHistories.Find(OwnerIdentifier))
	{
		if (ResolveHistory(*History, SampleTimestampSeconds, OutSnapshot))
		{
			return true;
		}
	}
	if (OwnerIdentifier.IsValid())
	{
		if (const TArray<FOpenMobileSensorScreenRotationSnapshot>* Global =
			RotationHistories.Find(FGuid()))
		{
			return ResolveHistory(
				*Global, SampleTimestampSeconds, OutSnapshot);
		}
	}
	return false;
}

void FOpenMobileSensorsScreenRotationService::RemoveOwner(
	const FGuid& OwnerIdentifier
)
{
	using namespace OpenMobileSensorScreenRotationPrivate;
	FWriteScopeLock Lock(RotationLock);
	RotationHistories.Remove(OwnerIdentifier);
}

void FOpenMobileSensorsScreenRotationService::Reset()
{
	using namespace OpenMobileSensorScreenRotationPrivate;
	FWriteScopeLock Lock(RotationLock);
	RotationHistories.Reset();
	NextRotationSequence = 1;
}

FVector FOpenMobileSensorsScreenRotationService::RotateVector(
	const FVector& DeviceVector,
	EOpenMobileSensorScreenRotation Rotation
)
{
	switch (Rotation)
	{
	case EOpenMobileSensorScreenRotation::Rotation90:
		return FVector(DeviceVector.Y, -DeviceVector.X, DeviceVector.Z);
	case EOpenMobileSensorScreenRotation::Rotation180:
		return FVector(-DeviceVector.X, -DeviceVector.Y, DeviceVector.Z);
	case EOpenMobileSensorScreenRotation::Rotation270:
		return FVector(-DeviceVector.Y, DeviceVector.X, DeviceVector.Z);
	case EOpenMobileSensorScreenRotation::Rotation0:
	default:
		return DeviceVector;
	}
}

void FOpenMobileSensorsScreenRotationService::ApplyToSample(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	FOpenMobileVectorSensorSample& Sample
)
{
	using namespace OpenMobileSensorScreenRotationPrivate;
	if (CoordinateSpace != EOpenMobileSensorCoordinateSpace::CurrentScreen)
	{
		return;
	}
	FOpenMobileSensorScreenRotationSnapshot Snapshot;
	ResolveAndApply(OwnerIdentifier, Sample.Header, Snapshot);
	Sample.Value = RotateVector(Sample.Value, Snapshot.Rotation);
	if (Sample.bHasBias)
	{
		Sample.Bias = RotateVector(Sample.Bias, Snapshot.Rotation);
	}
}

void FOpenMobileSensorsScreenRotationService::ApplyToSample(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	FOpenMobileAttitudeSensorSample& Sample
)
{
	using namespace OpenMobileSensorScreenRotationPrivate;
	if (CoordinateSpace != EOpenMobileSensorCoordinateSpace::CurrentScreen)
	{
		return;
	}
	FOpenMobileSensorScreenRotationSnapshot Snapshot;
	ResolveAndApply(OwnerIdentifier, Sample.Header, Snapshot);
	const FVector VectorPart = RotateVector(
		FVector(Sample.Quaternion.X, Sample.Quaternion.Y, Sample.Quaternion.Z),
		Snapshot.Rotation
	);
	Sample.Quaternion = FQuat(
		VectorPart.X,
		VectorPart.Y,
		VectorPart.Z,
		Sample.Quaternion.W
	).GetNormalized();
	FOpenMobileSensorCoordinateConverter::UpdateEulerAndRotationMatrix(Sample);
}

void FOpenMobileSensorsScreenRotationService::ApplyToSample(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	FOpenMobileScalarSensorSample& Sample
)
{
	if (CoordinateSpace == EOpenMobileSensorCoordinateSpace::CurrentScreen)
	{
		FOpenMobileSensorScreenRotationSnapshot Snapshot;
		OpenMobileSensorScreenRotationPrivate::ResolveAndApply(
			OwnerIdentifier, Sample.Header, Snapshot);
	}
}

void FOpenMobileSensorsScreenRotationService::ApplyToSample(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	FOpenMobileHeadingSensorSample& Sample
)
{
	using namespace OpenMobileSensorScreenRotationPrivate;
	if (CoordinateSpace != EOpenMobileSensorCoordinateSpace::CurrentScreen)
	{
		return;
	}
	FOpenMobileSensorScreenRotationSnapshot Snapshot;
	ResolveAndApply(OwnerIdentifier, Sample.Header, Snapshot);
	Sample.HeadingDegrees = FMath::Fmod(
		Sample.HeadingDegrees + RotationDegrees(Snapshot.Rotation),
		360.0
	);
	if (Sample.HeadingDegrees < 0.0)
	{
		Sample.HeadingDegrees += 360.0;
	}
}

void FOpenMobileSensorsScreenRotationService::ApplyToSample(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	FOpenMobileStepsSensorSample& Sample
)
{
	if (CoordinateSpace == EOpenMobileSensorCoordinateSpace::CurrentScreen)
	{
		FOpenMobileSensorScreenRotationSnapshot Snapshot;
		OpenMobileSensorScreenRotationPrivate::ResolveAndApply(
			OwnerIdentifier, Sample.Header, Snapshot);
	}
}

void FOpenMobileSensorsScreenRotationService::ApplyToSample(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	FOpenMobileActivitySensorSample& Sample
)
{
	if (CoordinateSpace == EOpenMobileSensorCoordinateSpace::CurrentScreen)
	{
		FOpenMobileSensorScreenRotationSnapshot Snapshot;
		OpenMobileSensorScreenRotationPrivate::ResolveAndApply(
			OwnerIdentifier, Sample.Header, Snapshot);
	}
}

void FOpenMobileSensorsScreenRotationService::ApplyToSample(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	FOpenMobileOrientationSensorSample& Sample
)
{
	static_cast<void>(OwnerIdentifier);
	static_cast<void>(CoordinateSpace);
	Sample.Header.CoordinateSpace =
		EOpenMobileSensorCoordinateSpace::DeviceFixed;
}

void FOpenMobileSensorsScreenRotationService::ApplyToSample(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorCoordinateSpace CoordinateSpace,
	FOpenMobileProximitySensorSample& Sample
)
{
	if (CoordinateSpace == EOpenMobileSensorCoordinateSpace::CurrentScreen)
	{
		FOpenMobileSensorScreenRotationSnapshot Snapshot;
		OpenMobileSensorScreenRotationPrivate::ResolveAndApply(
			OwnerIdentifier, Sample.Header, Snapshot);
	}
}
