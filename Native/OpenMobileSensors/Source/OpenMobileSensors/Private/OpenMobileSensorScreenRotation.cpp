#include "OpenMobileSensorScreenRotation.h"

#include "OpenMobileSensorScreenRotationService.h"

#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeRWLock.h"
#include "OpenMobileSensorCoordinates.h"

namespace OpenMobileSensorScreenRotationPrivate
{
	constexpr int32 MaximumRotationHistory = 16;
	FRWLock RotationLock;
	TMap<FGuid, TArray<FOpenMobileSensorScreenRotationSnapshot>>
		RotationHistories;
	int64 NextRotationSequence = 1;
	TOptional<bool> PlatformNaturalOrientationLandscape;

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

	bool TryMapPlatformOrientation(
		EDeviceScreenOrientation Orientation,
		bool bNaturalOrientationLandscape,
		EOpenMobileSensorScreenRotation& OutRotation)
	{
		if (bNaturalOrientationLandscape)
		{
			switch (Orientation)
			{
			case EDeviceScreenOrientation::LandscapeLeft:
				OutRotation = EOpenMobileSensorScreenRotation::Rotation0;
				return true;
			case EDeviceScreenOrientation::PortraitUpsideDown:
				OutRotation = EOpenMobileSensorScreenRotation::Rotation90;
				return true;
			case EDeviceScreenOrientation::LandscapeRight:
				OutRotation = EOpenMobileSensorScreenRotation::Rotation180;
				return true;
			case EDeviceScreenOrientation::Portrait:
				OutRotation = EOpenMobileSensorScreenRotation::Rotation270;
				return true;
			default:
				return false;
			}
		}
		switch (Orientation)
		{
		case EDeviceScreenOrientation::Portrait:
			OutRotation = EOpenMobileSensorScreenRotation::Rotation0;
			return true;
		case EDeviceScreenOrientation::LandscapeLeft:
			OutRotation = EOpenMobileSensorScreenRotation::Rotation90;
			return true;
		case EDeviceScreenOrientation::PortraitUpsideDown:
			OutRotation = EOpenMobileSensorScreenRotation::Rotation180;
			return true;
		case EDeviceScreenOrientation::LandscapeRight:
			OutRotation = EOpenMobileSensorScreenRotation::Rotation270;
			return true;
		default:
			return false;
		}
	}

	bool ResolveAndApply(
		const FGuid& OwnerIdentifier,
		FOpenMobileSensorSampleHeader& Header,
		FOpenMobileSensorScreenRotationSnapshot& OutSnapshot
	)
	{
		if (!FOpenMobileSensorsScreenRotationService::ResolveRotation(
			OwnerIdentifier,
			Header.TimestampSeconds,
			OutSnapshot
		))
		{
			Header.bValid = false;
			return false;
		}
		ApplySnapshot(Header, OutSnapshot);
		return true;
	}
}

bool FOpenMobileSensorsScreenRotationService::CapturePlatformScreenOrientation(
	EDeviceScreenOrientation Orientation,
	bool bNaturalOrientationLandscape,
	double TimestampSeconds)
{
	EOpenMobileSensorScreenRotation Rotation;
	if (!OpenMobileSensorScreenRotationPrivate::TryMapPlatformOrientation(
		Orientation,
		bNaturalOrientationLandscape,
		Rotation))
	{
		return false;
	}
	return CapturePlatformScreenRotation(
		Rotation,
		bNaturalOrientationLandscape,
		TimestampSeconds);
}

bool FOpenMobileSensorsScreenRotationService::CapturePlatformScreenRotation(
	EOpenMobileSensorScreenRotation Rotation,
	bool bNaturalOrientationLandscape,
	double TimestampSeconds)
{
	if (!CaptureApplicationWindowRotation(
		FGuid(), Rotation, TimestampSeconds, bNaturalOrientationLandscape))
	{
		return false;
	}
	FWriteScopeLock Lock(
		OpenMobileSensorScreenRotationPrivate::RotationLock);
	OpenMobileSensorScreenRotationPrivate::
		PlatformNaturalOrientationLandscape = bNaturalOrientationLandscape;
	return true;
}

bool FOpenMobileSensorsScreenRotationService::NotifyCurrentScreenRotation(
	const FGuid& OwnerIdentifier,
	EOpenMobileSensorScreenRotation Rotation)
{
	bool bNaturalOrientationLandscape = false;
	{
		FReadScopeLock Lock(
			OpenMobileSensorScreenRotationPrivate::RotationLock);
		if (!OpenMobileSensorScreenRotationPrivate::
			PlatformNaturalOrientationLandscape.IsSet())
		{
			return false;
		}
		bNaturalOrientationLandscape = OpenMobileSensorScreenRotationPrivate::
			PlatformNaturalOrientationLandscape.GetValue();
	}
	return CaptureApplicationWindowRotation(
		OwnerIdentifier,
		Rotation,
		FPlatformTime::Seconds(),
		bNaturalOrientationLandscape);
}

bool FOpenMobileSensorsScreenRotationService::HasRotationSource(
	const FGuid& OwnerIdentifier)
{
	FReadScopeLock Lock(OpenMobileSensorScreenRotationPrivate::RotationLock);
	if (const TArray<FOpenMobileSensorScreenRotationSnapshot>* OwnerHistory =
		OpenMobileSensorScreenRotationPrivate::RotationHistories.Find(
			OwnerIdentifier);
		OwnerHistory && !OwnerHistory->IsEmpty())
	{
		return true;
	}
	const TArray<FOpenMobileSensorScreenRotationSnapshot>* GlobalHistory =
		OpenMobileSensorScreenRotationPrivate::RotationHistories.Find(FGuid());
	return GlobalHistory && !GlobalHistory->IsEmpty();
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
	if (!OwnerIdentifier.IsValid())
	{
		PlatformNaturalOrientationLandscape.Reset();
	}
}

void FOpenMobileSensorsScreenRotationService::Reset()
{
	using namespace OpenMobileSensorScreenRotationPrivate;
	FWriteScopeLock Lock(RotationLock);
	RotationHistories.Reset();
	NextRotationSequence = 1;
	PlatformNaturalOrientationLandscape.Reset();
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
	if (!ResolveAndApply(OwnerIdentifier, Sample.Header, Snapshot))
	{
		return;
	}
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
	if (!ResolveAndApply(OwnerIdentifier, Sample.Header, Snapshot))
	{
		return;
	}
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
	if (!ResolveAndApply(OwnerIdentifier, Sample.Header, Snapshot))
	{
		return;
	}
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
