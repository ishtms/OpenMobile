#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticsCapabilityTester.generated.h"

class UOpenMobileHapticsSubsystem;

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICSPREVIEW_API
FOpenMobileHapticsCapabilityTesterSupport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	EOpenMobileHapticSupportState State =
		EOpenMobileHapticSupportState::Unknown;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICSPREVIEW_API
FOpenMobileHapticsCapabilityTesterLimit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	bool bKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	double Value = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	FString Unit;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICSPREVIEW_API
FOpenMobileHapticsCapabilityTesterSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	int32 SchemaVersion = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	FString Backend;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	FString NativeApiTier;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	EOpenMobileHapticAvailability Availability =
		EOpenMobileHapticAvailability::UnsupportedPlatform;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	FString EngineState;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	EOpenMobileHapticPreparationState PreparationState =
		EOpenMobileHapticPreparationState::Unprepared;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	bool bPlayerEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	float MasterIntensity = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	int32 ActivePlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	int32 QueuedPlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	int32 PreparedNamedPatternCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	int32 CachedTimelineCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	int64 CachedTimelineBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	TArray<FOpenMobileHapticsCapabilityTesterSupport> FeatureSupport;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	TArray<FOpenMobileHapticsCapabilityTesterSupport> PrimitiveSupport;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	TArray<FOpenMobileHapticsCapabilityTesterSupport> PresetSupport;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	TArray<FOpenMobileHapticsCapabilityTesterLimit> Limits;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics Tester")
	bool bTruncated = false;
};

class OPENMOBILEHAPTICSPREVIEW_API
FOpenMobileHapticsCapabilityTesterSnapshotBuilder final
{
public:
	static FOpenMobileHapticsCapabilityTesterSnapshot Capture(
		const UOpenMobileHapticsSubsystem& Haptics
	);
	static FOpenMobileHapticsCapabilityTesterSnapshot Build(
		const FOpenMobileHapticCapabilities& Capabilities,
		const FOpenMobileHapticUserPolicy& Policy,
		const FOpenMobileHapticsDiagnostics& Diagnostics
	);
	static bool Serialize(
		const FOpenMobileHapticsCapabilityTesterSnapshot& Snapshot,
		FString& OutJson,
		FString& OutError
	);
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
	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Haptics Tester",
		meta = (WorldContext = "WorldContextObject")
	)
	static bool CreateSanitizedCapabilitySnapshot(
		const UObject* WorldContextObject,
		FOpenMobileHapticsCapabilityTesterSnapshot& OutSnapshot,
		FString& OutJson,
		FString& OutError
	);
};
