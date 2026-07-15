#include "OpenMobileHapticsBackendRegistry.h"

#include "Features/IModularFeatures.h"
#include "Containers/Ticker.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformTime.h"
#include "IOpenMobileHapticsBackend.h"
#include "Misc/ScopeLock.h"
#include "OpenMobileHapticsRecoveryPolicy.h"
#include "OpenMobileHapticsSettings.h"
#include "OpenMobileHapticsTimelineManager.h"

namespace OpenMobileHapticsBackendRegistryPrivate
{
	TAtomic<uint64> Generation(1);
	TAtomic<uint64> LifecycleGeneration(1);
	TAtomic<bool> bShuttingDown(false);
	TAtomic<bool> bApplicationActive(true);
	uint64 NextRequestId = 1;
	TSet<IOpenMobileHapticsBackend*> ShutdownBackends;
	FCriticalSection CapabilityMutex;
	FOpenMobileHapticCapabilities CapabilitySnapshot;
	FOpenMobileHapticsRecoveryPolicy RecoveryPolicy;
	FOpenMobileHapticsLifecyclePolicy LifecyclePolicy;
	FName RecoveryBackendName;
	bool bRecoveryRequested = false;
	FTSTicker::FDelegateHandle RecoveryTickerHandle;
	FOpenMobileHapticsInterruptionDelegate InterruptionDelegate;
	FOpenMobileHapticsRecoveryDelegate RecoveryDelegate;
	FOpenMobileHapticsApplicationLifecycleDelegate LifecycleDelegate;

	FOpenMobileHapticsTimelineManager& TimelineManager()
	{
		static FOpenMobileHapticsTimelineManager Manager;
		return Manager;
	}

	void AdvanceGeneration()
	{
		Generation++;
		if (Generation.Load() == 0)
		{
			Generation++;
		}
	}

	void AdvanceLifecycleGeneration()
	{
		LifecycleGeneration++;
		if (LifecycleGeneration.Load() == 0)
		{
			LifecycleGeneration++;
		}
	}

	uint64 AllocateRequestId()
	{
		const uint64 RequestId = NextRequestId++;
		if (NextRequestId == 0)
		{
			NextRequestId = 1;
		}
		return RequestId;
	}

	TArray<IOpenMobileHapticsBackend*> GetBackends()
	{
		return IModularFeatures::Get()
			.GetModularFeatureImplementations<IOpenMobileHapticsBackend>(
				IOpenMobileHapticsBackend::GetModularFeatureName()
			);
	}

	IOpenMobileHapticsBackend* SelectBackend()
	{
		if (bShuttingDown.Load())
		{
			return nullptr;
		}

		IOpenMobileHapticsBackend* Best = nullptr;
		for (IOpenMobileHapticsBackend* Candidate : GetBackends())
		{
			if (!Candidate || !Candidate->IsAvailable())
			{
				continue;
			}

			const bool bHigherPriority = !Best
				|| Candidate->GetPriority() > Best->GetPriority();
			const bool bStableTieBreak = Best
				&& Candidate->GetPriority() == Best->GetPriority()
				&& Candidate->GetBackendName().LexicalLess(
					Best->GetBackendName()
				);
			if (bHigherPriority || bStableTieBreak)
			{
				Best = Candidate;
			}
		}
		return Best;
	}

	IOpenMobileHapticsBackend* FindBackendByName(FName BackendName)
	{
		for (IOpenMobileHapticsBackend* Backend : GetBackends())
		{
			if (Backend && Backend->GetBackendName() == BackendName)
			{
				return Backend;
			}
		}
		return nullptr;
	}

	void PublishCapabilities()
	{
		FOpenMobileHapticCapabilities Capabilities;
		if (IOpenMobileHapticsBackend* Backend = SelectBackend())
		{
			Capabilities = Backend->GetCapabilities();
			if (Capabilities.BackendName.IsNone())
			{
				Capabilities.BackendName = Backend->GetBackendName();
			}
		}
		else
		{
			Capabilities.Detail =
				TEXT("No mobile Haptics backend is available.");
		}
		if (RecoveryPolicy.IsRecovering())
		{
			Capabilities.Availability =
				EOpenMobileHapticAvailability::TemporarilyUnavailable;
			Capabilities.Detail =
				TEXT("The mobile Haptics backend is recovering from an interruption.");
		}

		FScopeLock Lock(&CapabilityMutex);
		CapabilitySnapshot = MoveTemp(Capabilities);
	}

