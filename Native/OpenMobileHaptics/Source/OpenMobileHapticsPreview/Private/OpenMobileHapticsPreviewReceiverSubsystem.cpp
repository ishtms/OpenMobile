#include "OpenMobileHapticsPreviewReceiverSubsystem.h"

#include "Common/UdpSocketBuilder.h"
#include "Engine/GameInstance.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileHapticPatternAsset.h"
#include "OpenMobileHapticsPreviewProtocol.h"
#include "OpenMobileHapticsSubsystem.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "UObject/StrongObjectPtr.h"

namespace OpenMobileHapticsPreviewReceiverPrivate
{
	constexpr int32 MaximumDatagramsPerTick = 16;

	struct FPendingPreview
	{
		FOpenMobileHapticsPreviewMessage Message;
		FIPv4Endpoint Endpoint;
	};

	FString MakePairingCode()
	{
		return FString::Printf(
			TEXT("%06u"),
			FGuid::NewGuid().A % 1000000
		);
	}
}

struct FOpenMobileHapticsPreviewReceiverState
{
	FSocket* Socket = nullptr;
	FGuid ReceiverId = FGuid::NewGuid();
	int32 Port = 0;
	FString ReceiverLabel;
	FString LastError;

	FOpenMobileHapticsPreviewPairingRequest PendingPairing;
	FGuid PendingEditorId;
	FIPv4Endpoint PendingEndpoint;
	double PendingExpirationSeconds = 0.0;

	FOpenMobileHapticsPreviewSessionPolicy SessionPolicy;
	FIPv4Endpoint PairedEndpoint;
	FString PairedEditorLabel;
	TArray<OpenMobileHapticsPreviewReceiverPrivate::FPendingPreview>
		PreviewQueue;
	FOpenMobileHapticPlaybackHandle ActiveHandle;
	TStrongObjectPtr<UOpenMobileHapticPatternAsset> ActivePattern;
};

void FOpenMobileHapticsPreviewReceiverStateDeleter::operator()(
	FOpenMobileHapticsPreviewReceiverState* State
) const
{
	delete State;
}

namespace OpenMobileHapticsPreviewReceiverPrivate
{
	FOpenMobileHapticsPreviewCapabilities ResolveCapabilities(
		const UOpenMobileHapticsPreviewReceiverSubsystem& Receiver
	)
	{
		const UGameInstance* GameInstance = Receiver.GetGameInstance();
		const UOpenMobileHapticsSubsystem* Haptics = GameInstance
			? GameInstance->GetSubsystem<UOpenMobileHapticsSubsystem>()
			: nullptr;
		return FOpenMobileHapticsPreviewCapabilities::FromRuntime(
			Haptics ? Haptics->GetHapticCapabilities()
				: FOpenMobileHapticCapabilities{}
		);
	}

	bool SendMessage(
		FOpenMobileHapticsPreviewReceiverState& State,
		const FIPv4Endpoint& Endpoint,
		FOpenMobileHapticsPreviewMessage Message
	)
	{
		if (!State.Socket)
		{
			return false;
		}
		Message.SenderId = State.ReceiverId;
		TArray<uint8> Packet;
		FString Error;
		if (!FOpenMobileHapticsPreviewProtocol::Encode(
			Message,
			Packet,
			Error
		))
		{
			State.LastError = Error;
			return false;
		}
		int32 SentBytes = 0;
		return State.Socket->SendTo(
			Packet.GetData(),
			Packet.Num(),
			SentBytes,
			*Endpoint.ToInternetAddr()
		) && SentBytes == Packet.Num();
	}

	void SendRejection(
		FOpenMobileHapticsPreviewReceiverState& State,
		const FIPv4Endpoint& Endpoint,
		const FGuid& RequestId,
		EOpenMobileHapticsPreviewResultCode Code,
		const FString& Error
	)
	{
		FOpenMobileHapticsPreviewMessage Response;
		Response.Type = EOpenMobileHapticsPreviewMessageType::PairRejected;
		Response.RequestId = RequestId;
		Response.ResultCode = Code;
		Response.Error = FOpenMobileHapticsPreviewProtocol::SanitizeText(Error);
		SendMessage(State, Endpoint, MoveTemp(Response));
	}

