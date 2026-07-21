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
	FOpenMobileHapticsPreviewTransport();
	~FOpenMobileHapticsPreviewTransport();

	bool Start();
	void Shutdown();
	void Discover();
	void SelectNextDevice();
	bool RequestPairing();
	bool SendPreview(const UOpenMobileHapticPatternAsset& Asset);
	void StopPreview();

	const FOpenMobileHapticsPreviewDiscoveredDevice* GetSelectedDevice() const;
	FText GetSelectedDeviceText() const;
	FText GetStatusText() const;
	bool CanRequestPairing() const;
	bool CanSendPreview() const;
	FSimpleMulticastDelegate& OnChanged();

private:
	bool Tick(float DeltaTime);
	void HandleEndPIE(bool bIsSimulating);
	bool Send(
		const FIPv4Endpoint& Endpoint,
		FOpenMobileHapticsPreviewMessage Message
	);
	void HandleMessage(
		const FIPv4Endpoint& Endpoint,
		const FOpenMobileHapticsPreviewMessage& Message
	);
	bool SendPairingRequest(FOpenMobileHapticsPreviewDiscoveredDevice& Device);
	FOpenMobileHapticsPreviewDiscoveredDevice* FindDevice(const FGuid& Id);
	FOpenMobileHapticsPreviewDiscoveredDevice* FindPairedDevice();
	const FOpenMobileHapticsPreviewDiscoveredDevice* FindPairedDevice() const;
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
