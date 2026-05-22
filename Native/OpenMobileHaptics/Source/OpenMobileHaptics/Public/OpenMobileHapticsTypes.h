#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileHapticsTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileHapticAvailability : uint8
{
	UnsupportedPlatform,
	NoActuator,
	BasicVibration,
	SemanticFeedback,
	RichHaptics,
	DisabledByPolicy,
	TemporarilyUnavailable
};

UENUM(BlueprintType)
enum class EOpenMobileHapticSupportState : uint8
{
	Unknown,
	Supported,
	Unsupported
};

UENUM(BlueprintType)
enum class EOpenMobileHapticSemanticEffect : uint8
{
	Selection,
	ImpactLight,
	ImpactMedium,
	ImpactHeavy,
	ImpactSoft,
	ImpactRigid,
	NotificationSuccess,
	NotificationWarning,
	NotificationError,
	Confirm,
	Reject,
	Tick,
	Click,
	Bump,
	Damage,
	Pickup,
	Achievement
};

UENUM(BlueprintType)
enum class EOpenMobileHapticPatternEventType : uint8
{
	Transient,
	Continuous,
	Silence
};

UENUM(BlueprintType)
enum class EOpenMobileHapticChannelPriority : uint8
{
	Low,
	Normal,
	High,
	Critical
};

UENUM(BlueprintType)
enum class EOpenMobileHapticOverlapPolicy : uint8
{
	Replace,
	Ignore,
	Queue,
	InterruptLowerPriority,
	MixWhenSupported
};

UENUM(BlueprintType)
enum class EOpenMobileHapticFallbackPolicy : uint8
{
	Automatic,
	NoBasicVibration,
	ExactOnly,
	NoEffectAllowed
};

UENUM(BlueprintType)
enum class EOpenMobileHapticScheduleMode : uint8
{
	Immediate,
	Relative,
	AbsoluteGameTime,
	AbsoluteAudioTime
};

UENUM(BlueprintType)
enum class EOpenMobileHapticPlaybackState : uint8
{
	Invalid,
	Accepted,
	Scheduled,
	Started,
	Paused,
	Resumed,
	Stopped,
	Cancelled,
	Completed,
	Interrupted,
	Failed
};

UENUM(BlueprintType)
enum class EOpenMobileHapticPlaybackOutcome : uint8
{
	Rejected,
	Accepted,
	Suppressed,
	Fallback
};

UENUM(BlueprintType)
enum class EOpenMobileHapticControlOutcome : uint8
{
	Rejected,
	Accepted,
	Unsupported,
	StaleHandle
};

UENUM(BlueprintType)
enum class EOpenMobileHapticEventEvidence : uint8
{
	Estimated,
	SchedulerConfirmed,
	NativeConfirmed
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPlaybackHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FGuid Id;

	bool IsValid() const
	{
		return Id.IsValid();
	}

	bool operator==(const FOpenMobileHapticPlaybackHandle& Other) const
	{
		return Id == Other.Id;
	}

	bool operator!=(const FOpenMobileHapticPlaybackHandle& Other) const
	{
		return !(*this == Other);
	}
};

FORCEINLINE uint32 GetTypeHash(const FOpenMobileHapticPlaybackHandle& Handle)
{
	return GetTypeHash(Handle.Id);
}

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticCapabilities
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticAvailability Availability =
		EOpenMobileHapticAvailability::UnsupportedPlatform;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState BasicVibration =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState SemanticFeedback =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState RichHaptics =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FString Detail;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPatternEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticPatternEventType Type =
		EOpenMobileHapticPatternEventType::Transient;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0"))
	double StartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0"))
	double DurationSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Sharpness = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FrequencyIntent = 0.5f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPattern
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticPatternEvent> Events;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticSchedule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticScheduleMode Mode =
		EOpenMobileHapticScheduleMode::Immediate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	double TimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	double LatencyOffsetSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticLoopOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0"))
	int32 RepeatCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0"))
	double RepeatStartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0"))
	double MaximumDurationSeconds = 30.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPlaybackOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FName Channel = TEXT("Gameplay");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FName Category = TEXT("Gameplay");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticOverlapPolicy OverlapPolicy =
		EOpenMobileHapticOverlapPolicy::Replace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticFallbackPolicy FallbackPolicy =
		EOpenMobileHapticFallbackPolicy::Automatic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticSchedule Schedule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticLoopOptions Loop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IntensityScale = 1.0f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticSemanticRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSemanticEffect Effect =
		EOpenMobileHapticSemanticEffect::Selection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackOptions Options;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticOneShotRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0"))
	float DurationSeconds = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackOptions Options;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticNamedPatternRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FName PatternName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackOptions Options;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPlaybackResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticPlaybackOutcome Outcome =
		EOpenMobileHapticPlaybackOutcome::Rejected;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileError Error;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName ResolvedPath;

	bool IsAccepted() const
	{
		return Outcome == EOpenMobileHapticPlaybackOutcome::Accepted
			|| Outcome == EOpenMobileHapticPlaybackOutcome::Fallback;
	}

	static FOpenMobileHapticPlaybackResult MakeRejected(
		EOpenMobileErrorCode ErrorCode,
		FString Message
	)
	{
		FOpenMobileHapticPlaybackResult Result;
		Result.State = EOpenMobileHapticPlaybackState::Failed;
		Result.Error = FOpenMobileError::Make(ErrorCode, MoveTemp(Message));
		return Result;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticControlResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticControlOutcome Outcome =
		EOpenMobileHapticControlOutcome::Rejected;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileError Error;

	static FOpenMobileHapticControlResult MakeRejected(
		EOpenMobileErrorCode ErrorCode,
		FString Message
	)
	{
		FOpenMobileHapticControlResult Result;
		Result.Error = FOpenMobileError::Make(ErrorCode, MoveTemp(Message));
		return Result;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPlaybackEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticEventEvidence Evidence =
		EOpenMobileHapticEventEvidence::Estimated;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName PatternOrEffect;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName ResolvedPath;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileError Error;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticUserPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MasterIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	TMap<FName, float> CategoryScales;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	TMap<FName, float> EffectScales;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticsDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticCapabilities Capabilities;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 ActivePlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 QueuedPlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileError LastError;
};
