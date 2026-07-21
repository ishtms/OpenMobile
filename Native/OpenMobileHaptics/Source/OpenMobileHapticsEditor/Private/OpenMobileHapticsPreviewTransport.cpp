#include "OpenMobileHapticsPreviewTransport.h"

#include "Common/UdpSocketBuilder.h"
#include "Editor.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Misc/App.h"
#include "OpenMobileHapticPatternAsset.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

#define LOCTEXT_NAMESPACE "OpenMobileHapticsPreviewTransport"

namespace OpenMobileHapticsPreviewTransportPrivate
{
	constexpr int32 MaximumDatagramsPerTick = 16;
	constexpr int32 MaximumDiscoveredDevices = 32;
	constexpr double DeviceLifetimeSeconds = 8.0;
	constexpr double HeartbeatIntervalSeconds = 10.0;
	constexpr double PairingRetryIntervalSeconds = 1.0;
}

FOpenMobileHapticsPreviewTransport::FOpenMobileHapticsPreviewTransport()
{
	SetStatus(LOCTEXT("TransportOff", "Device preview transport is off. Select Find Device to enable it."));
}

FOpenMobileHapticsPreviewTransport::~FOpenMobileHapticsPreviewTransport()
{
	Shutdown();
}

bool FOpenMobileHapticsPreviewTransport::Start()
{
#if !OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	SetStatus(LOCTEXT("DevelopmentOnly", "Device preview is available only in Development builds."), true);
	return false;
#else
	if (Socket)
	{
		return true;
	}
	Socket = FUdpSocketBuilder(TEXT("OpenMobileHapticsPreviewEditor"))
		.AsNonBlocking()
		.AsReusable()
		.WithBroadcast()
		.BoundToAddress(FIPv4Address::Any)
		.BoundToPort(0)
		.WithReceiveBufferSize(
			FOpenMobileHapticsPreviewProtocol::MaximumPacketBytes * 4
		);
	if (!Socket)
	{
		SetStatus(LOCTEXT("SocketFailed", "Preview transport could not open a UDP socket."), true);
		return false;
	}
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(
			this,
			&FOpenMobileHapticsPreviewTransport::Tick
		),
		0.05f
	);
	FEditorDelegates::EndPIE.AddSP(
		this,
		&FOpenMobileHapticsPreviewTransport::HandleEndPIE
	);
	SetStatus(LOCTEXT("Ready", "Device preview transport is ready."));
	return true;
#endif
}

void FOpenMobileHapticsPreviewTransport::Shutdown()
{
	if (!Socket)
	{
		return;
	}
	if (const FOpenMobileHapticsPreviewDiscoveredDevice* Device =
		FindPairedDevice())
	{
		FOpenMobileHapticsPreviewMessage Stop;
		Stop.Type = EOpenMobileHapticsPreviewMessageType::Stop;
		Stop.SessionId = Device->SessionId;
		Send(Device->Endpoint, MoveTemp(Stop));
		FOpenMobileHapticsPreviewMessage Goodbye;
		Goodbye.Type = EOpenMobileHapticsPreviewMessageType::Goodbye;
		Goodbye.SessionId = Device->SessionId;
		Send(Device->Endpoint, MoveTemp(Goodbye));
	}
	FEditorDelegates::EndPIE.RemoveAll(this);
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	Socket->Close();
	ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
	Socket = nullptr;
	Devices.Reset();
	SelectedReceiverId.Invalidate();
}

void FOpenMobileHapticsPreviewTransport::Discover()
{
	if (!Socket && !Start())
	{
		return;
	}
	FOpenMobileHapticsPreviewMessage Message;
	Message.Type = EOpenMobileHapticsPreviewMessageType::Discover;
	const FIPv4Endpoint Broadcast(
		FIPv4Address(255, 255, 255, 255),
		FOpenMobileHapticsPreviewProtocol::DefaultPort
	);
	const FIPv4Endpoint Loopback(
		FIPv4Address(127, 0, 0, 1),
		FOpenMobileHapticsPreviewProtocol::DefaultPort
	);
	Send(Broadcast, Message);
	Send(Loopback, MoveTemp(Message));
	SetStatus(LOCTEXT("Discovering", "Searching for enabled Development receivers."));
}

