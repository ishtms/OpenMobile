#include "OpenMobileHapticPreparationLease.h"

#include "Engine/StreamableManager.h"
#include "OpenMobileHapticsSubsystem.h"

bool UOpenMobileHapticPreparationLease::IsValid() const
{
	if (bReleased)
	{
		return false;
	}
	if (Subsystem.IsValid() || AssetHandle.IsValid())
	{
		return true;
	}
	for (const UObject* Object : PreparedObjects)
	{
		if (::IsValid(Object))
		{
			return true;
		}
	}
	return false;
}

void UOpenMobileHapticPreparationLease::Release()
{
	if (bReleased)
	{
		return;
	}
	bReleased = true;
	if (Subsystem.IsValid())
	{
		Subsystem->ReleasePreparationLease(this);
	}
	Subsystem.Reset();
	if (AssetHandle)
	{
		AssetHandle->ReleaseHandle();
		AssetHandle.Reset();
	}
	PreparedObjects.Reset();
}

void UOpenMobileHapticPreparationLease::BeginDestroy()
{
	Release();
	Super::BeginDestroy();
}

void UOpenMobileHapticPreparationLease::InitializeLease(
	UOpenMobileHapticsSubsystem* InSubsystem
)
{
	Subsystem = InSubsystem;
}

void UOpenMobileHapticPreparationLease::InitializeAssetLease(
	UObject* PrimaryAsset,
	UObject* PreparedDependency,
	TSharedPtr<FStreamableHandle> InAssetHandle
)
{
	if (PrimaryAsset)
	{
		PreparedObjects.Add(PrimaryAsset);
	}
	if (PreparedDependency)
	{
		PreparedObjects.AddUnique(PreparedDependency);
	}
	AssetHandle = MoveTemp(InAssetHandle);
}

void UOpenMobileHapticPreparationLease::HandleGameInstanceTeardown()
{
	bReleased = true;
	Subsystem.Reset();
	if (AssetHandle)
	{
		AssetHandle->ReleaseHandle();
		AssetHandle.Reset();
	}
	PreparedObjects.Reset();
}
