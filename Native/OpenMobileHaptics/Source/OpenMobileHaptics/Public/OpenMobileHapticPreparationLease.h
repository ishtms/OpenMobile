#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OpenMobileHapticPreparationLease.generated.h"

class UOpenMobileHapticsSubsystem;

UCLASS(
	BlueprintType,
	Transient,
	meta = (DisplayName = "Haptic Preparation Lease")
)
class OPENMOBILEHAPTICS_API UOpenMobileHapticPreparationLease final :
	public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Is Haptic Preparation Lease Valid", Keywords = "haptic prepared ready owner", ToolTip = "Returns true while this lease owns one prepared-content claim in its Game Instance."))
	bool IsValid() const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Release Haptic Preparation Lease", Keywords = "haptic prepared unload release", ToolTip = "Releases only this caller's preparation claim. Shared resources remain prepared while another lease or legacy owner still needs them."))
	void Release();

	virtual void BeginDestroy() override;

private:
	friend class UOpenMobileHapticsSubsystem;

	void InitializeLease(UOpenMobileHapticsSubsystem* InSubsystem);
	void HandleGameInstanceTeardown();

	TWeakObjectPtr<UOpenMobileHapticsSubsystem> Subsystem;
	bool bReleased = false;
};
