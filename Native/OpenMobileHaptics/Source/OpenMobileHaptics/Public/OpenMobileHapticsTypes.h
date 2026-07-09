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
enum class EOpenMobileHapticImpactStyle : uint8
{
	Light,
	Medium,
	Heavy,
	Soft,
	Rigid
};

UENUM(BlueprintType)
enum class EOpenMobileHapticNotificationType : uint8
{
	Success,
	Warning,
	Error
};

UENUM(BlueprintType)
enum class EOpenMobileHapticGamePreset : uint8
{
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
enum class EOpenMobileHapticCurveParameter : uint8
{
	IntensityControl,
	SharpnessControl
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
enum class EOpenMobileHapticInterruptionPolicy : uint8
{
	Stop,
	Restart
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
enum class EOpenMobileHapticTimingClock : uint8
{
	None,
	Game,
	Audio
};

UENUM(BlueprintType)
enum class EOpenMobileHapticTimingCalibrationStatus : uint8
{
	Rejected,
	Accepted,
	ClockDiscontinuity
};

UENUM(BlueprintType)
enum class EOpenMobileHapticSynchronizationMode : uint8
{
	None,
	NativeAudioAndHaptics,
	BestEffort
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
enum class EOpenMobileHapticNamedPatternStatus : uint8
{
	Unprepared,
	Loading,
	Loaded,
	Missing,
	Invalid
};

UENUM(BlueprintType)
enum class EOpenMobileHapticPreparationState : uint8
{
	Unprepared,
	Preparing,
	Prepared,
	Failed
};

struct OPENMOBILEHAPTICS_API FOpenMobileHapticsPreparedResourceLimits
{
	int32 MaximumCount = 32;
	int64 MaximumBytes = 4 * 1024 * 1024;
	double IdleLifetimeSeconds = 30.0;
};

UENUM(BlueprintType)
enum class EOpenMobileHapticFallbackFloor : uint8
{
	PortableRich,
	PrimitiveOrPredefined,
	Semantic,
	BasicVibration
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
enum class EOpenMobileHapticControlImplementation : uint8
{
	None,
	Native,
	Emulated,
	Unsupported
};

UENUM(BlueprintType)
enum class EOpenMobileHapticEventEvidence : uint8
{
	Estimated,
	SchedulerConfirmed,
	NativeConfirmed
};

UENUM(BlueprintType)
enum class EOpenMobileHapticErrorCode : uint8
{
	None,
	UnsupportedHardware,
	UnsupportedFeature,
	DisabledByPolicy,
	InvalidPattern,
	RateLimited,
	ChannelBusy,
	LifecycleRestricted,
	NotConfigured,
	NativeEngineFailure,
	Interrupted,
	Cancelled,
	InvalidRequest,
	BackendUnavailable,
	Internal
};

UENUM(BlueprintType)
enum class EOpenMobileHapticFailureStage : uint8
{
	None,
	Validation,
	Policy,
	Capability,
	Channel,
	RateLimit,
	Lifecycle,
	Preparation,
	Compilation,
	NativeSubmission,
	Playback,
	Interruption,
	Shutdown
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
struct OPENMOBILEHAPTICS_API FOpenMobileHapticDynamicParameterUpdate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	bool bUpdateIntensity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	bool bUpdateSharpness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized sharpness control. 0.5 is neutral, 0 is softer, and 1 is sharper."))
	float Sharpness = 0.5f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticError
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticErrorCode Code = EOpenMobileHapticErrorCode::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileErrorCode CommonCode = EOpenMobileErrorCode::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticFailureStage Stage =
		EOpenMobileHapticFailureStage::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FString NativeDomain;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FString NativeCode;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName FailedItem;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FName> FallbackAttempts;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FString Correction;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bRejectedBeforeSubmission = true;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bInterruptedAfterAcceptance = false;

	bool IsSet() const
	{
		return Code != EOpenMobileHapticErrorCode::None;
	}

	static FOpenMobileHapticError Make(
		EOpenMobileHapticErrorCode HapticCode,
		EOpenMobileErrorCode InCommonCode,
		EOpenMobileHapticFailureStage InStage,
		FString InMessage
	)
	{
		FOpenMobileHapticError Error;
		Error.Code = HapticCode;
		Error.CommonCode = InCommonCode;
		Error.Stage = InStage;
		Error.Message = MoveTemp(InMessage);
		return Error;
	}

