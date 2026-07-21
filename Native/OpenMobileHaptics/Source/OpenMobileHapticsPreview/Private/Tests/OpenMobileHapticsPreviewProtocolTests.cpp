#if WITH_DEV_AUTOMATION_TESTS && OPENMOBILE_HAPTICS_PREVIEW_ENABLED

#include "Misc/AutomationTest.h"
#include "OpenMobileHapticsPreviewProtocol.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsPreviewProtocolTest,
	"OpenMobile.Haptics.Preview.Protocol",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsPreviewProtocolTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FGuid FirstEditor = FGuid::NewGuid();
	const FGuid SecondEditor = FGuid::NewGuid();
	const FGuid FirstRequest = FGuid::NewGuid();
	const FGuid SecondRequest = FGuid::NewGuid();
	const FGuid WireSession = FGuid::NewGuid();

	FOpenMobileHapticsPreviewMessage Preview;
	Preview.Type = EOpenMobileHapticsPreviewMessageType::Preview;
	Preview.SenderId = FirstEditor;
	Preview.SessionId = WireSession;
	Preview.Revision = 7;
	Preview.Pattern.CapabilitySignature = 12345;
	Preview.Pattern.Category = TEXT("Gameplay Preview");
	Preview.Pattern.PrimitiveOrPresetFallback = TEXT("HeavyClick");
	Preview.Pattern.CookedPattern.SourceHash = 42;
	Preview.Pattern.CookedPattern.DurationMicroseconds = 100000;
	Preview.Pattern.CookedPattern.GranularityMicroseconds = 1000;
	FOpenMobileHapticCookedPatternEvent& Event =
		Preview.Pattern.CookedPattern.Events.AddDefaulted_GetRef();
	Event.Type = EOpenMobileHapticPatternEventType::Continuous;
	Event.DurationMicroseconds = 100000;

	TArray<uint8> Packet;
	FString Error;
	TestTrue(TEXT("Compiled preview encodes"),
		FOpenMobileHapticsPreviewProtocol::Encode(Preview, Packet, Error));
	TestTrue(TEXT("Encoded preview stays under the packet limit"),
		Packet.Num() <= FOpenMobileHapticsPreviewProtocol::MaximumPacketBytes);
	FOpenMobileHapticsPreviewMessage Decoded;
	TestTrue(TEXT("Compiled preview decodes"),
		FOpenMobileHapticsPreviewProtocol::Decode(Packet, Decoded, Error));
	TestEqual(TEXT("Revision survives transport"), Decoded.Revision,
		Preview.Revision);
	TestEqual(TEXT("Cooked hash survives transport"),
		Decoded.Pattern.CookedPattern.SourceHash,
		Preview.Pattern.CookedPattern.SourceHash);
	TestEqual(TEXT("Capability signature survives transport"),
		Decoded.Pattern.CapabilitySignature,
		Preview.Pattern.CapabilitySignature);
	if (Packet.Num() <= 4)
	{
		AddError(TEXT("Encoded preview packet is unexpectedly empty."));
		return false;
	}

	TArray<uint8> WrongVersion = Packet;
	WrongVersion[4] ^= 0xff;
	TestFalse(TEXT("Version mismatch is rejected"),
		FOpenMobileHapticsPreviewProtocol::Decode(
			WrongVersion,
			Decoded,
			Error
		));
	TArray<uint8> Truncated = Packet;
	Truncated.SetNum(Truncated.Num() - 1);
	TestFalse(TEXT("Truncated payload is rejected"),
		FOpenMobileHapticsPreviewProtocol::Decode(
			Truncated,
			Decoded,
			Error
		));
	TArray<uint8> Oversized;
	Oversized.SetNumZeroed(
		FOpenMobileHapticsPreviewProtocol::MaximumPacketBytes + 1
	);
	TestFalse(TEXT("Oversized payload is rejected"),
		FOpenMobileHapticsPreviewProtocol::Decode(
			Oversized,
			Decoded,
			Error
		));
	Preview.Pattern.Category = TEXT("/tmp/pattern");
	TestFalse(TEXT("Pattern paths are not accepted as identifiers"),
		FOpenMobileHapticsPreviewProtocol::Encode(Preview, Packet, Error));
	Preview.Pattern.Category = TEXT("Preview");
	Preview.Pattern.CookedPattern.DurationMicroseconds = 0;
	Event.Type = EOpenMobileHapticPatternEventType::Transient;
	Event.DurationMicroseconds = 0;
	TestTrue(TEXT("A transient at time zero remains previewable"),
		FOpenMobileHapticsPreviewProtocol::Encode(Preview, Packet, Error));

	FOpenMobileHapticsPreviewMessage Result;
	Result.Type = EOpenMobileHapticsPreviewMessageType::Result;
	Result.SenderId = SecondEditor;
	Result.SessionId = WireSession;
	Result.Revision = 7;
	Result.ResultCode = EOpenMobileHapticsPreviewResultCode::Accepted;
	Result.Capabilities.Signature = 12345;
	Result.ResolvedPath = TEXT("PortableTimeline");
	Result.ResolvedStartTimeSeconds = 12.25;
	Result.EstimatedPrecisionSeconds = 0.004;
	TestTrue(TEXT("Resolved playback result encodes"),
		FOpenMobileHapticsPreviewProtocol::Encode(Result, Packet, Error));
	TestTrue(TEXT("Resolved playback result decodes"),
		FOpenMobileHapticsPreviewProtocol::Decode(Packet, Decoded, Error));
	TestEqual(TEXT("Resolved path returns to the editor"),
		Decoded.ResolvedPath,
		Result.ResolvedPath);
	TestEqual(TEXT("Resolved start time returns to the editor"),
		Decoded.ResolvedStartTimeSeconds,
		Result.ResolvedStartTimeSeconds);
	TestEqual(TEXT("Timing precision returns to the editor"),
		Decoded.EstimatedPrecisionSeconds,
		Result.EstimatedPrecisionSeconds);

	FOpenMobileHapticsPreviewSessionPolicy Policy;
	TestEqual(TEXT("First editor starts pairing"),
		Policy.BeginPairing(FirstEditor, FirstRequest),
		EOpenMobileHapticsPreviewPairDecision::Started);
	TestEqual(TEXT("Lost pending response can be retried"),
		Policy.BeginPairing(FirstEditor, FirstRequest),
		EOpenMobileHapticsPreviewPairDecision::ExistingPending);
	TestEqual(TEXT("Second editor is rejected while pairing"),
		Policy.BeginPairing(SecondEditor, SecondRequest),
		EOpenMobileHapticsPreviewPairDecision::Busy);
	TestTrue(TEXT("Host approval creates a session"),
		Policy.ApprovePairing(FirstRequest, 100.0));
	const FGuid FirstSession = Policy.GetSessionId();
	TestTrue(TEXT("Approved session identity is valid"),
		FirstSession.IsValid());
	TestEqual(TEXT("Lost approval response returns the same session"),
		Policy.BeginPairing(FirstEditor, FirstRequest),
		EOpenMobileHapticsPreviewPairDecision::ExistingSession);
	TestTrue(TEXT("Only the approved editor matches the session"),
		Policy.MatchesSession(FirstEditor, FirstSession));
	TestFalse(TEXT("Another editor cannot reuse the session"),
		Policy.MatchesSession(SecondEditor, FirstSession));

	TestEqual(TEXT("First revision is admitted"),
		Policy.AdmitPreview(1, 100.0, 0),
		EOpenMobileHapticsPreviewResultCode::Accepted);
	TestEqual(TEXT("Repeated revision is stale"),
		Policy.AdmitPreview(1, 100.0, 0),
		EOpenMobileHapticsPreviewResultCode::StaleRevision);
	TestEqual(TEXT("Full queue rejects new work"),
		Policy.AdmitPreview(
			2,
			100.0,
			FOpenMobileHapticsPreviewProtocol::MaximumPreviewQueueDepth
		),
		EOpenMobileHapticsPreviewResultCode::QueueFull);
	TestEqual(TEXT("Second revision is admitted when capacity returns"),
		Policy.AdmitPreview(2, 100.0, 0),
		EOpenMobileHapticsPreviewResultCode::Accepted);
	TestEqual(TEXT("Third revision is admitted"),
		Policy.AdmitPreview(3, 100.0, 0),
		EOpenMobileHapticsPreviewResultCode::Accepted);
	TestEqual(TEXT("Fourth revision is admitted"),
		Policy.AdmitPreview(4, 100.0, 0),
		EOpenMobileHapticsPreviewResultCode::Accepted);
	TestEqual(TEXT("Request burst is rate limited"),
		Policy.AdmitPreview(5, 100.0, 0),
		EOpenMobileHapticsPreviewResultCode::RateLimited);
	TestTrue(TEXT("Idle session expires"),
		Policy.IsSessionExpired(
			100.0
			+ FOpenMobileHapticsPreviewProtocol::SessionIdleTimeoutSeconds
		));

	Policy.Disconnect();
	TestEqual(TEXT("Second editor can reconnect after disconnect"),
		Policy.BeginPairing(SecondEditor, SecondRequest),
		EOpenMobileHapticsPreviewPairDecision::Started);
	TestTrue(TEXT("Reconnect requires fresh host approval"),
		Policy.ApprovePairing(SecondRequest, 200.0));
	TestNotEqual(TEXT("Reconnect rotates the session identity"),
		Policy.GetSessionId(),
		FirstSession);
	TestTrue(TEXT("Absolute session lifetime is bounded"),
		Policy.IsSessionExpired(
			200.0
			+ FOpenMobileHapticsPreviewProtocol::SessionLifetimeSeconds
		));
	return true;
}

#endif
