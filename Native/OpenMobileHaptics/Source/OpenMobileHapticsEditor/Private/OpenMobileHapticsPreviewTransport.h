#pragma once

#include "Containers/Ticker.h"
#include "CoreMinimal.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "OpenMobileHapticsPreviewProtocol.h"

class FSocket;
class UOpenMobileHapticPatternAsset;

struct FOpenMobileHapticsPreviewDiscoveredDevice
{
	FGuid ReceiverId;
	FString Label;
	FIPv4Endpoint Endpoint;
	FOpenMobileHapticsPreviewCapabilities Capabilities;
	double LastSeenSeconds = 0.0;
	FGuid PairingRequestId;
	FString PairingCode;
	double PairingExpirationSeconds = 0.0;
	double LastPairingRequestSeconds = 0.0;
	FGuid SessionId;
	bool bPairingPending = false;
	bool bPaired = false;
};

class FOpenMobileHapticsPreviewTransport final
	: public TSharedFromThis<FOpenMobileHapticsPreviewTransport>
{
public:
	/** Creates a fresh editor identity for discovery and pairing, sockets stay closed till Start. */
	FOpenMobileHapticsPreviewTransport();
	/** Stops ticker, socket, and PIE callbacks before discovered device state is released. */
	~FOpenMobileHapticsPreviewTransport();

	/** Opens the UDP endpoint and ticker once, returning false when the editor can't bind preview transport. */
	bool Start();
	/** Stops network activity and invalidates pairing sessions without touching pattern assets. */
	void Shutdown();
	/** Broadcasts a discovery request and clears receivers that haven't been seen within the timeout. */
	void Discover();
	/** Cycles through live receivers so the toolbar can select without owning device-array indices. */
	void SelectNextDevice();
	/** Starts or refreshes pairing for the selected receiver while throttling repeated requests. */
	bool RequestPairing();
	/** Serializes one validated pattern into the paired preview session with a monotonic revision. */
	bool SendPreview(const UOpenMobileHapticPatternAsset& Asset);
	/** Asks the paired receiver to stop its current preview and invalidates the local revision. */
	void StopPreview();

	/** Returns the selected live receiver, selection can become stale between discovery ticks. */
	const FOpenMobileHapticsPreviewDiscoveredDevice* GetSelectedDevice() const;
	/** Formats selected receiver and pairing state for the editor toolbar. */
	FText GetSelectedDeviceText() const;
	/** Returns the latest transport or receiver response without exposing error-state storage. */
	FText GetStatusText() const;
	/** Allows pairing only when a live selected receiver isn't already paired. */
	bool CanRequestPairing() const;
	/** Allows preview only after a selected receiver owns a valid paired session. */
	bool CanSendPreview() const;
	/** Lets Slate refresh device and status widgets after network state changes. */
	FSimpleMulticastDelegate& OnChanged();

private:
	/** Drains UDP messages, expires receivers, and maintains heartbeat state on the editor ticker. */
	bool Tick(float DeltaTime);
	/** Stops device preview when PIE ends so a handset doesn't continue playing stale editor state. */
	void HandleEndPIE(bool bIsSimulating);
	/** Encodes and sends one protocol message to an explicit endpoint, pairing doesn't alter the destination implicitly. */
	bool Send(
		const FIPv4Endpoint& Endpoint,
		FOpenMobileHapticsPreviewMessage Message
	);
	/** Applies discovery, pairing, heartbeat, and preview replies only from their claimed receiver endpoint. */
	void HandleMessage(
		const FIPv4Endpoint& Endpoint,
		const FOpenMobileHapticsPreviewMessage& Message
	);
	/** Issues one pairing challenge and stores its expiry alongside the receiver. */
	bool SendPairingRequest(FOpenMobileHapticsPreviewDiscoveredDevice& Device);
	/** Finds mutable receiver state by stable id rather than its changing discovery order. */
	FOpenMobileHapticsPreviewDiscoveredDevice* FindDevice(const FGuid& Id);
	/** Finds the selected paired receiver for send paths that must mutate session timestamps. */
	FOpenMobileHapticsPreviewDiscoveredDevice* FindPairedDevice();
	/** Finds the selected paired receiver for toolbar checks without exposing mutation. */
	const FOpenMobileHapticsPreviewDiscoveredDevice* FindPairedDevice() const;
	/** Updates user-facing status and broadcasts once so every toolbar control refreshes together. */
	void SetStatus(FText Status, bool bError = false);

	FSocket* Socket = nullptr;
	FGuid EditorId = FGuid::NewGuid();
	TArray<FOpenMobileHapticsPreviewDiscoveredDevice> Devices;
	FGuid SelectedReceiverId;
	FTSTicker::FDelegateHandle TickerHandle;
	uint64 NextRevision = 0;
	double LastHeartbeatSeconds = 0.0;
	double LastSessionResponseSeconds = 0.0;
	FText StatusText;
	bool bStatusError = false;
	FSimpleMulticastDelegate Changed;
};
