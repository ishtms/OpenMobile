#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "OpenMobileHapticsPreviewReceiverSubsystem.generated.h"

UENUM(BlueprintType, meta = (ToolTip = "Outcome of a Development-only Haptic preview receiver or pairing action."))
enum class EOpenMobileHapticsPreviewActionOutcome : uint8
{
	Succeeded UMETA(DisplayName = "Succeeded", ToolTip = "The receiver or pairing action completed successfully."),
	Rejected UMETA(DisplayName = "Rejected", ToolTip = "The action was invalid, expired, busy, or could not bind its receiver port."),
	Unavailable UMETA(DisplayName = "Unavailable In This Build", ToolTip = "Haptic device preview is available only in Development builds.")
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICSPREVIEW_API FOpenMobileHapticsPreviewPairingRequest
{
	GENERATED_BODY()

	UPROPERTY(
		BlueprintReadOnly,
		Category = "OpenMobile|Haptics|Preview",
		meta = (ToolTip = "True while an editor is waiting for approval.")
	)
	bool bValid = false;

	UPROPERTY()
	FGuid RequestId;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "OpenMobile|Haptics|Preview",
		meta = (ToolTip = "Sanitized label supplied by the requesting editor.")
	)
	FString EditorLabel;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "OpenMobile|Haptics|Preview",
		meta = (ToolTip = "Six-digit code shown in the editor for local confirmation.")
	)
	FString PairingCode;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "OpenMobile|Haptics|Preview",
		meta = (
			Units = "s",
			ToolTip = "Seconds remaining before this pairing request expires."
		)
	)
	double RemainingSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICSPREVIEW_API FOpenMobileHapticsPreviewReceiverStatus
{
	GENERATED_BODY()

	UPROPERTY(
		BlueprintReadOnly,
		Category = "OpenMobile|Haptics|Preview",
		meta = (ToolTip = "True when the receiver exists in this Development build.")
	)
	bool bAvailableInThisBuild = false;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "OpenMobile|Haptics|Preview",
		meta = (ToolTip = "True while the receiver is listening for editors.")
	)
	bool bEnabled = false;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "OpenMobile|Haptics|Preview",
		meta = (ToolTip = "True after a local pairing request was approved.")
	)
	bool bPaired = false;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "OpenMobile|Haptics|Preview",
		meta = (ToolTip = "UDP port used by the enabled receiver.")
	)
	int32 Port = 0;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "OpenMobile|Haptics|Preview",
		meta = (ToolTip = "Sanitized device label announced to editors.")
	)
	FString ReceiverLabel;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "OpenMobile|Haptics|Preview",
		meta = (ToolTip = "Label of the currently paired editor, when any.")
	)
	FString PairedEditorLabel;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "OpenMobile|Haptics|Preview",
		meta = (ToolTip = "Latest receiver error, or empty when no error is active.")
	)
	FString LastError;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
	FOpenMobileHapticsPreviewReceiverStatusChanged
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileHapticsPreviewReceiverChanged,
	FOpenMobileHapticsPreviewReceiverStatus,
	Status,
	FOpenMobileHapticsPreviewPairingRequest,
	PendingPairing
);

struct FOpenMobileHapticsPreviewReceiverState;