void FOpenMobileHapticsPreviewTransport::SelectNextDevice()
{
	if (const FOpenMobileHapticsPreviewDiscoveredDevice* PairedDevice =
		FindPairedDevice())
	{
		SelectedReceiverId = PairedDevice->ReceiverId;
		SetStatus(LOCTEXT("PairingLocked", "The paired receiver remains selected until its session ends."));
		return;
	}
	if (Devices.IsEmpty())
	{
		SelectedReceiverId.Invalidate();
		return;
	}
	int32 CurrentIndex = Devices.IndexOfByPredicate(
		[this](const FOpenMobileHapticsPreviewDiscoveredDevice& Device)
		{
			return Device.ReceiverId == SelectedReceiverId;
		}
	);
	CurrentIndex = (CurrentIndex + 1) % Devices.Num();
	SelectedReceiverId = Devices[CurrentIndex].ReceiverId;
	SetStatus(FText::Format(
		LOCTEXT("Selected", "Selected {0}."),
		FText::FromString(Devices[CurrentIndex].Label)
	));
}

bool FOpenMobileHapticsPreviewTransport::RequestPairing()
{
	FOpenMobileHapticsPreviewDiscoveredDevice* Device =
		FindDevice(SelectedReceiverId);
	if (!Device || FindPairedDevice()
		|| Devices.ContainsByPredicate(
			[](const FOpenMobileHapticsPreviewDiscoveredDevice& Candidate)
			{
				return Candidate.bPairingPending;
			}
		))
	{
		return false;
	}
	const double Now = FPlatformTime::Seconds();
	Device->PairingRequestId = FGuid::NewGuid();
	Device->PairingCode.Reset();
	Device->bPairingPending = true;
	Device->PairingExpirationSeconds = Now
		+ FOpenMobileHapticsPreviewProtocol::PairingLifetimeSeconds;
	Device->LastPairingRequestSeconds = 0.0;
	if (!SendPairingRequest(*Device))
	{
		Device->bPairingPending = false;
		return false;
	}
	SetStatus(LOCTEXT("PairingRequested", "Pairing requested. Approve it on the selected test host."));
	return true;
}

bool FOpenMobileHapticsPreviewTransport::SendPreview(
	const UOpenMobileHapticPatternAsset& Asset
)
{
	FOpenMobileHapticsPreviewDiscoveredDevice* Device =
		FindDevice(SelectedReceiverId);
	if (!Device || !Device->bPaired || !Asset.IsDerivedDataCurrent())
	{
		SetStatus(LOCTEXT("PreviewUnavailable", "A valid pattern and approved device pairing are required."), true);
		return false;
	}
	FOpenMobileHapticsPreviewMessage Preview;
	Preview.Type = EOpenMobileHapticsPreviewMessageType::Preview;
	Preview.SessionId = Device->SessionId;
	Preview.Revision = ++NextRevision;
	Preview.Pattern = FOpenMobileHapticsPreviewPattern::FromAsset(
		Asset,
		Device->Capabilities.Signature
	);
	FString PatternError;
	if (!Preview.Pattern.Validate(PatternError))
	{
		SetStatus(FText::FromString(PatternError), true);
		return false;
	}
	if (!Send(Device->Endpoint, MoveTemp(Preview)))
	{
		return false;
	}
	SetStatus(FText::Format(
		LOCTEXT("PreviewSent", "Preview revision {0} sent."),
		FText::AsNumber(NextRevision)
	));
	return true;
}

void FOpenMobileHapticsPreviewTransport::StopPreview()
{
	const FOpenMobileHapticsPreviewDiscoveredDevice* Device =
		GetSelectedDevice();
	if (!Device || !Device->bPaired)
	{
		return;
	}
	FOpenMobileHapticsPreviewMessage Stop;
	Stop.Type = EOpenMobileHapticsPreviewMessageType::Stop;
	Stop.SessionId = Device->SessionId;
	Send(Device->Endpoint, MoveTemp(Stop));
	SetStatus(LOCTEXT("PreviewStopped", "Device preview stopped."));
}

const FOpenMobileHapticsPreviewDiscoveredDevice*
FOpenMobileHapticsPreviewTransport::GetSelectedDevice() const
{
	return Devices.FindByPredicate(
		[this](const FOpenMobileHapticsPreviewDiscoveredDevice& Device)
		{
			return Device.ReceiverId == SelectedReceiverId;
		}
	);
}