	void SendResult(
		FOpenMobileHapticsPreviewReceiverState& State,
		const FIPv4Endpoint& Endpoint,
		const FGuid& SessionId,
		uint64 Revision,
		EOpenMobileHapticsPreviewResultCode Code,
		const FOpenMobileHapticsPreviewCapabilities& Capabilities,
		FName ResolvedPath,
		double ResolvedStartTimeSeconds,
		double EstimatedPrecisionSeconds,
		const FString& Error
	)
	{
		FOpenMobileHapticsPreviewMessage Response;
		Response.Type = EOpenMobileHapticsPreviewMessageType::Result;
		Response.SessionId = SessionId;
		Response.Revision = Revision;
		Response.ResultCode = Code;
		Response.Capabilities = Capabilities;
		Response.ResolvedPath = FOpenMobileHapticsPreviewProtocol::SanitizeText(
			ResolvedPath.ToString(),
			32
		);
		Response.ResolvedStartTimeSeconds = ResolvedStartTimeSeconds;
		Response.EstimatedPrecisionSeconds = EstimatedPrecisionSeconds;
		Response.Error = FOpenMobileHapticsPreviewProtocol::SanitizeText(Error);
		SendMessage(State, Endpoint, MoveTemp(Response));
	}
}

UOpenMobileHapticsPreviewReceiverSubsystem::
~UOpenMobileHapticsPreviewReceiverSubsystem() = default;

void UOpenMobileHapticsPreviewReceiverSubsystem::Initialize(
	FSubsystemCollectionBase& Collection
)
{
	Super::Initialize(Collection);
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	Collection.InitializeDependency<UOpenMobileHapticsSubsystem>();
	State.Reset(new FOpenMobileHapticsPreviewReceiverState());
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(
		this,
		&UOpenMobileHapticsPreviewReceiverSubsystem::HandleApplicationBackground
	);
#endif
}

void UOpenMobileHapticsPreviewReceiverSubsystem::Deinitialize()
{
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
	DisableReceiver();
	OnReceiverChanged.Clear();
	OnStatusChanged.Clear();
	State.Reset();
#endif
	Super::Deinitialize();
}

bool UOpenMobileHapticsPreviewReceiverSubsystem::ShouldCreateSubsystem(
	UObject* Outer
) const
{
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	return Super::ShouldCreateSubsystem(Outer);
#else
	static_cast<void>(Outer);
	return false;
#endif
}

void UOpenMobileHapticsPreviewReceiverSubsystem::
EnableHapticPreviewReceiver(
	int32 Port,
	FString ReceiverLabel,
	EOpenMobileHapticsPreviewActionOutcome& Outcome,
	FString& Error
)
{
	Outcome = EOpenMobileHapticsPreviewActionOutcome::Unavailable;
	Error = TEXT("Haptic preview is available only in Development builds.");
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	if (EnableReceiver(Port, MoveTemp(ReceiverLabel)))
	{
		Outcome = EOpenMobileHapticsPreviewActionOutcome::Succeeded;
		Error.Reset();
		return;
	}
	Outcome = EOpenMobileHapticsPreviewActionOutcome::Rejected;
	Error = GetReceiverStatus().LastError;
	if (Error.IsEmpty())
	{
		Error = TEXT("Haptic preview receiver could not be enabled.");
	}
#else
	static_cast<void>(Port);
	static_cast<void>(ReceiverLabel);
#endif
}

void UOpenMobileHapticsPreviewReceiverSubsystem::
DisableHapticPreviewReceiver(
	EOpenMobileHapticsPreviewActionOutcome& Outcome,
	FString& Error
)
{
	Outcome = EOpenMobileHapticsPreviewActionOutcome::Unavailable;
	Error = TEXT("Haptic preview is available only in Development builds.");
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	DisableReceiver();
	Outcome = EOpenMobileHapticsPreviewActionOutcome::Succeeded;
	Error.Reset();
#endif
}

void UOpenMobileHapticsPreviewReceiverSubsystem::
ApproveHapticPreviewPairing(
	FOpenMobileHapticsPreviewPairingRequest PairingRequest,
	EOpenMobileHapticsPreviewActionOutcome& Outcome,
	FString& Error
)
{
	Outcome = EOpenMobileHapticsPreviewActionOutcome::Unavailable;
	Error = TEXT("Haptic preview is available only in Development builds.");
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	if (PairingRequest.bValid && ApprovePairing(PairingRequest.RequestId))
	{
		Outcome = EOpenMobileHapticsPreviewActionOutcome::Succeeded;
		Error.Reset();
		return;
	}
	Outcome = EOpenMobileHapticsPreviewActionOutcome::Rejected;
	Error = TEXT("The pairing request is invalid, expired, or no longer pending.");
#else
	static_cast<void>(PairingRequest);
#endif
}

