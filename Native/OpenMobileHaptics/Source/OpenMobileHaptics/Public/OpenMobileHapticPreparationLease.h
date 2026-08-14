#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OpenMobileHapticPreparationLease.generated.h"

class UOpenMobileHapticsSubsystem;
class UOpenMobileHapticPreparationAsyncAction;
struct FStreamableHandle;

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
	/** Use this before depending on prepared content, because Game Instance teardown invalidates the claim also. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Is Haptic Preparation Lease Valid", Keywords = "haptic prepared ready owner", ToolTip = "Returns true while this lease owns one prepared-content claim in its Game Instance."))
	bool IsValid() const;

	/** Releases only your claim, the cache stays prepared till its last owner lets go. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "Release Haptic Preparation Lease", Keywords = "haptic prepared unload release", ToolTip = "Releases only this caller's preparation claim. Shared resources remain prepared while another lease or legacy owner still needs them."))
	void Release();

	/** Gives the claim back during garbage collection so an abandoned Blueprint task can't pin prepared data forever. */
	virtual void BeginDestroy() override;

private:
	friend class UOpenMobileHapticsSubsystem;
	friend class UOpenMobileHapticPreparationAsyncAction;

	/** Connects a shared-library claim to the subsystem that must receive the matching release. */
	void InitializeLease(UOpenMobileHapticsSubsystem* InSubsystem);

	/** Holds direct asset preparation alive even when no named-library cache owns those objects. */
	void InitializeAssetLease(
		UObject* PrimaryAsset,
		UObject* PreparedDependency,
		TSharedPtr<FStreamableHandle> InAssetHandle
	);
	/** Invalidates ownership before the subsystem disappears, then Release stays safe if Blueprint calls it later. */
	void HandleGameInstanceTeardown();

	TWeakObjectPtr<UOpenMobileHapticsSubsystem> Subsystem;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> PreparedObjects;

	TSharedPtr<FStreamableHandle> AssetHandle;
	bool bReleased = false;
};