struct FOpenMobileHapticsPreviewReceiverStateDeleter
{
	/** Keeps socket-heavy receiver state out of the public header while deleting it in the module that knows the full type. */
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
	/** Owns an out-of-line destructor because receiver state stays intentionally incomplete in this header. */
	virtual ~UOpenMobileHapticsPreviewReceiverSubsystem() override;
	/** Allocates Development-only transport state and binds background cleanup before any editor can discover the receiver. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** Closes the socket, clears pairing, and removes engine delegates before the Game Instance releases us. */
	virtual void Deinitialize() override;
	/** Prevents the receiver from existing in builds where live device preview was compiled out. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Starts listening with typed outcome branches, invalid ports and labels come back as Rejected instead of a vague false. */
	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Haptics|Preview",
		meta = (
			DisplayName = "Enable Haptic Preview Receiver (Development Only)",
			ToolTip = "Starts the Development-only receiver and reports whether it succeeded, was rejected, or is unavailable in this build.",
			Keywords = "haptics device live preview pair development",
			ExpandEnumAsExecs = "Outcome",
			CPP_Default_Port = "41798",
			CPP_Default_ReceiverLabel = "Test Host"
		)
	)
	void EnableHapticPreviewReceiver(
		UPARAM(meta = (ClampMin = "1024", ClampMax = "65535"))
		int32 Port,
		UPARAM(DisplayName = "Receiver Label") FString ReceiverLabel,
		EOpenMobileHapticsPreviewActionOutcome& Outcome,
		FString& Error
	);

	/** Ends the session and queued preview work together so an editor can't keep control after the receiver is off. */
	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Haptics|Preview",
		meta = (
			DisplayName = "Disable Haptic Preview Receiver (Development Only)",
			ToolTip = "Stops the Development-only receiver, clears pairing, and stops preview playback.",
			Keywords = "haptics device live preview disconnect development",
			ExpandEnumAsExecs = "Outcome"
		)
	)
	void DisableHapticPreviewReceiver(
		EOpenMobileHapticsPreviewActionOutcome& Outcome,
		FString& Error
	);

	/** Accepts the typed prompt only while its hidden request ID is still current, an old UI button can't approve a newer editor. */
	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Haptics|Preview",
		meta = (
			DisplayName = "Approve Haptic Preview Pairing (Development Only)",
			ToolTip = "Approves the current typed pairing request after its code is confirmed locally.",
			Keywords = "haptics device live preview pair allow development",
			ExpandEnumAsExecs = "Outcome"
		)
	)
	void ApproveHapticPreviewPairing(
		FOpenMobileHapticsPreviewPairingRequest PairingRequest,
		EOpenMobileHapticsPreviewActionOutcome& Outcome,
		FString& Error
	);

	/** Rejects the exact pending prompt and tells you when it already expired or changed. */
	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Haptics|Preview",
		meta = (
			DisplayName = "Reject Haptic Preview Pairing (Development Only)",
			ToolTip = "Rejects the current typed pairing request.",
			Keywords = "haptics device live preview pair deny development",
			ExpandEnumAsExecs = "Outcome"
		)
	)
	void RejectHapticPreviewPairing(
		FOpenMobileHapticsPreviewPairingRequest PairingRequest,
		EOpenMobileHapticsPreviewActionOutcome& Outcome,
		FString& Error
	);

	/** Keeps old bool-based graphs working, new graphs should use the typed node to separate rejection from build availability. */
	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Haptics|Preview|Advanced",
		meta = (
			DeprecatedFunction,
			DeprecationMessage = "Use Enable Haptic Preview Receiver for typed outcome branches.",
			DisplayName = "Enable Receiver (Legacy, Development Only)",
			ToolTip = "Legacy bool API for enabling the Development-only preview receiver."
		)
	)
	bool EnableReceiver(
		int32 Port = 41798,
		FString ReceiverLabel = TEXT("Test Host")
	);

	/** Keeps old graphs loadable while sharing the same socket and pairing cleanup as the typed node. */
	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Haptics|Preview|Advanced",
		meta = (
			DeprecatedFunction,
			DeprecationMessage = "Use Disable Haptic Preview Receiver.",
			DisplayName = "Disable Receiver (Legacy, Development Only)",
			ToolTip = "Legacy API for disabling the Development-only preview receiver."
		)
	)
	void DisableReceiver();

	/** Returns sanitized receiver state for UI, it won't expose socket addresses or session tokens. */
	UFUNCTION(
		BlueprintPure,
		Category = "OpenMobile|Haptics|Preview",
		meta = (
			DisplayName = "Get Haptic Preview Receiver Status (Development Only)",
			ToolTip = "Returns a snapshot of the Development-only receiver state."
		)
	)
	FOpenMobileHapticsPreviewReceiverStatus GetReceiverStatus() const;

	/** Recomputes the prompt lifetime at read time so UI doesn't display the stale value captured on arrival. */
	UFUNCTION(
		BlueprintPure,
		Category = "OpenMobile|Haptics|Preview",
		meta = (
			DisplayName = "Get Pending Haptic Preview Pairing (Development Only)",
			ToolTip = "Returns the current typed pairing request and its remaining lifetime."
		)
	)
	FOpenMobileHapticsPreviewPairingRequest GetPendingPairingRequest() const;

	/** Keeps legacy native and Blueprint callers working, raw IDs get exact-current-request validation only. */
	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Haptics|Preview|Advanced",
		meta = (
			DeprecatedFunction,
			DeprecationMessage = "Use Approve Haptic Preview Pairing with the typed request.",
			DisplayName = "Approve Pairing by ID (Legacy, Development Only)",
			ToolTip = "Legacy raw-ID API for approving a Development-only pairing request."
		)
	)
	bool ApprovePairing(FGuid RequestId);

	/** Rejects by raw ID for old callers without weakening the current pending-request check. */
	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Haptics|Preview|Advanced",
		meta = (
			DeprecatedFunction,
			DeprecationMessage = "Use Reject Haptic Preview Pairing with the typed request.",
			DisplayName = "Reject Pairing by ID (Legacy, Development Only)",
			ToolTip = "Legacy raw-ID API for rejecting a Development-only pairing request."
		)
	)
	bool RejectPairing(FGuid RequestId);

	UPROPERTY(
		BlueprintAssignable,
		Category = "OpenMobile|Haptics|Preview",
		meta = (
			DisplayName = "On Haptic Preview Receiver Changed (Development Only)",
			ToolTip = "Fires with current receiver status and the pending pairing request whenever either changes."
		)
	)
	FOpenMobileHapticsPreviewReceiverChanged OnReceiverChanged;

	UPROPERTY(
		BlueprintAssignable,
		Category = "OpenMobile|Haptics|Preview|Advanced",
		meta = (
			DisplayName = "On Receiver Status Changed (Legacy, Development Only)",
			ToolTip = "Legacy payload-free notification. Use On Haptic Preview Receiver Changed for current values."
		)
	)
	FOpenMobileHapticsPreviewReceiverStatusChanged OnStatusChanged;

	/** Drains non-blocking UDP work and advances pairing or session expiry on the Game Instance tick. */
	virtual void Tick(float DeltaTime) override;
	/** Registers receiver work with Unreal's tick stats so Development profiling can account for it. */
	virtual TStatId GetStatId() const override;
	/** Ticks only while a real receiver socket exists, templates and disabled builds stay quiet. */
	virtual bool IsTickable() const override;
	/** Associates ticking with this Game Instance's world so teardown and pause rules stay in the right context. */
	virtual UWorld* GetTickableGameObjectWorld() const override;

private:
	/** Sends both current typed payloads and the old payload-free event from one state transition. */
	void BroadcastReceiverChanged();
	/** Stops preview output as soon as the app backgrounds, remote control shouldn't continue while local UI is hidden. */
	void HandleApplicationBackground();
	/** Cancels the active preview handle and optionally discards queued revisions when ownership ends. */
	void StopPreviewPlayback(bool bClearQueue);

	TUniquePtr<
		FOpenMobileHapticsPreviewReceiverState,
		FOpenMobileHapticsPreviewReceiverStateDeleter
	> State;
};
