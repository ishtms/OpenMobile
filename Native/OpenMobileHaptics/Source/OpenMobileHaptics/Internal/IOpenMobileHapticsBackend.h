#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobileHapticsTypes.h"

enum class EOpenMobileHapticsBackendPreparationState : uint8
{
	Unprepared,
	Preparing,
	Prepared,
	Failed
};

struct FOpenMobileHapticsBackendControlSupport
{
	bool bStop = false;
	bool bStopChannel = false;
	bool bStopAll = false;
	bool bPause = false;
	bool bResume = false;
	bool bSeek = false;
	bool bDynamicParameters = false;
};

struct FOpenMobileHapticsBackendRequestToken
{
	uint64 RegistryGeneration = 0;
	uint64 RequestId = 0;
	FName BackendName;
	FOpenMobileHapticPlaybackHandle PlaybackHandle;

	bool IsValid() const
	{
		return RegistryGeneration != 0
			&& RequestId != 0
			&& !BackendName.IsNone();
	}

	bool operator==(const FOpenMobileHapticsBackendRequestToken& Other) const
	{
		return RegistryGeneration == Other.RegistryGeneration
			&& RequestId == Other.RequestId
			&& BackendName == Other.BackendName
			&& PlaybackHandle == Other.PlaybackHandle;
	}
};

struct FOpenMobileHapticsBackendCallback
{
	FOpenMobileHapticsBackendRequestToken Token;
	uint64 Sequence = 0;
	FOpenMobileHapticPlaybackEvent Event;
};

using FOpenMobileHapticsBackendEventCallback =
	TFunction<void(const FOpenMobileHapticsBackendCallback&)>;

struct FOpenMobileHapticsBackendSubmission
{
	FOpenMobileHapticPlaybackResult Result;
	bool bCreatesControllablePlayback = false;
	bool bExpectsCallbacks = false;
};

class IOpenMobileHapticsBackend : public IModularFeature
{
public:
	virtual ~IOpenMobileHapticsBackend() = default;

	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("OpenMobile.Haptics.Backend"));
		return FeatureName;
	}

	virtual FName GetBackendName() const = 0;
	virtual int32 GetPriority() const { return 0; }
	virtual bool IsAvailable() const { return true; }
	virtual FOpenMobileHapticCapabilities GetCapabilities() const = 0;
	virtual EOpenMobileHapticsBackendPreparationState
	GetPreparationState() const = 0;
	virtual FOpenMobileHapticsBackendControlSupport
	GetControlSupport() const = 0;
	virtual void HandleLifecycleChange() {}

	virtual FOpenMobileHapticsBackendSubmission SubmitSemantic(
		const FOpenMobileHapticSemanticRequest& Request,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) = 0;
	virtual FOpenMobileHapticsBackendSubmission SubmitOneShot(
		const FOpenMobileHapticOneShotRequest& Request,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) = 0;
	virtual FOpenMobileHapticsBackendSubmission SubmitNamedPattern(
		const FOpenMobileHapticNamedPatternRequest& Request,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) = 0;

	virtual FOpenMobileHapticControlResult StopPlayback(
		const FOpenMobileHapticsBackendRequestToken& Token
	) = 0;
	virtual FOpenMobileHapticControlResult StopChannel(FName Channel)
	{
		static_cast<void>(Channel);
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::Unsupported;
		return Result;
	}
	virtual FOpenMobileHapticControlResult StopAll()
	{
		FOpenMobileHapticControlResult Result;
		Result.Outcome = EOpenMobileHapticControlOutcome::Unsupported;
		return Result;
	}
	virtual void BeginShutdown() = 0;
};