	static FOpenMobileHapticError FromCommon(
		EOpenMobileErrorCode InCommonCode,
		FString InMessage,
		EOpenMobileHapticFailureStage InStage =
			EOpenMobileHapticFailureStage::None
	)
	{
		EOpenMobileHapticErrorCode HapticCode =
			EOpenMobileHapticErrorCode::Internal;
		switch (InCommonCode)
		{
		case EOpenMobileErrorCode::None:
			HapticCode = EOpenMobileHapticErrorCode::None;
			break;
		case EOpenMobileErrorCode::NotSupported:
			HapticCode = EOpenMobileHapticErrorCode::UnsupportedFeature;
			break;
		case EOpenMobileErrorCode::NotConfigured:
			HapticCode = EOpenMobileHapticErrorCode::NotConfigured;
			break;
		case EOpenMobileErrorCode::Unavailable:
			HapticCode = EOpenMobileHapticErrorCode::BackendUnavailable;
			break;
		case EOpenMobileErrorCode::Busy:
			HapticCode = EOpenMobileHapticErrorCode::ChannelBusy;
			break;
		case EOpenMobileErrorCode::Cancelled:
			HapticCode = EOpenMobileHapticErrorCode::Cancelled;
			break;
		case EOpenMobileErrorCode::InvalidArgument:
			HapticCode = EOpenMobileHapticErrorCode::InvalidRequest;
			break;
		case EOpenMobileErrorCode::NativeFailure:
			HapticCode = EOpenMobileHapticErrorCode::NativeEngineFailure;
			break;
		case EOpenMobileErrorCode::Internal:
			HapticCode = EOpenMobileHapticErrorCode::Internal;
			break;
		}
		return Make(HapticCode, InCommonCode, InStage, MoveTemp(InMessage));
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticNamedSupport
{
	GENERATED_BODY()

	FOpenMobileHapticNamedSupport() = default;
	FOpenMobileHapticNamedSupport(
		FName InName,
		EOpenMobileHapticSupportState InSupport
	)
		: Name(InName)
		, Support(InSupport)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Name;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Support =
		EOpenMobileHapticSupportState::Unknown;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticIntegerLimit
{
	GENERATED_BODY()

	FOpenMobileHapticIntegerLimit() = default;
	FOpenMobileHapticIntegerLimit(bool bInKnown, int32 InValue)
		: bKnown(bInKnown)
		, Value(InValue)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 Value = 0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticDurationLimit
{
	GENERATED_BODY()

	FOpenMobileHapticDurationLimit() = default;
	FOpenMobileHapticDurationLimit(bool bInKnown, double InSeconds)
		: bKnown(bInKnown)
		, Seconds(InSeconds)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double Seconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticFrequencyRange
{
	GENERATED_BODY()

	FOpenMobileHapticFrequencyRange() = default;
	FOpenMobileHapticFrequencyRange(
		bool bInKnown,
		float InMinimumHertz,
		float InMaximumHertz
	)
		: bKnown(bInKnown)
		, MinimumHertz(InMinimumHertz)
		, MaximumHertz(InMaximumHertz)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics", meta = (Units = "Hz"))
	float MinimumHertz = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics", meta = (Units = "Hz"))
	float MaximumHertz = 0.0f;
};

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
	EOpenMobileHapticSupportState AmplitudeControl =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState SemanticEffects =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState PredefinedEffects =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState WaveformTiming =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Looping =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Primitives =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Envelopes =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState FrequencyControl =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState TransientEvents =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState ContinuousEvents =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState DynamicParameters =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState AudioEvents =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState AHAP =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Scheduling =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Pause =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Resume =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Seek =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticNamedSupport> PrimitiveSupport;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticNamedSupport> PresetSupport;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticIntegerLimit MaximumEventCount;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticIntegerLimit MaximumControlPointCount;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticDurationLimit MaximumDurationSeconds;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticIntegerLimit MaximumQueueDepth;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticDurationLimit MinimumTimingGranularitySeconds;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticDurationLimit MaximumControlPointDurationSeconds;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticFrequencyRange FrequencyRange;

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
struct OPENMOBILEHAPTICS_API FOpenMobileHapticCurvePoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", Units = "s"))
	double RelativeTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Value = 1.0f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticParameterCurve
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticCurveParameter Parameter =
		EOpenMobileHapticCurveParameter::IntensityControl;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", Units = "s"))
	double StartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ToolTip = "Pattern-wide normalized control points. Intensity 1.0 and sharpness 0.5 are neutral."))
	TArray<FOpenMobileHapticCurvePoint> ControlPoints;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPattern
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticPatternEvent> Events;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticParameterCurve> ParameterCurves;
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
struct OPENMOBILEHAPTICS_API FOpenMobileHapticTimingAnchor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticTimingClock Clock = EOpenMobileHapticTimingClock::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double ClockTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double PlatformMonotonicTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double EstimatedPrecisionSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 CalibrationRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 LifecycleGeneration = 0;

	bool IsValid() const
	{
		return Clock != EOpenMobileHapticTimingClock::None
			&& CalibrationRevision > 0
			&& LifecycleGeneration > 0;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticTimingCalibrationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticTimingCalibrationStatus Status =
		EOpenMobileHapticTimingCalibrationStatus::Rejected;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticTimingAnchor Anchor;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FString Error;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticSynchronizationDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSynchronizationMode Mode =
		EOpenMobileHapticSynchronizationMode::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticTimingClock Clock = EOpenMobileHapticTimingClock::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double RequestedTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double ResolvedPlatformTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double EstimatedPrecisionSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double LatenessSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 CalibrationRevision = 0;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticInterruptionPolicy InterruptionPolicy =
		EOpenMobileHapticInterruptionPolicy::Stop;

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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FSoftObjectPath PatternAsset;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FSoftObjectPath PlatformOverrideAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackOptions Options;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticLibraryPreloadHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FGuid Id;

	bool IsValid() const { return Id.IsValid(); }

	friend bool operator==(
		const FOpenMobileHapticLibraryPreloadHandle& Left,
		const FOpenMobileHapticLibraryPreloadHandle& Right
	)
	{
		return Left.Id == Right.Id;
	}
};

UENUM(BlueprintType)
enum class EOpenMobileHapticLibraryPreloadOutcome : uint8
{
	Prepared,
	Cancelled,
	Failed
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticLibraryPreloadResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticLibraryPreloadHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticLibraryPreloadOutcome Outcome =
		EOpenMobileHapticLibraryPreloadOutcome::Failed;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 PreparedPatternCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FString> Errors;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticDurationDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double RequestedSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double ResolvedSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bNativeDurationKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double NativeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bNativeClamped = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticIntensityDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	float Requested = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	float Resolved = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bNativeIntensityKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	float Native = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bNativeClamped = false;
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
	FOpenMobileHapticError Error;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName ResolvedPath;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FName> FallbackAttempts;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticDurationDiagnostics Duration;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticIntensityDiagnostics Intensity;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticSynchronizationDiagnostics Synchronization;

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
		Result.Error = FOpenMobileHapticError::FromCommon(
			ErrorCode,
			MoveTemp(Message)
		);
		return Result;
	}

	static FOpenMobileHapticPlaybackResult MakeRejected(
		FOpenMobileHapticError Error
	)
	{
		FOpenMobileHapticPlaybackResult Result;
		Result.State = EOpenMobileHapticPlaybackState::Failed;
		Result.Error = MoveTemp(Error);
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
	EOpenMobileHapticControlImplementation Implementation =
		EOpenMobileHapticControlImplementation::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double RequestedPositionSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double ResolvedPositionSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double PositionGranularitySeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 CompletedRepeatCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 ControlRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bQuantized = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticError Error;

	static FOpenMobileHapticControlResult MakeRejected(
		EOpenMobileErrorCode ErrorCode,
		FString Message
	)
	{
		FOpenMobileHapticControlResult Result;
		Result.Error = FOpenMobileHapticError::FromCommon(
			ErrorCode,
			MoveTemp(Message)
		);
		return Result;
	}

	static FOpenMobileHapticControlResult MakeRejected(
		FOpenMobileHapticError Error
	)
	{
		FOpenMobileHapticControlResult Result;
		Result.Error = MoveTemp(Error);
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
	FOpenMobileHapticPlaybackHandle RecoverySourceHandle;

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
	FOpenMobileHapticError Error;
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
	FOpenMobileHapticError LastError;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticDurationDiagnostics LastDuration;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticIntensityDiagnostics LastIntensity;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName LastNamedPattern;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticNamedPatternStatus LastNamedPatternStatus =
		EOpenMobileHapticNamedPatternStatus::Unprepared;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 PreparedNamedPatternCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticPreparationState PreparationState =
		EOpenMobileHapticPreparationState::Unprepared;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName LastResolvedPath;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FName> LastFallbackAttempts;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticPlaybackEvent> RecentPlaybackEvents;
};
