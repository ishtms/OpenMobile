#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticPatternAsset.h"

enum class EOpenMobileHapticsPreviewMessageType : uint8
{
	Discover = 1,
	Announce,
	PairRequest,
	PairPending,
	PairAccepted,
	PairRejected,
	Preview,
	Result,
	Stop,
	Heartbeat,
	Goodbye
};

enum class EOpenMobileHapticsPreviewResultCode : uint8
{
	Accepted,
	RejectedProtocol,
	PairingRequired,
	Busy,
	StaleRevision,
	RateLimited,
	QueueFull,
	CapabilityMismatch,
	InvalidPattern,
	PlaybackRejected,
	SessionExpired,
	Disabled
};

enum class EOpenMobileHapticsPreviewPairDecision : uint8
{
	Started,
	ExistingPending,
	ExistingSession,
	Busy,
	Invalid
};

struct OPENMOBILEHAPTICSPREVIEW_API FOpenMobileHapticsPreviewCapabilities
{
	EOpenMobileHapticAvailability Availability =
		EOpenMobileHapticAvailability::UnsupportedPlatform;
	EOpenMobileHapticSupportState BasicVibration =
		EOpenMobileHapticSupportState::Unknown;
	EOpenMobileHapticSupportState RichHaptics =
		EOpenMobileHapticSupportState::Unknown;
	EOpenMobileHapticSupportState Primitives =
		EOpenMobileHapticSupportState::Unknown;
	EOpenMobileHapticSupportState WaveformTiming =
		EOpenMobileHapticSupportState::Unknown;
	EOpenMobileHapticSupportState FrequencyControl =
		EOpenMobileHapticSupportState::Unknown;
	EOpenMobileHapticSupportState DynamicParameters =
		EOpenMobileHapticSupportState::Unknown;
	EOpenMobileHapticSupportState AHAP =
		EOpenMobileHapticSupportState::Unknown;
	int32 MaximumEventCount = 0;
	int32 MaximumControlPointCount = 0;
	double MaximumDurationSeconds = 0.0;
	uint32 Signature = 0;

	static FOpenMobileHapticsPreviewCapabilities FromRuntime(
		const FOpenMobileHapticCapabilities& Capabilities
	);
};

struct OPENMOBILEHAPTICSPREVIEW_API FOpenMobileHapticsPreviewPattern
{
	FOpenMobileHapticCookedPatternData CookedPattern;
	FOpenMobileHapticLoopOptions Loop;
	FString Category = TEXT("Preview");
	EOpenMobileHapticFallbackPolicy FallbackPolicy =
		EOpenMobileHapticFallbackPolicy::Automatic;
	EOpenMobileHapticFallbackFloor LowestAllowedFallback =
		EOpenMobileHapticFallbackFloor::BasicVibration;
	FString PrimitiveOrPresetFallback;
	bool bAllowSemanticFallback = false;
	EOpenMobileHapticSemanticEffect SemanticFallback =
		EOpenMobileHapticSemanticEffect::Click;
	uint32 CapabilitySignature = 0;

	static FOpenMobileHapticsPreviewPattern FromAsset(
		const UOpenMobileHapticPatternAsset& Asset,
		uint32 InCapabilitySignature
	);
	bool Validate(FString& OutError) const;
};

struct OPENMOBILEHAPTICSPREVIEW_API FOpenMobileHapticsPreviewMessage
{
	EOpenMobileHapticsPreviewMessageType Type =
		EOpenMobileHapticsPreviewMessageType::Discover;
	FGuid SenderId;
	FGuid RequestId;
	FGuid SessionId;
	uint64 Revision = 0;
	FString Label;
	FString PairingCode;
	FOpenMobileHapticsPreviewCapabilities Capabilities;
	FOpenMobileHapticsPreviewPattern Pattern;
	EOpenMobileHapticsPreviewResultCode ResultCode =
		EOpenMobileHapticsPreviewResultCode::RejectedProtocol;
	FString ResolvedPath;
	double ResolvedStartTimeSeconds = 0.0;
	double EstimatedPrecisionSeconds = 0.0;
	FString Error;
};

class OPENMOBILEHAPTICSPREVIEW_API FOpenMobileHapticsPreviewProtocol final
{
public:
	static constexpr uint16 Version = 1;
	static constexpr int32 DefaultPort = 41798;
	static constexpr int32 MaximumPacketBytes = 48 * 1024;
	static constexpr int32 MaximumEvents = 256;
	static constexpr int32 MaximumCurves = 32;
	static constexpr int32 MaximumControlPoints = 512;
	static constexpr double MaximumPatternDurationSeconds = 10.0;
	static constexpr int32 MaximumPreviewQueueDepth = 4;
	static constexpr int32 MaximumRequestsPerSecond = 4;
	static constexpr double PairingLifetimeSeconds = 30.0;
	static constexpr double SessionIdleTimeoutSeconds = 30.0;
	static constexpr double SessionLifetimeSeconds = 300.0;

	static bool Encode(
		const FOpenMobileHapticsPreviewMessage& Message,
		TArray<uint8>& OutPacket,
		FString& OutError
	);
	static bool Decode(
		TConstArrayView<uint8> Packet,
		FOpenMobileHapticsPreviewMessage& OutMessage,
		FString& OutError
	);
	static FString SanitizeText(const FString& Value, int32 MaximumLength = 96);
};

class OPENMOBILEHAPTICSPREVIEW_API
FOpenMobileHapticsPreviewSessionPolicy final
{
public:
	EOpenMobileHapticsPreviewPairDecision BeginPairing(
		const FGuid& EditorId,
		const FGuid& RequestId
	);
	bool ApprovePairing(const FGuid& RequestId, double NowSeconds);
	bool RejectPairing(const FGuid& RequestId);
	bool IsPairingPending() const;
	bool IsPaired() const;
	bool MatchesSession(
		const FGuid& EditorId,
		const FGuid& SessionId
	) const;
	bool IsSessionExpired(double NowSeconds) const;
	void TouchSession(double NowSeconds);
	EOpenMobileHapticsPreviewResultCode AdmitPreview(
		uint64 Revision,
		double NowSeconds,
		int32 QueueDepth
	);
	void Disconnect();
	void Reset();

	const FGuid& GetPendingRequestId() const;
	const FGuid& GetPairedEditorId() const;
	const FGuid& GetPairedRequestId() const;
	const FGuid& GetSessionId() const;

private:
	FGuid PendingEditorId;
	FGuid PendingRequestId;
	FGuid PairedEditorId;
	FGuid PairedRequestId;
	FGuid SessionId;
	double SessionExpirationSeconds = 0.0;
	double LastSessionActivitySeconds = 0.0;
	uint64 LastRevision = 0;
	TArray<double> RecentRequestTimes;
};