void UOpenMobileHapticsPreviewReceiverSubsystem::
RejectHapticPreviewPairing(
	FOpenMobileHapticsPreviewPairingRequest PairingRequest,
	EOpenMobileHapticsPreviewActionOutcome& Outcome,
	FString& Error
)
{
	Outcome = EOpenMobileHapticsPreviewActionOutcome::Unavailable;
	Error = TEXT("Haptic preview is available only in Development builds.");
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	if (PairingRequest.bValid && RejectPairing(PairingRequest.RequestId))
	{
		Outcome = EOpenMobileHapticsPreviewActionOutcome::Succeeded;
		Error.Reset();
		return;
	}
	Outcome = EOpenMobileHapticsPreviewActionOutcome::Rejected;
	Error = TEXT("The pairing request is invalid or no longer pending.");
#else
	static_cast<void>(PairingRequest);
#endif
}

bool UOpenMobileHapticsPreviewReceiverSubsystem::EnableReceiver(
	int32 Port,
	FString ReceiverLabel
)
{
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	if (!State)
	{
		State.Reset(new FOpenMobileHapticsPreviewReceiverState());
	}
	if (State->Socket)
	{
		return true;
	}
	ReceiverLabel = FOpenMobileHapticsPreviewProtocol::SanitizeText(
		ReceiverLabel,
		32
	);
	if (Port < 1024 || Port > MAX_uint16 || ReceiverLabel.IsEmpty())
	{
		State->LastError = TEXT("Receiver port or label is invalid.");
		BroadcastReceiverChanged();
		return false;
	}
	State->Socket = FUdpSocketBuilder(TEXT("OpenMobileHapticsPreviewReceiver"))
		.AsNonBlocking()
		.AsReusable()
		.WithBroadcast()
		.BoundToAddress(FIPv4Address::Any)
		.BoundToPort(Port)
		.WithReceiveBufferSize(
			FOpenMobileHapticsPreviewProtocol::MaximumPacketBytes * 4
		);
	if (!State->Socket)
	{
		State->LastError = TEXT("Preview receiver could not bind its UDP port.");
		BroadcastReceiverChanged();
		return false;
	}
	State->Port = Port;
	State->ReceiverLabel = MoveTemp(ReceiverLabel);
	State->LastError.Reset();
	BroadcastReceiverChanged();
	return true;
#else
	static_cast<void>(Port);
	static_cast<void>(ReceiverLabel);
	return false;
#endif
}

void UOpenMobileHapticsPreviewReceiverSubsystem::DisableReceiver()
{
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	if (!State)
	{
		return;
	}
	StopPreviewPlayback(true);
	if (State->Socket)
	{
		if (State->SessionPolicy.IsPaired())
		{
			FOpenMobileHapticsPreviewMessage Goodbye;
			Goodbye.Type = EOpenMobileHapticsPreviewMessageType::Goodbye;
			Goodbye.SessionId = State->SessionPolicy.GetSessionId();
			OpenMobileHapticsPreviewReceiverPrivate::SendMessage(
				*State,
				State->PairedEndpoint,
				MoveTemp(Goodbye)
			);
		}
		State->Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(
			State->Socket
		);
		State->Socket = nullptr;
	}
	State->Port = 0;
	State->PendingPairing = {};
	State->SessionPolicy.Reset();
	State->PairedEditorLabel.Reset();
	State->PreviewQueue.Reset();
	BroadcastReceiverChanged();
#endif
}

FOpenMobileHapticsPreviewReceiverStatus
UOpenMobileHapticsPreviewReceiverSubsystem::GetReceiverStatus() const
{
	FOpenMobileHapticsPreviewReceiverStatus Result;
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	Result.bAvailableInThisBuild = true;
	if (State)
	{
		Result.bEnabled = State->Socket != nullptr;
		Result.bPaired = State->SessionPolicy.IsPaired();
		Result.Port = State->Port;
		Result.ReceiverLabel = State->ReceiverLabel;
		Result.PairedEditorLabel = State->PairedEditorLabel;
		Result.LastError = State->LastError;
	}
#endif
	return Result;
}