FText FOpenMobileHapticsPreviewTransport::GetSelectedDeviceText() const
{
	const FOpenMobileHapticsPreviewDiscoveredDevice* Device =
		GetSelectedDevice();
	if (!Device)
	{
		return LOCTEXT("NoDevice", "Device: none");
	}
	if (Device->bPaired)
	{
		return FText::Format(
			LOCTEXT("PairedDevice", "Device: {0} (paired, capability {1})"),
			FText::FromString(Device->Label),
			FText::FromString(FString::Printf(
				TEXT("%08X"),
				Device->Capabilities.Signature
			))
		);
	}
	if (!Device->PairingCode.IsEmpty())
	{
		return FText::Format(
			LOCTEXT("PendingDevice", "Device: {0} (code {1}, capability {2})"),
			FText::FromString(Device->Label),
			FText::FromString(Device->PairingCode),
			FText::FromString(FString::Printf(
				TEXT("%08X"),
				Device->Capabilities.Signature
			))
		);
	}
	return FText::Format(
		LOCTEXT("AvailableDevice", "Device: {0} (capability {1})"),
		FText::FromString(Device->Label),
		FText::FromString(FString::Printf(
			TEXT("%08X"),
			Device->Capabilities.Signature
		))
	);
}

FText FOpenMobileHapticsPreviewTransport::GetStatusText() const
{
	return StatusText;
}

bool FOpenMobileHapticsPreviewTransport::CanRequestPairing() const
{
	const FOpenMobileHapticsPreviewDiscoveredDevice* Device =
		GetSelectedDevice();
	return Device && !FindPairedDevice()
		&& !Devices.ContainsByPredicate(
			[](const FOpenMobileHapticsPreviewDiscoveredDevice& Candidate)
			{
				return Candidate.bPairingPending;
			}
		);
}

bool FOpenMobileHapticsPreviewTransport::CanSendPreview() const
{
	const FOpenMobileHapticsPreviewDiscoveredDevice* Device =
		GetSelectedDevice();
	return Device && Device->bPaired;
}

FSimpleMulticastDelegate& FOpenMobileHapticsPreviewTransport::OnChanged()
{
	return Changed;
}

bool FOpenMobileHapticsPreviewTransport::Tick(float DeltaTime)
{
	static_cast<void>(DeltaTime);
	using namespace OpenMobileHapticsPreviewTransportPrivate;
	if (!Socket)
	{
		return false;
	}
	for (int32 Index = 0; Index < MaximumDatagramsPerTick; ++Index)
	{
		uint32 PendingBytes = 0;
		if (!Socket->HasPendingData(PendingBytes))
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
			Socket->RecvFrom(
				Discard.GetData(),
				Discard.Num(),
				DiscardedBytes,
				*DiscardedSender
			);
			SetStatus(LOCTEXT("OversizedPacket", "Oversized preview response was rejected."), true);
			continue;
		}
		TArray<uint8> Packet;
		Packet.SetNumUninitialized(PendingBytes);
		int32 ReadBytes = 0;
		TSharedRef<FInternetAddr> Sender =
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)
			->CreateInternetAddr();
		if (!Socket->RecvFrom(
			Packet.GetData(),
			Packet.Num(),
			ReadBytes,
			*Sender
		) || ReadBytes != Packet.Num())
		{
			continue;
		}
		FOpenMobileHapticsPreviewMessage Message;
		FString Error;
		if (!FOpenMobileHapticsPreviewProtocol::Decode(
			Packet,
			Message,
			Error
		))
		{
			SetStatus(FText::FromString(Error), true);
			continue;
		}
		HandleMessage(FIPv4Endpoint(Sender), Message);
	}

	const double Now = FPlatformTime::Seconds();
	FOpenMobileHapticsPreviewDiscoveredDevice* PendingDevice =
		Devices.FindByPredicate(
			[](const FOpenMobileHapticsPreviewDiscoveredDevice& Device)
			{
				return Device.bPairingPending;
			}
		);
	if (PendingDevice && Now >= PendingDevice->PairingExpirationSeconds)
	{
		PendingDevice->bPairingPending = false;
		PendingDevice->PairingCode.Reset();
		PendingDevice->PairingRequestId.Invalidate();
		SetStatus(LOCTEXT("PairingExpired", "Pairing request expired. Try again."), true);
	}
	else if (PendingDevice
		&& Now - PendingDevice->LastPairingRequestSeconds
			>= PairingRetryIntervalSeconds)
	{
		SendPairingRequest(*PendingDevice);
	}
	const int32 Removed = Devices.RemoveAll(
		[Now](const FOpenMobileHapticsPreviewDiscoveredDevice& Device)
		{
			return !Device.bPaired && !Device.bPairingPending
				&& Now - Device.LastSeenSeconds >= DeviceLifetimeSeconds;
		}
	);
	if (Removed > 0 && !FindDevice(SelectedReceiverId))
	{
		SelectedReceiverId = Devices.IsEmpty()
			? FGuid() : Devices[0].ReceiverId;
		Changed.Broadcast();
	}
	FOpenMobileHapticsPreviewDiscoveredDevice* Device = FindPairedDevice();
	if (Device && Device->bPaired
		&& Now - LastHeartbeatSeconds >= HeartbeatIntervalSeconds)
	{
		FOpenMobileHapticsPreviewMessage Heartbeat;
		Heartbeat.Type = EOpenMobileHapticsPreviewMessageType::Heartbeat;
		Heartbeat.SessionId = Device->SessionId;
		Send(Device->Endpoint, MoveTemp(Heartbeat));
		LastHeartbeatSeconds = Now;
	}
	if (Device && Device->bPaired
		&& Now - LastSessionResponseSeconds
			>= FOpenMobileHapticsPreviewProtocol::SessionIdleTimeoutSeconds + 5.0)
	{
		Device->bPaired = false;
		Device->SessionId.Invalidate();
		SetStatus(LOCTEXT("SessionExpired", "Device preview session expired."), true);
	}
	return true;
}

