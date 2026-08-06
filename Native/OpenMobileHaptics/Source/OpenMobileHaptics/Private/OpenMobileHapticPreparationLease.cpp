#include "OpenMobileHapticPreparationLease.h"

#include "OpenMobileHapticsSubsystem.h"

bool UOpenMobileHapticPreparationLease::IsValid() const
{
	return !bReleased && Subsystem.IsValid();
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

void UOpenMobileHapticPreparationLease::HandleGameInstanceTeardown()
{
	bReleased = true;
	Subsystem.Reset();
}