	void StopBackend(IOpenMobileHapticsBackend& Backend)
	{
		if (!ShutdownBackends.Contains(&Backend))
		{
			ShutdownBackends.Add(&Backend);
			Backend.BeginShutdown();
		}
	}

	void InterruptBackendWithoutRecovery(
		IOpenMobileHapticsBackend& Backend,
		EOpenMobileHapticsInterruptionReason Reason
	)
	{
		AdvanceGeneration();
		AdvanceLifecycleGeneration();
		TimelineManager().Clear();
		FOpenMobileHapticsInterruption Interruption;
		Interruption.BackendName = Backend.GetBackendName();
		Interruption.Reason = Reason;
		Interruption.LifecycleGeneration = LifecycleGeneration.Load();
		InterruptionDelegate.Broadcast(Interruption);
		Backend.HandleInterruption(Reason);
	}

	void CancelRecoveryTicker()
	{
		if (RecoveryTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(RecoveryTickerHandle);
			RecoveryTickerHandle.Reset();
		}
	}

	void RunRecoveryAttempt(double NowSeconds);

	void ScheduleRecoveryAttempt()
	{
		if (RecoveryTickerHandle.IsValid()
			|| !RecoveryPolicy.IsRecovering()
			|| RecoveryPolicy.IsExhausted()
			|| !bRecoveryRequested
			|| !bApplicationActive.Load()
			|| bShuttingDown.Load())
		{
			return;
		}
		const double NowSeconds = FPlatformTime::Seconds();
		const double DelaySeconds = FMath::Max(
			0.01,
			RecoveryPolicy.GetNextAttemptTimeSeconds() - NowSeconds
		);
		RecoveryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([](float)
			{
				RecoveryTickerHandle.Reset();
				RunRecoveryAttempt(FPlatformTime::Seconds());
				return false;
			}),
			static_cast<float>(DelaySeconds)
		);
	}

	void RunRecoveryAttempt(double NowSeconds)
	{
		const FOpenMobileHapticsRecoveryAttempt Attempt =
			RecoveryPolicy.TryBeginAttempt(
				NowSeconds,
				bApplicationActive.Load(),
				bRecoveryRequested
			);
		if (Attempt.Outcome
			!= EOpenMobileHapticsRecoveryAttemptOutcome::Started)
		{
			if (Attempt.Outcome
				== EOpenMobileHapticsRecoveryAttemptOutcome::Deferred)
			{
				ScheduleRecoveryAttempt();
			}
			return;
		}

		IOpenMobileHapticsBackend* Backend =
			FindBackendByName(RecoveryBackendName);
		const EOpenMobileHapticsRecoveryResult Result = Backend
			? Backend->RecoverFromInterruption()
			: EOpenMobileHapticsRecoveryResult::PermanentFailure;
		if (Result == EOpenMobileHapticsRecoveryResult::Recovered)
		{
			RecoveryPolicy.CompleteAttempt(true, NowSeconds);
			RecoveryBackendName = NAME_None;
			bRecoveryRequested = false;
			PublishCapabilities();
			RecoveryDelegate.Broadcast();
			return;
		}
		if (Result == EOpenMobileHapticsRecoveryResult::PermanentFailure)
		{
			RecoveryPolicy.Abandon();
			bRecoveryRequested = false;
			PublishCapabilities();
			return;
		}

		RecoveryPolicy.CompleteAttempt(false, NowSeconds);
		PublishCapabilities();
		if (RecoveryPolicy.IsExhausted())
		{
			bRecoveryRequested = false;
			return;
		}
		ScheduleRecoveryAttempt();
	}
}

void FOpenMobileHapticsBackendRegistry::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (bShuttingDown.Exchange(false))
	{
		ShutdownBackends.Reset();
		CancelRecoveryTicker();
		RecoveryPolicy.Reset();
		LifecyclePolicy.Reset();
		bApplicationActive.Store(true);
		RecoveryBackendName = NAME_None;
		bRecoveryRequested = false;
		AdvanceGeneration();
		TimelineManager().Clear();
	}
	PublishCapabilities();
}