FOpenMobileHapticsPreviewPairingRequest
UOpenMobileHapticsPreviewReceiverSubsystem::GetPendingPairingRequest() const
{
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	if (State && State->PendingPairing.bValid)
	{
		FOpenMobileHapticsPreviewPairingRequest Result = State->PendingPairing;
		Result.RemainingSeconds = FMath::Max(
			0.0,
			State->PendingExpirationSeconds - FPlatformTime::Seconds()
		);
		return Result;
	}
#endif
	return {};
}

bool UOpenMobileHapticsPreviewReceiverSubsystem::ApprovePairing(
	FGuid RequestId
)
{
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	if (!State || !State->Socket || State->SessionPolicy.IsPaired()
		|| !State->PendingPairing.bValid
		|| State->PendingPairing.RequestId != RequestId
		|| FPlatformTime::Seconds() >= State->PendingExpirationSeconds)
	{
		return false;
	}
	const double Now = FPlatformTime::Seconds();
	if (!State->SessionPolicy.ApprovePairing(RequestId, Now))
	{
		return false;
	}
	State->PairedEndpoint = State->PendingEndpoint;
	State->PairedEditorLabel = State->PendingPairing.EditorLabel;
	State->PreviewQueue.Reset();
	FOpenMobileHapticsPreviewMessage Accepted;
	Accepted.Type = EOpenMobileHapticsPreviewMessageType::PairAccepted;
	Accepted.RequestId = RequestId;
	Accepted.SessionId = State->SessionPolicy.GetSessionId();
	OpenMobileHapticsPreviewReceiverPrivate::SendMessage(
		*State,
		State->PairedEndpoint,
		MoveTemp(Accepted)
	);
	State->PendingPairing = {};
	State->LastError.Reset();
	BroadcastReceiverChanged();
	return true;
#else
	static_cast<void>(RequestId);
	return false;
#endif
}

bool UOpenMobileHapticsPreviewReceiverSubsystem::RejectPairing(
	FGuid RequestId
)
{
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	if (!State || !State->PendingPairing.bValid
		|| State->PendingPairing.RequestId != RequestId)
	{
		return false;
	}
	OpenMobileHapticsPreviewReceiverPrivate::SendRejection(
		*State,
		State->PendingEndpoint,
		RequestId,
		EOpenMobileHapticsPreviewResultCode::PairingRequired,
		TEXT("Pairing was rejected on the receiver.")
	);
	State->SessionPolicy.RejectPairing(RequestId);
	State->PendingPairing = {};
	BroadcastReceiverChanged();
	return true;
#else
	static_cast<void>(RequestId);
	return false;
#endif
}

