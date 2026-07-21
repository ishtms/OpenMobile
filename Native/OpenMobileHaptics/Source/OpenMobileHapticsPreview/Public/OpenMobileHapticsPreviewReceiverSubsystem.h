#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "OpenMobileHapticsPreviewReceiverSubsystem.generated.h"

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICSPREVIEW_API FOpenMobileHapticsPreviewPairingRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Preview")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Preview")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Preview")
	FString EditorLabel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Preview")
	FString PairingCode;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Preview")
	double RemainingSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICSPREVIEW_API FOpenMobileHapticsPreviewReceiverStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Preview")
	bool bAvailableInThisBuild = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Preview")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Preview")
	bool bPaired = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Preview")
	int32 Port = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Preview")
	FString ReceiverLabel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Preview")
	FString PairedEditorLabel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Preview")
	FString LastError;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
	FOpenMobileHapticsPreviewReceiverStatusChanged
);

struct FOpenMobileHapticsPreviewReceiverState;

struct FOpenMobileHapticsPreviewReceiverStateDeleter
{
	void operator()(FOpenMobileHapticsPreviewReceiverState* State) const;
};

UCLASS()
class OPENMOBILEHAPTICSPREVIEW_API
UOpenMobileHapticsPreviewReceiverSubsystem final
	: public UGameInstanceSubsystem
	, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual ~UOpenMobileHapticsPreviewReceiverSubsystem() override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics Preview")
	bool EnableReceiver(
		int32 Port = 41798,
		FString ReceiverLabel = TEXT("Test Host")
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics Preview")
	void DisableReceiver();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Haptics Preview")
	FOpenMobileHapticsPreviewReceiverStatus GetReceiverStatus() const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Haptics Preview")
	FOpenMobileHapticsPreviewPairingRequest GetPendingPairingRequest() const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics Preview")
	bool ApprovePairing(FGuid RequestId);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics Preview")
	bool RejectPairing(FGuid RequestId);

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Haptics Preview")
	FOpenMobileHapticsPreviewReceiverStatusChanged OnStatusChanged;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;

private:
	void HandleApplicationBackground();
	void StopPreviewPlayback(bool bClearQueue);

	TUniquePtr<
		FOpenMobileHapticsPreviewReceiverState,
		FOpenMobileHapticsPreviewReceiverStateDeleter
	> State;
};