bool FOpenMobileHapticsBackendRegistry::RegisterBackend(
	IOpenMobileHapticsBackend& Backend
)
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (bShuttingDown.Load() || Backend.GetBackendName().IsNone())
	{
		return false;
	}

	for (IOpenMobileHapticsBackend* Registered : GetBackends())
	{
		if (Registered == &Backend
			|| (Registered
				&& Registered->GetBackendName() == Backend.GetBackendName()))
		{
			return false;
		}
	}
	IOpenMobileHapticsBackend* PreviousBackend = SelectBackend();
	const bool bReplacingRecoveringBackend = PreviousBackend
		&& RecoveryPolicy.IsRecovering()
		&& RecoveryBackendName == PreviousBackend->GetBackendName();

	ShutdownBackends.Remove(&Backend);
	IModularFeatures::Get().RegisterModularFeature(
		IOpenMobileHapticsBackend::GetModularFeatureName(),
		&Backend
	);
	IOpenMobileHapticsBackend* SelectedBackend = SelectBackend();
	if (PreviousBackend && SelectedBackend != PreviousBackend)
	{
		if (bReplacingRecoveringBackend)
		{
			CancelRecoveryTicker();
			RecoveryPolicy.Reset();
			RecoveryBackendName = NAME_None;
			bRecoveryRequested = false;
		}
		else
		{
			InterruptBackendWithoutRecovery(
				*PreviousBackend,
				EOpenMobileHapticsInterruptionReason::BackendReplaced
			);
		}
	}
	else if (!PreviousBackend)
	{
		TimelineManager().Clear();
		AdvanceGeneration();
	}
	PublishCapabilities();
	if (PreviousBackend && SelectedBackend != PreviousBackend)
	{
		RecoveryDelegate.Broadcast();
	}
	return true;
}

bool FOpenMobileHapticsBackendRegistry::UnregisterBackend(
	IOpenMobileHapticsBackend& Backend
)
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (!GetBackends().Contains(&Backend))
	{
		return false;
	}
	IOpenMobileHapticsBackend* PreviousBackend = SelectBackend();
	const bool bRemovingSelectedBackend = PreviousBackend == &Backend;
	const bool bRemovingRecoveringBackend = RecoveryPolicy.IsRecovering()
		&& RecoveryBackendName == Backend.GetBackendName();
	if (bRemovingRecoveringBackend)
	{
		CancelRecoveryTicker();
		RecoveryPolicy.Reset();
		RecoveryBackendName = NAME_None;
		bRecoveryRequested = false;
	}

	IModularFeatures::Get().UnregisterModularFeature(
		IOpenMobileHapticsBackend::GetModularFeatureName(),
		&Backend
	);
	if (bRemovingSelectedBackend && !bShuttingDown.Load()
		&& !bRemovingRecoveringBackend)
	{
		InterruptBackendWithoutRecovery(
			Backend,
			EOpenMobileHapticsInterruptionReason::BackendReplaced
		);
	}
	StopBackend(Backend);
	ShutdownBackends.Remove(&Backend);
	TimelineManager().Clear();
	PublishCapabilities();
	if (bRemovingSelectedBackend && !bShuttingDown.Load()
		&& SelectBackend())
	{
		RecoveryDelegate.Broadcast();
	}
	return true;
}

bool FOpenMobileHapticsBackendRegistry::IsBackendRegistered(
	const IOpenMobileHapticsBackend* Backend
)
{
	check(IsInGameThread());
	return Backend
		&& OpenMobileHapticsBackendRegistryPrivate::GetBackends().Contains(
			Backend
		);
}

IOpenMobileHapticsBackend* FOpenMobileHapticsBackendRegistry::FindBackend()
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	return SelectBackend();
}

void FOpenMobileHapticsBackendRegistry::RefreshCapabilities()
{
	check(IsInGameThread());
	OpenMobileHapticsBackendRegistryPrivate::PublishCapabilities();
}

FOpenMobileHapticCapabilities
FOpenMobileHapticsBackendRegistry::GetCapabilitySnapshot()
{
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	FScopeLock Lock(&CapabilityMutex);
	return CapabilitySnapshot;
}