void FOpenMobileHapticsPreviewTransport::HandleEndPIE(bool bIsSimulating)
{
	static_cast<void>(bIsSimulating);
	StopPreview();
}

bool FOpenMobileHapticsPreviewTransport::Send(
	const FIPv4Endpoint& Endpoint,
	FOpenMobileHapticsPreviewMessage Message
)
{
	if (!Socket)
	{
		return false;
	}
	Message.SenderId = EditorId;
	TArray<uint8> Packet;
	FString Error;
	if (!FOpenMobileHapticsPreviewProtocol::Encode(Message, Packet, Error))
	{
		SetStatus(FText::FromString(Error), true);
		return false;
	}
	int32 SentBytes = 0;
	if (!Socket->SendTo(
		Packet.GetData(),
		Packet.Num(),
		SentBytes,
		*Endpoint.ToInternetAddr()
	) || SentBytes != Packet.Num())
	{
		SetStatus(LOCTEXT("SendFailed", "Preview datagram could not be sent."), true);
		return false;
	}
	return true;
}

void FOpenMobileHapticsPreviewTransport::HandleMessage(
	const FIPv4Endpoint& Endpoint,
	const FOpenMobileHapticsPreviewMessage& Message
)
{
	const double Now = FPlatformTime::Seconds();
	if (Message.Type == EOpenMobileHapticsPreviewMessageType::Announce)
	{
		FOpenMobileHapticsPreviewDiscoveredDevice* Device =
			FindDevice(Message.SenderId);
		if (Device && Device->Endpoint != Endpoint
			&& (Device->bPaired || Device->bPairingPending))
		{
			return;
		}
		if (!Device)
		{
			if (Devices.Num()
				>= OpenMobileHapticsPreviewTransportPrivate::
					MaximumDiscoveredDevices)
			{
				SetStatus(
					LOCTEXT(
						"DeviceLimit",
						"Additional preview receivers were ignored because the device list is full."
					),
					true
				);
				return;
			}
			Device = &Devices.AddDefaulted_GetRef();
			Device->ReceiverId = Message.SenderId;
		}
		Device->Label = FOpenMobileHapticsPreviewProtocol::SanitizeText(
			Message.Label,
			32
		);
		Device->Endpoint = Endpoint;
		Device->Capabilities = Message.Capabilities;
		Device->LastSeenSeconds = Now;
		if (!SelectedReceiverId.IsValid())
		{
			SelectedReceiverId = Device->ReceiverId;
		}
		SetStatus(FText::Format(
			LOCTEXT("DeviceFound", "Found receiver {0}."),
			FText::FromString(Device->Label)
		));
		return;
	}
	FOpenMobileHapticsPreviewDiscoveredDevice* Device =
		FindDevice(Message.SenderId);
	if (!Device || Device->Endpoint != Endpoint)
	{
		return;
	}
	Device->LastSeenSeconds = Now;
	if (Message.Type == EOpenMobileHapticsPreviewMessageType::PairPending
		&& Device->bPairingPending
		&& Message.RequestId == Device->PairingRequestId)
	{
		Device->PairingCode = Message.PairingCode;
		SetStatus(FText::Format(
			LOCTEXT("PairingPending", "Approve pairing code {0} on {1}."),
			FText::FromString(Message.PairingCode),
			FText::FromString(Device->Label)
		));
		return;
	}
	if (Message.Type == EOpenMobileHapticsPreviewMessageType::PairAccepted
		&& Device->bPairingPending
		&& Message.RequestId == Device->PairingRequestId
		&& Message.SessionId.IsValid())
	{
		Device->bPairingPending = false;
		Device->PairingExpirationSeconds = 0.0;
		Device->bPaired = true;
		Device->SessionId = Message.SessionId;
		LastHeartbeatSeconds = Now;
		LastSessionResponseSeconds = Now;
		SetStatus(FText::Format(
			LOCTEXT("PairingAccepted", "Paired with {0}."),
			FText::FromString(Device->Label)
		));
		return;
	}
	if (Message.Type == EOpenMobileHapticsPreviewMessageType::PairRejected
		&& Device->bPairingPending
		&& Message.RequestId == Device->PairingRequestId)
	{
		Device->bPairingPending = false;
		Device->PairingExpirationSeconds = 0.0;
		Device->PairingCode.Reset();
		SetStatus(FText::FromString(
			FOpenMobileHapticsPreviewProtocol::SanitizeText(Message.Error)
		), true);
		return;
	}
	if (Message.Type == EOpenMobileHapticsPreviewMessageType::Heartbeat
		&& Device->bPaired && Message.SessionId == Device->SessionId)
	{
		LastSessionResponseSeconds = Now;
		return;
	}
	if (Message.Type == EOpenMobileHapticsPreviewMessageType::Goodbye
		&& Device->bPaired && Message.SessionId == Device->SessionId)
	{
		Device->bPaired = false;
		Device->SessionId.Invalidate();
		SetStatus(LOCTEXT("ReceiverStopped", "The preview receiver stopped."), true);
		return;
	}
	if (Message.Type == EOpenMobileHapticsPreviewMessageType::Result
		&& Device->bPaired && Message.SessionId == Device->SessionId)
	{
		LastSessionResponseSeconds = Now;
		Device->Capabilities = Message.Capabilities;
		if (Message.ResultCode == EOpenMobileHapticsPreviewResultCode::Accepted)
		{
			SetStatus(FText::Format(
				LOCTEXT("PreviewAccepted", "Revision {0} accepted through {1}, precision {2} ms."),
				FText::AsNumber(Message.Revision),
				FText::FromString(Message.ResolvedPath),
				FText::AsNumber(Message.EstimatedPrecisionSeconds * 1000.0)
			));
		}
		else
		{
			SetStatus(FText::FromString(
				FOpenMobileHapticsPreviewProtocol::SanitizeText(Message.Error)
			), true);
		}
	}
}