void UOpenMobileHapticsPreviewReceiverSubsystem::Tick(float DeltaTime)
{
	static_cast<void>(DeltaTime);
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	using namespace OpenMobileHapticsPreviewReceiverPrivate;
	if (!State || !State->Socket)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (State->PendingPairing.bValid
		&& Now >= State->PendingExpirationSeconds)
	{
		SendRejection(
			*State,
			State->PendingEndpoint,
			State->PendingPairing.RequestId,
			EOpenMobileHapticsPreviewResultCode::SessionExpired,
			TEXT("Pairing request expired.")
		);
		State->SessionPolicy.RejectPairing(
			State->PendingPairing.RequestId
		);
		State->PendingPairing = {};
		BroadcastReceiverChanged();
	}
	if (State->SessionPolicy.IsSessionExpired(Now))
	{
		StopPreviewPlayback(true);
		FOpenMobileHapticsPreviewMessage Goodbye;
		Goodbye.Type = EOpenMobileHapticsPreviewMessageType::Goodbye;
		Goodbye.SessionId = State->SessionPolicy.GetSessionId();
		SendMessage(
			*State,
			State->PairedEndpoint,
			MoveTemp(Goodbye)
		);
		State->SessionPolicy.Disconnect();
		State->PairedEditorLabel.Reset();
		State->PreviewQueue.Reset();
		State->LastError = TEXT("Preview session expired.");
		BroadcastReceiverChanged();
	}

	for (int32 DatagramIndex = 0;
		DatagramIndex < MaximumDatagramsPerTick;
		++DatagramIndex)
	{
		uint32 PendingBytes = 0;
		if (!State->Socket->HasPendingData(PendingBytes))
		{
			break;
		}
		if (PendingBytes == 0
			|| PendingBytes
				> FOpenMobileHapticsPreviewProtocol::MaximumPacketBytes)
		{
			TArray<uint8> Discard;
			Discard.SetNumUninitialized(FMath::Min<uint32>(PendingBytes, 65536));
			int32 DiscardedBytes = 0;
			TSharedRef<FInternetAddr> DiscardedSender =
				ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
				->CreateInternetAddr();
			State->Socket->RecvFrom(
				Discard.GetData(),
				Discard.Num(),
				DiscardedBytes,
				*DiscardedSender
			);
			State->LastError = TEXT("Oversized preview packet rejected.");
			continue;
		}
		TArray<uint8> Packet;
		Packet.SetNumUninitialized(PendingBytes);
		int32 ReadBytes = 0;
		TSharedRef<FInternetAddr> Sender =
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
			->CreateInternetAddr();
		if (!State->Socket->RecvFrom(
			Packet.GetData(),
			Packet.Num(),
			ReadBytes,
			*Sender
		) || ReadBytes != Packet.Num())
		{
			continue;
		}
		const FIPv4Endpoint Endpoint(Sender);
		FOpenMobileHapticsPreviewMessage Message;
		FString DecodeError;
		if (!FOpenMobileHapticsPreviewProtocol::Decode(
			Packet,
			Message,
			DecodeError
		))
		{
			State->LastError =
				FOpenMobileHapticsPreviewProtocol::SanitizeText(DecodeError);
			continue;
		}

		if (Message.Type == EOpenMobileHapticsPreviewMessageType::Discover)
		{
			FOpenMobileHapticsPreviewMessage Announce;
			Announce.Type = EOpenMobileHapticsPreviewMessageType::Announce;
			Announce.Label = State->ReceiverLabel;
			Announce.Capabilities = ResolveCapabilities(*this);
			SendMessage(*State, Endpoint, MoveTemp(Announce));
			continue;
		}
		if (Message.Type == EOpenMobileHapticsPreviewMessageType::PairRequest)
		{
			const EOpenMobileHapticsPreviewPairDecision PairDecision =
				State->SessionPolicy.BeginPairing(
					Message.SenderId,
					Message.RequestId
				);
			if (PairDecision
					== EOpenMobileHapticsPreviewPairDecision::ExistingSession
				&& Endpoint == State->PairedEndpoint)
			{
				FOpenMobileHapticsPreviewMessage Accepted;
				Accepted.Type =
					EOpenMobileHapticsPreviewMessageType::PairAccepted;
				Accepted.RequestId =
					State->SessionPolicy.GetPairedRequestId();
				Accepted.SessionId = State->SessionPolicy.GetSessionId();
				SendMessage(*State, Endpoint, MoveTemp(Accepted));
				continue;
			}
			if (PairDecision
				== EOpenMobileHapticsPreviewPairDecision::Invalid)
			{
				SendRejection(
					*State,
					Endpoint,
					Message.RequestId,
					EOpenMobileHapticsPreviewResultCode::RejectedProtocol,
					TEXT("Pairing request is invalid.")
				);
				continue;
			}
			const bool bEndpointChanged = PairDecision
					== EOpenMobileHapticsPreviewPairDecision::ExistingPending
				&& State->PendingEndpoint != Endpoint;
			if (PairDecision == EOpenMobileHapticsPreviewPairDecision::Busy
				|| PairDecision
					== EOpenMobileHapticsPreviewPairDecision::ExistingSession
				|| bEndpointChanged)
			{
				SendRejection(
					*State,
					Endpoint,
					Message.RequestId,
					EOpenMobileHapticsPreviewResultCode::Busy,
					TEXT("Receiver is already pairing or paired.")
				);
				continue;
			}
			if (PairDecision
				== EOpenMobileHapticsPreviewPairDecision::Started)
			{
				State->PendingPairing.bValid = true;
				State->PendingPairing.RequestId = Message.RequestId;
				State->PendingPairing.EditorLabel =
					FOpenMobileHapticsPreviewProtocol::SanitizeText(
						Message.Label,
						32
					);
				State->PendingPairing.PairingCode = MakePairingCode();
				State->PendingEditorId = Message.SenderId;
				State->PendingEndpoint = Endpoint;
				State->PendingExpirationSeconds = Now
					+ FOpenMobileHapticsPreviewProtocol::PairingLifetimeSeconds;
				BroadcastReceiverChanged();
			}
			FOpenMobileHapticsPreviewMessage Pending;
			Pending.Type = EOpenMobileHapticsPreviewMessageType::PairPending;
			Pending.RequestId = Message.RequestId;
			Pending.PairingCode = State->PendingPairing.PairingCode;
			SendMessage(*State, Endpoint, MoveTemp(Pending));
			continue;
		}

		const bool bSessionMatches = State->SessionPolicy.MatchesSession(
				Message.SenderId,
				Message.SessionId
			)
			&& Endpoint == State->PairedEndpoint;
		if (!bSessionMatches)
		{
			if (Message.Type == EOpenMobileHapticsPreviewMessageType::Preview)
			{
				SendResult(
					*State,
					Endpoint,
					Message.SessionId,
					Message.Revision,
					EOpenMobileHapticsPreviewResultCode::PairingRequired,
					ResolveCapabilities(*this),
					NAME_None,
					0.0,
					0.0,
					TEXT("A current approved pairing is required.")
				);
			}
			continue;
		}
		State->SessionPolicy.TouchSession(Now);
		if (Message.Type == EOpenMobileHapticsPreviewMessageType::Heartbeat)
		{
			FOpenMobileHapticsPreviewMessage Heartbeat;
			Heartbeat.Type = EOpenMobileHapticsPreviewMessageType::Heartbeat;
			Heartbeat.SessionId = State->SessionPolicy.GetSessionId();
			SendMessage(*State, Endpoint, MoveTemp(Heartbeat));
			continue;
		}
		if (Message.Type == EOpenMobileHapticsPreviewMessageType::Stop)
		{
			StopPreviewPlayback(true);
			continue;
		}
		if (Message.Type == EOpenMobileHapticsPreviewMessageType::Goodbye)
		{
			StopPreviewPlayback(true);
			State->SessionPolicy.Disconnect();
			State->PairedEditorLabel.Reset();
			State->PreviewQueue.Reset();
			BroadcastReceiverChanged();
			continue;
		}
		if (Message.Type != EOpenMobileHapticsPreviewMessageType::Preview)
		{
			continue;
		}

		const FOpenMobileHapticsPreviewCapabilities Capabilities =
			ResolveCapabilities(*this);
		FString PatternError;
		if (Message.Pattern.CapabilitySignature != Capabilities.Signature)
		{
			SendResult(
				*State,
				Endpoint,
				State->SessionPolicy.GetSessionId(),
				Message.Revision,
				EOpenMobileHapticsPreviewResultCode::CapabilityMismatch,
				Capabilities,
				NAME_None,
				0.0,
				0.0,
				TEXT("Receiver capabilities changed. Discover the device again.")
			);
			continue;
		}
		if (!Message.Pattern.Validate(PatternError))
		{
			SendResult(
				*State,
				Endpoint,
				State->SessionPolicy.GetSessionId(),
				Message.Revision,
				EOpenMobileHapticsPreviewResultCode::InvalidPattern,
				Capabilities,
				NAME_None,
				0.0,
				0.0,
				PatternError
			);
			continue;
		}
		const EOpenMobileHapticsPreviewResultCode Admission =
			State->SessionPolicy.AdmitPreview(
				Message.Revision,
				Now,
				State->PreviewQueue.Num()
			);
		if (Admission != EOpenMobileHapticsPreviewResultCode::Accepted)
		{
			FString AdmissionError;
			switch (Admission)
			{
			case EOpenMobileHapticsPreviewResultCode::StaleRevision:
				AdmissionError = TEXT("Preview revision is stale.");
				break;
			case EOpenMobileHapticsPreviewResultCode::RateLimited:
				AdmissionError = TEXT("Preview request rate exceeded.");
				break;
			case EOpenMobileHapticsPreviewResultCode::QueueFull:
				AdmissionError = TEXT("Preview queue is full.");
				break;
			default:
				AdmissionError = TEXT("Preview session expired.");
				break;
			}
			SendResult(
				*State,
				Endpoint,
				State->SessionPolicy.GetSessionId(),
				Message.Revision,
				Admission,
				Capabilities,
				NAME_None,
				0.0,
				0.0,
				AdmissionError
			);
			continue;
		}
		FPendingPreview& Pending = State->PreviewQueue.AddDefaulted_GetRef();
		Pending.Message = MoveTemp(Message);
		Pending.Endpoint = Endpoint;
	}

	if (!State->PreviewQueue.IsEmpty())
	{
		FPendingPreview Pending = MoveTemp(State->PreviewQueue[0]);
		State->PreviewQueue.RemoveAt(0);
		const FOpenMobileHapticsPreviewCapabilities Capabilities =
			ResolveCapabilities(*this);
		StopPreviewPlayback(false);
		UOpenMobileHapticPatternAsset* PreviewAsset =
			NewObject<UOpenMobileHapticPatternAsset>(GetTransientPackage());
		const FOpenMobileHapticsPreviewPattern& Pattern =
			Pending.Message.Pattern;
		if (!PreviewAsset || !PreviewAsset->InitializeCookedPreviewData(
			Pattern.CookedPattern,
			Pattern.Loop,
			FName(*Pattern.Category),
			Pattern.FallbackPolicy,
			Pattern.LowestAllowedFallback,
			FName(*Pattern.PrimitiveOrPresetFallback),
			Pattern.bAllowSemanticFallback,
			Pattern.SemanticFallback
		))
		{
			SendResult(
				*State,
				Pending.Endpoint,
				State->SessionPolicy.GetSessionId(),
				Pending.Message.Revision,
				EOpenMobileHapticsPreviewResultCode::InvalidPattern,
				Capabilities,
				NAME_None,
				0.0,
				0.0,
				TEXT("Preview pattern could not be initialized.")
			);
			return;
		}
		State->ActivePattern.Reset(PreviewAsset);
		UOpenMobileHapticsSubsystem* Haptics = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UOpenMobileHapticsSubsystem>()
			: nullptr;
		FOpenMobileHapticPlaybackResult Playback;
		if (Haptics)
		{
			FOpenMobileHapticPlaybackOptions Options;
			Options.Channel = TEXT("OpenMobilePreview");
			Options.Category = FName(*Pattern.Category);
			Options.OverlapPolicy = EOpenMobileHapticOverlapPolicy::Replace;
			Options.FallbackPolicy = Pattern.FallbackPolicy;
			Options.Loop = Pattern.Loop;
			Playback = Haptics->SubmitCookedPreview(PreviewAsset, Options);
		}
		if (Playback.IsAccepted())
		{
			State->ActiveHandle = Playback.Handle;
		}
		else
		{
			State->ActivePattern.Reset();
		}
		SendResult(
			*State,
			Pending.Endpoint,
			State->SessionPolicy.GetSessionId(),
			Pending.Message.Revision,
			Playback.IsAccepted()
				? EOpenMobileHapticsPreviewResultCode::Accepted
				: EOpenMobileHapticsPreviewResultCode::PlaybackRejected,
			Capabilities,
			Playback.ResolvedPath,
			Playback.Synchronization.ResolvedPlatformTimeSeconds,
			Playback.Synchronization.EstimatedPrecisionSeconds,
			Playback.Error.Message
		);
	}
#endif
}