FOpenMobileHapticsBackendRequestToken
FOpenMobileHapticsBackendRegistry::CreateRequestToken(
	IOpenMobileHapticsBackend& Backend,
	bool bCreatePlaybackHandle
)
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (bShuttingDown.Load() || !GetBackends().Contains(&Backend))
	{
		return {};
	}

	FOpenMobileHapticsBackendRequestToken Token;
	Token.RegistryGeneration = Generation.Load();
	Token.RequestId = AllocateRequestId();
	Token.BackendName = Backend.GetBackendName();
	if (bCreatePlaybackHandle)
	{
		Token.PlaybackHandle.Id = FGuid::NewGuid();
	}
	return Token;
}

bool FOpenMobileHapticsBackendRegistry::IsCallbackCurrent(
	const FOpenMobileHapticsBackendRequestToken& Token
)
{
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	return Token.IsValid()
		&& !bShuttingDown.Load()
		&& Token.RegistryGeneration == Generation.Load();
}

void FOpenMobileHapticsBackendRegistry::NotifyLifecycleChange()
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (bShuttingDown.Load())
	{
		return;
	}
	AdvanceLifecycleGeneration();
	TimelineManager().Clear();
	for (IOpenMobileHapticsBackend* Backend : GetBackends())
	{
		if (Backend)
		{
			Backend->HandleLifecycleChange();
		}
	}
	PublishCapabilities();
}

void FOpenMobileHapticsBackendRegistry::SetApplicationActive(bool bActive)
{
	NotifyApplicationLifecycle(
		bActive
			? EOpenMobileHapticsLifecycleEvent::HasReactivated
			: EOpenMobileHapticsLifecycleEvent::WillEnterBackground
	);
}

bool FOpenMobileHapticsBackendRegistry::IsApplicationActive()
{
	return OpenMobileHapticsBackendRegistryPrivate::bApplicationActive.Load();
}

EOpenMobileHapticsApplicationState
FOpenMobileHapticsBackendRegistry::GetApplicationState()
{
	check(IsInGameThread());
	return OpenMobileHapticsBackendRegistryPrivate::LifecyclePolicy.GetState();
}

void FOpenMobileHapticsBackendRegistry::NotifyApplicationLifecycle(
	EOpenMobileHapticsLifecycleEvent Event
)
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (bShuttingDown.Load())
	{
		return;
	}
	const FOpenMobileHapticsLifecycleTransition Transition =
		LifecyclePolicy.Apply(Event);
	if (!Transition.bChanged)
	{
		return;
	}
	bApplicationActive.Store(
		Transition.CurrentState
			== EOpenMobileHapticsApplicationState::Active
	);
	if (Transition.bInterruptsPlayback)
	{
		AdvanceGeneration();
		AdvanceLifecycleGeneration();
		TimelineManager().Clear();
	}
	LifecycleDelegate.Broadcast(Transition);
	for (IOpenMobileHapticsBackend* Backend : GetBackends())
	{
		if (Backend)
		{
			Backend->HandleApplicationLifecycle(Transition);
		}
	}
	PublishCapabilities();
	if (Transition.bRefreshesNativeServices)
	{
		ScheduleRecoveryAttempt();
	}
}

uint64 FOpenMobileHapticsBackendRegistry::GetLifecycleGeneration()
{
	return OpenMobileHapticsBackendRegistryPrivate::LifecycleGeneration.Load();
}

FOpenMobileHapticsTimelineManager&
FOpenMobileHapticsBackendRegistry::GetTimelineManager()
{
	return OpenMobileHapticsBackendRegistryPrivate::TimelineManager();
}

