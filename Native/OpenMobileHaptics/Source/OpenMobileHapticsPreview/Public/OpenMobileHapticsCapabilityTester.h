#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticsCapabilityTester.generated.h"

class UOpenMobileHapticsSubsystem;

UENUM(BlueprintType, meta = (ToolTip = "Outcome of capturing a sanitized Development-only Haptic capability snapshot."))
enum class EOpenMobileHapticsCapabilitySnapshotOutcome : uint8
{
	Succeeded UMETA(DisplayName = "Succeeded", ToolTip = "The sanitized snapshot and deterministic JSON were created."),
	Failed UMETA(DisplayName = "Failed", ToolTip = "The Game Instance, Haptics subsystem, or JSON serialization failed."),
	Unavailable UMETA(DisplayName = "Unavailable In This Build", ToolTip = "Capability snapshots are available only in Development builds.")
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICSPREVIEW_API
FOpenMobileHapticsCapabilityTesterSupport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Allowlisted capability or limit name."))
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Reported support state for this capability."))
	EOpenMobileHapticSupportState State =
		EOpenMobileHapticSupportState::Unknown;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICSPREVIEW_API
FOpenMobileHapticsCapabilityTesterLimit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Allowlisted limit name."))
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "True when the device reported a numeric value."))
	bool bKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Sanitized numeric value when Known is true."))
	double Value = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Unit used by the numeric value."))
	FString Unit;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICSPREVIEW_API
FOpenMobileHapticsCapabilityTesterSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Snapshot schema version for exported JSON readers."))
	int32 SchemaVersion = 1;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Active provider-neutral runtime backend name."))
	FString Backend;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Highest native haptics API tier available on this device."))
	FString NativeApiTier;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Current runtime haptics availability."))
	EOpenMobileHapticAvailability Availability =
		EOpenMobileHapticAvailability::UnsupportedPlatform;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Sanitized runtime engine state."))
	FString EngineState;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Current named-library preparation state."))
	EOpenMobileHapticPreparationState PreparationState =
		EOpenMobileHapticPreparationState::Unprepared;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "True when the user policy allows haptic playback."))
	bool bPlayerEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Sanitized global intensity scale from 0 through 1."))
	float MasterIntensity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Number of haptic playbacks currently active."))
	int32 ActivePlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Number of haptic playbacks currently queued."))
	int32 QueuedPlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Number of named patterns currently prepared."))
	int32 PreparedNamedPatternCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Number of resolved timelines in the runtime cache."))
	int32 CachedTimelineCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (Units = "B", ToolTip = "Approximate bytes used by cached resolved timelines."))
	int64 CachedTimelineBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Allowlisted high-level feature support."))
	TArray<FOpenMobileHapticsCapabilityTesterSupport> FeatureSupport;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Allowlisted Android primitive support."))
	TArray<FOpenMobileHapticsCapabilityTesterSupport> PrimitiveSupport;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Allowlisted native preset support."))
	TArray<FOpenMobileHapticsCapabilityTesterSupport> PresetSupport;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "Sanitized device and runtime limits."))
	TArray<FOpenMobileHapticsCapabilityTesterLimit> Limits;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Preview", meta = (ToolTip = "True when non-allowlisted or unsafe diagnostic data was omitted."))
	bool bTruncated = false;
};

class OPENMOBILEHAPTICSPREVIEW_API
FOpenMobileHapticsCapabilityTesterSnapshotBuilder final
{
public:
	/** Captures the related runtime values together so a changing subsystem can't leave you with a mixed snapshot. */
	static FOpenMobileHapticsCapabilityTesterSnapshot Capture(
		const UOpenMobileHapticsSubsystem& Haptics
	);
	/** Reduces raw runtime diagnostics to the fixed allowlist used by device reports, unknown values remain honest also. */
	static FOpenMobileHapticsCapabilityTesterSnapshot Build(
		const FOpenMobileHapticCapabilities& Capabilities,
		const FOpenMobileHapticUserPolicy& Policy,
		const FOpenMobileHapticsDiagnostics& Diagnostics
	);
	/** Produces stable JSON for saving or comparing device reports without exposing names outside the allowlist. */
	static bool Serialize(
		const FOpenMobileHapticsCapabilityTesterSnapshot& Snapshot,
		FString& OutJson,
		FString& OutError
	);
	/** Formats the same sanitized values for an in-game screen, so it won't suddenly reveal extra runtime details. */
	static FString ToDisplayText(
		const FOpenMobileHapticsCapabilityTesterSnapshot& Snapshot
	);
};

UCLASS()
class OPENMOBILEHAPTICSPREVIEW_API
UOpenMobileHapticsCapabilityTesterLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Use this in a Development build when you need a shareable device report, Shipping returns Unavailable on purpose. */
	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Haptics|Preview",
		meta = (
			WorldContext = "WorldContextObject",
			DisplayName = "Create Haptic Capability Snapshot (Development Only)",
			ToolTip = "Captures a sanitized capability snapshot and deterministic JSON in Development builds.",
			Keywords = "haptics device capability diagnostics json development",
			ExpandEnumAsExecs = "Outcome"
		)
	)
	static void CreateHapticCapabilitySnapshot(
		const UObject* WorldContextObject,
		EOpenMobileHapticsCapabilitySnapshotOutcome& Outcome,
		UPARAM(DisplayName = "Snapshot")
		FOpenMobileHapticsCapabilityTesterSnapshot& OutSnapshot,
		UPARAM(DisplayName = "JSON") FString& OutJson,
		UPARAM(DisplayName = "Error") FString& OutError
	);

	/** Keeps old bool-based graphs loading, new graphs should take the typed branches so Unavailable isn't mixed with failure. */
	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Haptics|Preview|Advanced",
		meta = (
			WorldContext = "WorldContextObject",
			DeprecatedFunction,
			DeprecationMessage = "Use Create Haptic Capability Snapshot for typed outcome branches.",
			DisplayName = "Create Sanitized Capability Snapshot (Legacy, Development Only)",
			ToolTip = "Legacy bool API for capturing a sanitized Development-only capability snapshot."
		)
	)
	static bool CreateSanitizedCapabilitySnapshot(
		const UObject* WorldContextObject,
		FOpenMobileHapticsCapabilityTesterSnapshot& OutSnapshot,
		FString& OutJson,
		FString& OutError
	);
};
