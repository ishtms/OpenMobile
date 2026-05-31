#pragma once

#include "IOpenMobileHapticsBackend.h"

class FOpenMobileHapticsPlatformBackend : public IOpenMobileHapticsBackend
{
public:
	virtual int32 GetPriority() const override { return 100; }
	virtual bool IsAvailable() const override { return true; }
	virtual EOpenMobileHapticsBackendPreparationState
	GetPreparationState() const override
	{
		return EOpenMobileHapticsBackendPreparationState::Unprepared;
	}
	virtual FOpenMobileHapticsBackendControlSupport
	GetControlSupport() const override
	{
		return {};
	}
	virtual FOpenMobileHapticsBackendSubmission SubmitSemantic(
		const FOpenMobileHapticSemanticRequest& Request,
		const FOpenMobileHapticsSemanticResolution& Resolution,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override
	{
		static_cast<void>(Request);
		static_cast<void>(Resolution);
		static_cast<void>(Token);
		static_cast<void>(Callback);
		return MakeUnsupportedSubmission();
	}
	virtual FOpenMobileHapticsBackendSubmission SubmitOneShot(
		const FOpenMobileHapticOneShotRequest& Request,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override
	{
		static_cast<void>(Request);
		static_cast<void>(Token);
		static_cast<void>(Callback);
		return MakeUnsupportedSubmission();
	}
	virtual FOpenMobileHapticsBackendSubmission SubmitNamedPattern(
		const FOpenMobileHapticNamedPatternRequest& Request,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override
	{
		static_cast<void>(Request);
		static_cast<void>(Token);
		static_cast<void>(Callback);
		return MakeUnsupportedSubmission();
	}
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
	virtual void BeginShutdown() override {}

private:
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