void FOpenMobileHapticsBackendRegistry::NotifyInterruption(
	FName BackendName,
	EOpenMobileHapticsInterruptionReason Reason
)
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (bShuttingDown.Load() || BackendName.IsNone()
		|| !FindBackendByName(BackendName))
	{
		return;
	}
	if (RecoveryPolicy.IsRecovering()
		&& RecoveryBackendName == BackendName)
	{
		return;
	}
	CancelRecoveryTicker();
	RecoveryPolicy.Reset();
	RecoveryBackendName = BackendName;
	bRecoveryRequested = false;
	AdvanceGeneration();
	AdvanceLifecycleGeneration();
	TimelineManager().Clear();
	RecoveryPolicy.BeginInterruption(
		FPlatformTime::Seconds(),
		GetDefault<UOpenMobileHapticsSettings>()->MaximumRecoveryAttempts
	);
	PublishCapabilities();

	FOpenMobileHapticsInterruption Interruption;
	Interruption.BackendName = BackendName;
	Interruption.Reason = Reason;
	Interruption.LifecycleGeneration = LifecycleGeneration.Load();
	InterruptionDelegate.Broadcast(Interruption);
	if (IOpenMobileHapticsBackend* Backend = FindBackendByName(BackendName))
	{
		Backend->HandleInterruption(Reason);
	}
	PublishCapabilities();
}

bool FOpenMobileHapticsBackendRegistry::RequestRecovery(
	bool bPolicyAllowsRecovery
)
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (!RecoveryPolicy.IsRecovering())
	{
		return true;
	}
	if (!bPolicyAllowsRecovery || RecoveryPolicy.IsExhausted()
		|| bShuttingDown.Load())
	{
		return false;
	}
	bRecoveryRequested = true;
	const double NowSeconds = FPlatformTime::Seconds();
	if (!RecoveryTickerHandle.IsValid()
		&& bApplicationActive.Load()
		&& NowSeconds + UE_DOUBLE_SMALL_NUMBER
			>= RecoveryPolicy.GetNextAttemptTimeSeconds())
	{
		RunRecoveryAttempt(NowSeconds);
		return !RecoveryPolicy.IsRecovering();
	}
	ScheduleRecoveryAttempt();
	return false;
}

bool FOpenMobileHapticsBackendRegistry::IsRecovering()
{
	return OpenMobileHapticsBackendRegistryPrivate::RecoveryPolicy.IsRecovering();
}

FOpenMobileHapticsInterruptionDelegate&
FOpenMobileHapticsBackendRegistry::OnInterruption()
{
	return OpenMobileHapticsBackendRegistryPrivate::InterruptionDelegate;
}

FOpenMobileHapticsRecoveryDelegate&
FOpenMobileHapticsBackendRegistry::OnRecovery()
{
	return OpenMobileHapticsBackendRegistryPrivate::RecoveryDelegate;
}

FOpenMobileHapticsApplicationLifecycleDelegate&
FOpenMobileHapticsBackendRegistry::OnApplicationLifecycle()
{
	return OpenMobileHapticsBackendRegistryPrivate::LifecycleDelegate;
}

bool FOpenMobileHapticsBackendRegistry::IsShuttingDown()
{
	return OpenMobileHapticsBackendRegistryPrivate::bShuttingDown.Load();
}

void FOpenMobileHapticsBackendRegistry::BeginShutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	if (bShuttingDown.Exchange(true))
	{
		return;
	}
	CancelRecoveryTicker();
	RecoveryPolicy.Reset();
	LifecyclePolicy.Apply(
		EOpenMobileHapticsLifecycleEvent::WillTerminate
	);
	bApplicationActive.Store(false);
	RecoveryBackendName = NAME_None;
	bRecoveryRequested = false;

	AdvanceGeneration();
	TimelineManager().Clear();
	for (IOpenMobileHapticsBackend* Backend : GetBackends())
	{
		if (Backend)
		{
			StopBackend(*Backend);
		}
	}
	PublishCapabilities();
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileHapticsBackendRegistry::RunRecoveryAttemptForTests(
	double NowSeconds
)
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	CancelRecoveryTicker();
	RunRecoveryAttempt(NowSeconds);
}

void FOpenMobileHapticsBackendRegistry::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileHapticsBackendRegistryPrivate;
	bShuttingDown.Store(false);
	bApplicationActive.Store(true);
	CancelRecoveryTicker();
	RecoveryPolicy.Reset();
	LifecyclePolicy.Reset();
	RecoveryBackendName = NAME_None;
	bRecoveryRequested = false;
	InterruptionDelegate.Clear();
	RecoveryDelegate.Clear();
	LifecycleDelegate.Clear();
	ShutdownBackends.Reset();
	AdvanceGeneration();
	AdvanceLifecycleGeneration();
	TimelineManager().Clear();
	TimelineManager().ResetStatistics();
	PublishCapabilities();
}
#endif
