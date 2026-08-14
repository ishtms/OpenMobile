#pragma once

#include "IOpenMobileHapticsBackend.h"

class FOpenMobileHapticsPlatformBackend : public IOpenMobileHapticsBackend
{
public:
	/** Leaves room for real platform backends to outrank this safe fallback implementation. */
	virtual int32 GetPriority() const override { return 100; }
	/** Keeps the fallback selectable when no platform module registered, its submissions still reject cleanly. */
	virtual bool IsAvailable() const override { return true; }
	/** Reports unprepared because this fallback never owns native resources. */
	virtual EOpenMobileHapticPreparationState
	GetPreparationState() const override
	{
		return EOpenMobileHapticPreparationState::Unprepared;
	}
	/** Rejects preparation with a useful message instead of pretending unsupported resources are ready. */
	virtual FOpenMobileHapticsBackendPreparationResult PrepareResources(
		const FOpenMobileHapticsBackendPreparationRequest& Request
	) override
	{
		static_cast<void>(Request);
		FOpenMobileHapticsBackendPreparationResult Result;
		Result.Errors.Add(
			TEXT("The active Haptics backend cannot prepare resources.")
		);
		return Result;
	}
	/** Has no native cache to release, the empty override keeps shutdown uniform for callers. */
	virtual void ReleasePreparedResources() override {}
	/** Advertises no pause, resume, seek, or parameter support from the fallback backend. */
	virtual FOpenMobileHapticsBackendControlSupport
	GetControlSupport() const override
	{
		return {};
	}
	/** Rejects semantic submission through the shared unsupported result path. */
	virtual FOpenMobileHapticsBackendSubmission SubmitSemantic(
		const FOpenMobileHapticSemanticRequest& Request,
		const FOpenMobileHapticsSemanticResolution& Resolution,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override
	{
		static_cast<void>(Request);
		static_cast<void>(Resolution);
		static_cast<void>(Parameters);
		static_cast<void>(Token);
		static_cast<void>(Callback);
		return MakeUnsupportedSubmission();
	}
	/** Rejects one-shot submission without invoking a callback for work that never started. */
	virtual FOpenMobileHapticsBackendSubmission SubmitOneShot(
		const FOpenMobileHapticOneShotRequest& Request,
		const FOpenMobileHapticsOneShotResolution& Resolution,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override
	{
		static_cast<void>(Request);
		static_cast<void>(Resolution);
		static_cast<void>(Parameters);
		static_cast<void>(Token);
		static_cast<void>(Callback);
		return MakeUnsupportedSubmission();
	}
	/** Rejects named playback while preserving the normal backend contract. */
	virtual FOpenMobileHapticsBackendSubmission SubmitNamedPattern(
		const FOpenMobileHapticNamedPatternRequest& Request,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override
	{
		static_cast<void>(Request);
		static_cast<void>(Parameters);
		static_cast<void>(Token);
		static_cast<void>(Callback);
		return MakeUnsupportedSubmission();
	}
	/** Rejects stop because no playback token can belong to this fallback backend. */
	virtual FOpenMobileHapticControlResult StopPlayback(
		const FOpenMobileHapticsBackendRequestToken& Token
	) override
	{
		static_cast<void>(Token);
		return FOpenMobileHapticControlResult::MakeRejected(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The active Haptics backend does not support playback yet.")
		);
	}
	/** Needs no teardown work, but callers can still shut every backend down through one interface. */
	virtual void BeginShutdown() override {}

private:
	/** Builds the same empty-message rejection for every unsupported submission route. */
	static FOpenMobileHapticsBackendSubmission MakeUnsupportedSubmission()
	{
		FOpenMobileHapticsBackendSubmission Submission;
		Submission.Result = FOpenMobileHapticPlaybackResult::MakeRejected(
			EOpenMobileErrorCode::NotSupported,
			FString()
		);
		return Submission;
	}
};