TStatId UOpenMobileHapticsPreviewReceiverSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(
		UOpenMobileHapticsPreviewReceiverSubsystem,
		STATGROUP_Tickables
	);
}

bool UOpenMobileHapticsPreviewReceiverSubsystem::IsTickable() const
{
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	return State && State->Socket && !IsTemplate();
#else
	return false;
#endif
}

UWorld* UOpenMobileHapticsPreviewReceiverSubsystem::GetTickableGameObjectWorld()
	const
{
	return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

void UOpenMobileHapticsPreviewReceiverSubsystem::BroadcastReceiverChanged()
{
	OnReceiverChanged.Broadcast(
		GetReceiverStatus(),
		GetPendingPairingRequest()
	);
	OnStatusChanged.Broadcast();
}

void UOpenMobileHapticsPreviewReceiverSubsystem::HandleApplicationBackground()
{
	StopPreviewPlayback(true);
}

void UOpenMobileHapticsPreviewReceiverSubsystem::StopPreviewPlayback(
	bool bClearQueue
)
{
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	if (!State)
	{
		return;
	}
	if (State->ActiveHandle.IsValid())
	{
		if (UOpenMobileHapticsSubsystem* Haptics = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UOpenMobileHapticsSubsystem>()
			: nullptr)
		{
			Haptics->StopPlayback(State->ActiveHandle);
		}
	}
	State->ActiveHandle = {};
	State->ActivePattern.Reset();
	if (bClearQueue)
	{
		State->PreviewQueue.Reset();
	}
#endif
}
