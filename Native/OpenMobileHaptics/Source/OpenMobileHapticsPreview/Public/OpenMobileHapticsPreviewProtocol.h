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

	/** Copies only preview-safe capability fields and signs that set, the editor can reject content authored for another device report. */
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

	/** Sends cooked portable data instead of editor source, which keeps the device receiver independent of the asset editor. */
	static FOpenMobileHapticsPreviewPattern FromAsset(
		const UOpenMobileHapticPatternAsset& Asset,
		uint32 InCapabilitySignature
	);
	/** Rejects oversized or unsafe preview content before it reaches a device playback path. */
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

	/** Validates and caps a message before UDP transport, callers never get a partly written packet on failure. */
	static bool Encode(
		const FOpenMobileHapticsPreviewMessage& Message,
		TArray<uint8>& OutPacket,
		FString& OutError
	);
	/** Treats every received byte as untrusted and accepts only this exact protocol version and payload limits. */
	static bool Decode(
		TConstArrayView<uint8> Packet,
		FOpenMobileHapticsPreviewMessage& OutMessage,
		FString& OutError
	);
	/** Strips control characters and caps labels before they enter logs, UI, or a reply packet. */
	static FString SanitizeText(const FString& Value, int32 MaximumLength = 96);
};

class OPENMOBILEHAPTICSPREVIEW_API
FOpenMobileHapticsPreviewSessionPolicy final
{
public:
	/** Allows one pending editor at a time and makes retries from that same request idempotent. */
	EOpenMobileHapticsPreviewPairDecision BeginPairing(
		const FGuid& EditorId,
		const FGuid& RequestId
	);
	/** Turns only the current pending request into a time-limited session, stale approval screens can't pair a new request. */
	bool ApprovePairing(const FGuid& RequestId, double NowSeconds);
	/** Clears only the matching pending request so an old reject action can't dismiss somebody else's prompt. */
	bool RejectPairing(const FGuid& RequestId);
	/** Requires both pending IDs to be valid, half-written pairing state isn't exposed as a real prompt. */
	bool IsPairingPending() const;
	/** Requires the editor, request, and session IDs together before protected preview traffic is accepted. */
	bool IsPaired() const;
	/** Checks both sender and session because either ID by itself can survive an editor reconnect. */
	bool MatchesSession(
		const FGuid& EditorId,
		const FGuid& SessionId
	) const;
	/** Expires sessions on their absolute lifetime or idle timeout, a non-finite clock is rejected also. */
	bool IsSessionExpired(double NowSeconds) const;
	/** Refreshes idle time only for a valid session and finite clock sample. */
	void TouchSession(double NowSeconds);
	/** Enforces increasing revisions, request rate, and queue capacity before preview work is allowed in. */
	EOpenMobileHapticsPreviewResultCode AdmitPreview(
		uint64 Revision,
		double NowSeconds,
		int32 QueueDepth
	);
	/** Drops the active session and its rate history while leaving a separate pending pairing untouched. */
	void Disconnect();
	/** Clears pending and active pairing state when the receiver itself is restarted or disabled. */
	void Reset();

	/** Exposes the pending request by reference for transport replies, don't hold it after this policy changes. */
	const FGuid& GetPendingRequestId() const;
	/** Exposes the paired editor identity without copying, the reference belongs to this policy only. */
	const FGuid& GetPairedEditorId() const;
	/** Keeps the accepted pairing request available for acknowledgements and reconnect checks. */
	const FGuid& GetPairedRequestId() const;
	/** Returns the current session token by reference, it becomes invalid as soon as Disconnect or Reset runs. */
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
