#pragma once

#include "OpenMobileHapticsPlatformBackend.h"

class FOpenMobileHapticsAppleBridgeService;

class FOpenMobileHapticsIOSBackend final
	: public FOpenMobileHapticsPlatformBackend
{
public:
	/** Creates the Objective-C bridge service once so callbacks have stable ownership for the backend lifetime. */
	FOpenMobileHapticsIOSBackend();
	/** Shuts the bridge down before backend memory disappears, native blocks can complete after stop was requested. */
	virtual ~FOpenMobileHapticsIOSBackend() override;

	/** Keeps tokens and diagnostics tied to the iOS backend identity. */
	virtual FName GetBackendName() const override { return TEXT("IOS"); }
	/** Builds capability state from the cached Apple hardware probe and current OS support. */
	virtual FOpenMobileHapticCapabilities GetCapabilities() const override;
	/** Reports whether native players and generators were prepared for the current resource generation. */
	virtual EOpenMobileHapticPreparationState
	GetPreparationState() const override;
	/** Prepares portable Apple timelines as one bounded batch, releasing partial native state on failure. */
	virtual FOpenMobileHapticsBackendPreparationResult PrepareResources(
		const FOpenMobileHapticsBackendPreparationRequest& Request
	) override;
	/** Releases prepared Core Haptics players while leaving immediate semantic playback available. */
	virtual void ReleasePreparedResources() override;
	/** Advertises stop, pause, resume, seek, and dynamic updates supported by Core Haptics. */
	virtual FOpenMobileHapticsBackendControlSupport
	GetControlSupport() const override;
	/** Invalidates probe and engine state for older callers without a detailed application transition. */
	virtual void HandleLifecycleChange() override;
	/** Stops or refreshes Apple native services according to the resolved application state change. */
	virtual void HandleApplicationLifecycle(
		const FOpenMobileHapticsLifecycleTransition& Transition
	) override;
	/** Invalidates engine state after Core Haptics or audio-session interruption. */
	virtual void HandleInterruption(
		EOpenMobileHapticsInterruptionReason Reason
	) override;
	/** Recreates Core Haptics state and reports whether registry backoff should try again. */
	virtual EOpenMobileHapticsRecoveryResult
	RecoverFromInterruption() override;
	/** Submits system semantic or basic vibration using timing and callback ownership already resolved upstream. */
	virtual FOpenMobileHapticsBackendSubmission SubmitSemantic(
		const FOpenMobileHapticSemanticRequest& Request,
		const FOpenMobileHapticsSemanticResolution& Resolution,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override;
	/** Submits a one-shot through Apple's accepted semantic route while preserving scheduled start rules. */
	virtual FOpenMobileHapticsBackendSubmission SubmitOneShot(
		const FOpenMobileHapticOneShotRequest& Request,
		const FOpenMobileHapticsOneShotResolution& Resolution,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override;
	/** Submits prepared AHAP, transient, or continuous playback and wires its per-player control support. */
	virtual FOpenMobileHapticsBackendSubmission SubmitNamedPattern(
		const FOpenMobileHapticNamedPatternRequest& Request,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override;
	/** Stops the native player for one current token and forgets any AHAP intensity composition state. */
	virtual FOpenMobileHapticControlResult StopPlayback(
		const FOpenMobileHapticsBackendRequestToken& Token
	) override;
	/** Pauses a controllable Apple player using the subsystem's accepted control revision. */
	virtual FOpenMobileHapticControlResult PausePlayback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticsBackendControlCommand& Command
	) override;
	/** Resumes an existing Apple player, missing or completed players stay rejected. */
	virtual FOpenMobileHapticControlResult ResumePlayback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticsBackendControlCommand& Command
	) override;
	/** Seeks a compatible Apple player to the policy-resolved position. */
	virtual FOpenMobileHapticControlResult SeekPlayback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticsBackendControlCommand& Command
	) override;
	/** Composes live AHAP intensity with its static request scale before sending the native update. */
	virtual FOpenMobileHapticControlResult UpdatePlaybackParameters(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	) override;
	/** Seals native callbacks, players, prepared state, and AHAP scale tracking during module teardown. */
	virtual void BeginShutdown() override;

private:
	/** Maps Apple hardware probe states to the stable capability contract without exposing Objective-C types. */
	FOpenMobileHapticCapabilities ProbeHardwareCapabilities() const;
	/** Retains static AHAP intensity by request because later dynamic updates arrive without the original request. */
	void RememberAHAPIntensityScale(uint64 RequestId, float Scale);
	/** Removes intensity state with terminal playback so reused ids don't inherit an old scale. */
	void ForgetAHAPIntensityScale(uint64 RequestId);
	/** Clears every retained scale when engine lifecycle is invalidated. */
	void ForgetAllAHAPIntensityScales();
	/** Combines the retained static scale with one live update while leaving unrelated parameters unchanged. */
	FOpenMobileHapticDynamicParameterUpdate ComposeAHAPDynamicUpdate(
		uint64 RequestId,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	) const;

	TUniquePtr<FOpenMobileHapticsAppleBridgeService> BridgeService;
	mutable FCriticalSection PreparationMutex;
	EOpenMobileHapticPreparationState PreparationState =
		EOpenMobileHapticPreparationState::Unprepared;
	mutable FCriticalSection AHAPIntensityMutex;
	TMap<uint64, float> AHAPStaticIntensityScales;
};