bool FOpenMobileHapticsPreviewTransport::SendPairingRequest(
	FOpenMobileHapticsPreviewDiscoveredDevice& Device
)
{
	FOpenMobileHapticsPreviewMessage Request;
	Request.Type = EOpenMobileHapticsPreviewMessageType::PairRequest;
	Request.RequestId = Device.PairingRequestId;
	Request.Label = FOpenMobileHapticsPreviewProtocol::SanitizeText(
		FString::Printf(TEXT("%s Editor"), FApp::GetProjectName()),
		32
	);
	if (!Send(Device.Endpoint, MoveTemp(Request)))
	{
		return false;
	}
	Device.LastPairingRequestSeconds = FPlatformTime::Seconds();
	return true;
}

FOpenMobileHapticsPreviewDiscoveredDevice*
FOpenMobileHapticsPreviewTransport::FindDevice(const FGuid& Id)
{
	return Devices.FindByPredicate(
		[Id](const FOpenMobileHapticsPreviewDiscoveredDevice& Device)
		{
			return Device.ReceiverId == Id;
		}
	);
}

FOpenMobileHapticsPreviewDiscoveredDevice*
FOpenMobileHapticsPreviewTransport::FindPairedDevice()
{
	return Devices.FindByPredicate(
		[](const FOpenMobileHapticsPreviewDiscoveredDevice& Device)
		{
			return Device.bPaired;
		}
	);
}

const FOpenMobileHapticsPreviewDiscoveredDevice*
FOpenMobileHapticsPreviewTransport::FindPairedDevice() const
{
	return Devices.FindByPredicate(
		[](const FOpenMobileHapticsPreviewDiscoveredDevice& Device)
		{
			return Device.bPaired;
		}
	);
}

void FOpenMobileHapticsPreviewTransport::SetStatus(
	FText Status,
	bool bError
)
{
	StatusText = MoveTemp(Status);
	bStatusError = bError;
	Changed.Broadcast();
}

#undef LOCTEXT_NAMESPACE
