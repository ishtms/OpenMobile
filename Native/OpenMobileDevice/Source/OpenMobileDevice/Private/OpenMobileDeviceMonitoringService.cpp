#include "OpenMobileDeviceMonitoringService.h"

#include "Containers/Ticker.h"
#include "IOpenMobileDeviceBackend.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileAsync.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceSettings.h"
#include "OpenMobileDeviceSnapshotService.h"

namespace OpenMobileDeviceMonitoringServicePrivate
{
	constexpr float MaintenanceIntervalSeconds = 0.1f;
	constexpr float NetworkDebounceSeconds = 0.25f;
	constexpr float WindowDebounceSeconds = 0.1f;

	struct FRequest
	{
		TArray<EOpenMobileDeviceMonitoringGroup> Groups;
		float PollingIntervalSeconds = 1.0f;
	};

	struct FGroupState
	{
		int32 ReferenceCount = 0;
		float EffectiveIntervalSeconds = 1.0f;
		float ElapsedSeconds = 0.0f;
		bool bUsesFallback = true;
		bool bNativeObserverStarted = false;
		IOpenMobileDeviceBackend* Backend = nullptr;
		FOpenMobileDeviceCallbackToken BackendToken;
		FOpenMobileDeviceMonitoringCallbackToken CallbackToken;
		uint64 LastNativeSequence = 0;
	};

	TMap<FGuid, FRequest> Requests;
	TMap<EOpenMobileDeviceMonitoringGroup, FGroupState> GroupStates;
	FOpenMobileDeviceMonitoringGroupChanged GroupChanged;
	FOpenMobileDeviceMonitoredNetworkPathChanged NetworkPathChanged;
	FOpenMobileDeviceMonitoringMaintenance Maintenance;
	FTSTicker::FDelegateHandle TickerHandle;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;
	FDelegateHandle SafeFrameChangedHandle;
	bool bStarted = false;
	bool bApplicationActive = true;
	bool bInsideTicker = false;
	uint64 ObserverGeneration = 0;
	TOptional<FOpenMobileNetworkPathSnapshot> LastNetworkSnapshot;
	TOptional<FOpenMobileNetworkPathSnapshot> PendingNetworkSnapshot;
	float NetworkDebounceElapsedSeconds = 0.0f;
	bool bWindowRefreshPending = false;
	float WindowDebounceElapsedSeconds = 0.0f;

	bool EquivalentNetworkWithoutMetadata(
		FOpenMobileNetworkPathSnapshot Left,
		FOpenMobileNetworkPathSnapshot Right
	)
	{
		Left.Metadata = {};
		Right.Metadata = {};
		return Left == Right;
	}

	bool IsUnavailable(const FOpenMobileNetworkPathSnapshot& Snapshot)
	{
		return Snapshot.PathState == EOpenMobileNetworkPathState::Unavailable;
	}

	void ResetNetworkState()
	{
		LastNetworkSnapshot.Reset();
		PendingNetworkSnapshot.Reset();
		NetworkDebounceElapsedSeconds = 0.0f;
	}

	void ResetWindowState()
	{
		bWindowRefreshPending = false;
		WindowDebounceElapsedSeconds = 0.0f;
	}

	void ScheduleWindowRefresh()
	{
		bWindowRefreshPending = true;
		WindowDebounceElapsedSeconds = 0.0f;
	}

	void PrimeNetworkState()
	{
		LastNetworkSnapshot =
			FOpenMobileDeviceSnapshotService::GetNetworkPathSnapshot();
		PendingNetworkSnapshot.Reset();
		NetworkDebounceElapsedSeconds = 0.0f;
	}

	void PublishNetworkSnapshot(
		const FOpenMobileNetworkPathSnapshot& Snapshot
	)
	{
		LastNetworkSnapshot = Snapshot;
		PendingNetworkSnapshot.Reset();
		NetworkDebounceElapsedSeconds = 0.0f;
		NetworkPathChanged.Broadcast(Snapshot);
	}

	void RefreshNetworkPath()
	{
		const FOpenMobileNetworkPathSnapshot Snapshot =
			FOpenMobileDeviceSnapshotService::GetNetworkPathSnapshot();
		if (!LastNetworkSnapshot.IsSet())
		{
			PublishNetworkSnapshot(Snapshot);
			return;
		}
		if (EquivalentNetworkWithoutMetadata(
			LastNetworkSnapshot.GetValue(),
			Snapshot
		))
		{
			PendingNetworkSnapshot.Reset();
			NetworkDebounceElapsedSeconds = 0.0f;
			return;
		}
		if (IsUnavailable(LastNetworkSnapshot.GetValue())
			!= IsUnavailable(Snapshot))
		{
			PublishNetworkSnapshot(Snapshot);
			return;
		}
		PendingNetworkSnapshot = Snapshot;
		NetworkDebounceElapsedSeconds = 0.0f;
	}

	void RefreshGroup(
		EOpenMobileDeviceMonitoringGroup Group,
		bool bDebounceWindow = false
	)
	{
		if (Group == EOpenMobileDeviceMonitoringGroup::Network)
		{
			RefreshNetworkPath();
		}
		else if (Group == EOpenMobileDeviceMonitoringGroup::WindowDisplay
			&& bDebounceWindow)
		{
			ScheduleWindowRefresh();
		}
		else
		{
			GroupChanged.Broadcast(Group);
		}
	}

	void ProcessWindowDebounce(float DeltaTime)
	{
		if (!bWindowRefreshPending)
		{
			return;
		}
		WindowDebounceElapsedSeconds += FMath::Max(0.0f, DeltaTime);
		if (WindowDebounceElapsedSeconds >= WindowDebounceSeconds)
		{
			ResetWindowState();
			GroupChanged.Broadcast(
				EOpenMobileDeviceMonitoringGroup::WindowDisplay
			);
		}
	}

	void ProcessNetworkDebounce(float DeltaTime)
	{
		if (!PendingNetworkSnapshot.IsSet())
		{
			return;
		}
		NetworkDebounceElapsedSeconds += FMath::Max(0.0f, DeltaTime);
		if (NetworkDebounceElapsedSeconds >= NetworkDebounceSeconds)
		{
			const FOpenMobileNetworkPathSnapshot Snapshot =
				PendingNetworkSnapshot.GetValue();
			PublishNetworkSnapshot(Snapshot);
		}
	}

	float ClampInterval(float IntervalSeconds)
	{
		const UOpenMobileDeviceSettings* Settings =
			GetDefault<UOpenMobileDeviceSettings>();
		const float ResolvedInterval = !FMath::IsFinite(IntervalSeconds)
			|| IntervalSeconds == 0.0f
			? Settings->GetValidatedFallbackPollingIntervalSeconds()
			: IntervalSeconds;
		return FMath::Clamp(
			ResolvedInterval,
			UOpenMobileDeviceSettings::GetMinimumFallbackPollingIntervalSeconds(),
			UOpenMobileDeviceSettings::GetMaximumFallbackPollingIntervalSeconds()
		);
	}

	float ApplyGroupIntervalBounds(
		EOpenMobileDeviceMonitoringGroup Group,
		float IntervalSeconds
	)
	{
		if (Group != EOpenMobileDeviceMonitoringGroup::Storage)
		{
			return IntervalSeconds;
		}
		return FMath::Max(
			IntervalSeconds,
			GetDefault<UOpenMobileDeviceSettings>()
				->GetValidatedLowStorageFallbackPollingIntervalSeconds()
		);
	}

	bool IsValidGroup(EOpenMobileDeviceMonitoringGroup Group)
	{
		switch (Group)
		{
		case EOpenMobileDeviceMonitoringGroup::Locale:
		case EOpenMobileDeviceMonitoringGroup::Power:
		case EOpenMobileDeviceMonitoringGroup::MemoryPressure:
		case EOpenMobileDeviceMonitoringGroup::Storage:
		case EOpenMobileDeviceMonitoringGroup::Network:
		case EOpenMobileDeviceMonitoringGroup::WindowDisplay:
		case EOpenMobileDeviceMonitoringGroup::Appearance:
		case EOpenMobileDeviceMonitoringGroup::Accessibility:
		case EOpenMobileDeviceMonitoringGroup::MediaVolume:
		case EOpenMobileDeviceMonitoringGroup::Flashlight:
			return true;
		}
		return false;
	}

	uint64 NextObserverGeneration()
	{
		if (ObserverGeneration == MAX_uint64)
		{
			ObserverGeneration = 1;
		}
		else
		{
			++ObserverGeneration;
		}
		return ObserverGeneration;
	}

	void StopNativeObserver(
		EOpenMobileDeviceMonitoringGroup Group,
		FGroupState& State
	)
	{
		if (!State.bNativeObserverStarted)
		{
			return;
		}

		IOpenMobileDeviceBackend* BackendToStop = State.Backend;
		State.bNativeObserverStarted = false;
		State.Backend = nullptr;
		State.CallbackToken = {};
		State.LastNativeSequence = 0;
		if (FOpenMobileDeviceBackendRegistry::IsBackendRegistered(BackendToStop))
		{
			BackendToStop->StopMonitoring(Group);
		}
	}

	void ConfigureSource(
		EOpenMobileDeviceMonitoringGroup Group,
		FGroupState& State
	)
	{
		IOpenMobileDeviceBackend* Backend =
			FOpenMobileDeviceBackendRegistry::FindBackend();
		State.Backend = Backend;
		State.BackendToken = Backend
			? FOpenMobileDeviceBackendRegistry::CaptureCallbackToken()
			: FOpenMobileDeviceCallbackToken();
		State.CallbackToken = {};
		State.CallbackToken.Group = Group;
		if (Backend)
		{
			State.CallbackToken.BackendToken = State.BackendToken;
			State.CallbackToken.ObserverGeneration = NextObserverGeneration();
		}
		State.LastNativeSequence = 0;
		State.bNativeObserverStarted = Backend && Backend->StartMonitoring(
			Group,
			State.CallbackToken
		);
		State.bUsesFallback = !State.bNativeObserverStarted
			|| Backend->RequiresFallbackPolling(Group);
		if (!State.bNativeObserverStarted)
		{
			State.CallbackToken = {};
		}
		State.ElapsedSeconds = 0.0f;
	}

	void RecalculateInterval(EOpenMobileDeviceMonitoringGroup Group)
	{
		FGroupState* State = GroupStates.Find(Group);
		if (!State)
		{
			return;
		}

		float Interval =
			UOpenMobileDeviceSettings::GetMaximumFallbackPollingIntervalSeconds();
		for (const TPair<FGuid, FRequest>& Pair : Requests)
		{
			if (Pair.Value.Groups.Contains(Group))
			{
				Interval = FMath::Min(Interval, Pair.Value.PollingIntervalSeconds);
			}
		}
		State->EffectiveIntervalSeconds = ApplyGroupIntervalBounds(
			Group,
			Interval
		);
		State->ElapsedSeconds = FMath::Min(
			State->ElapsedSeconds,
			State->EffectiveIntervalSeconds
		);
	}

	void StopTicker()
	{
		if (!TickerHandle.IsValid())
		{
			return;
		}
		if (!bInsideTicker)
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		}
		TickerHandle.Reset();
	}

	void ProcessTick(float DeltaTime)
	{
		check(IsInGameThread());
		Maintenance.Broadcast();
		if (Requests.IsEmpty() || !bApplicationActive)
		{
			return;
		}
		ProcessNetworkDebounce(DeltaTime);
		ProcessWindowDebounce(DeltaTime);

		TArray<EOpenMobileDeviceMonitoringGroup> Groups;
		GroupStates.GenerateKeyArray(Groups);
		for (EOpenMobileDeviceMonitoringGroup Group : Groups)
		{
			FGroupState* State = GroupStates.Find(Group);
			if (!State || State->ReferenceCount <= 0)
			{
				continue;
			}

			if (!FOpenMobileDeviceBackendRegistry::IsCallbackCurrent(
				State->BackendToken
			))
			{
				StopNativeObserver(Group, *State);
				ConfigureSource(Group, *State);
				if (Group == EOpenMobileDeviceMonitoringGroup::Network)
				{
					PrimeNetworkState();
				}
			}
			if (!State->bUsesFallback)
			{
				continue;
			}

			State->ElapsedSeconds += FMath::Max(0.0f, DeltaTime);
			if (State->ElapsedSeconds >= State->EffectiveIntervalSeconds)
			{
				State->ElapsedSeconds = 0.0f;
				RefreshGroup(Group);
			}
		}
	}

	bool TickService(float DeltaTime)
	{
		bInsideTicker = true;
		ProcessTick(DeltaTime);
		bInsideTicker = false;
		const bool bContinue = !Requests.IsEmpty();
		if (!bContinue)
		{
			TickerHandle.Reset();
		}
		return bContinue;
	}

	void EnsureTicker()
	{
		if (Requests.IsEmpty() || TickerHandle.IsValid())
		{
			return;
		}
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateStatic(&TickService),
			MaintenanceIntervalSeconds
		);
	}

	void SetApplicationActive(bool bActive)
	{
		check(IsInGameThread());
		if (bApplicationActive == bActive)
		{
			return;
		}
		bApplicationActive = bActive;
		if (!bApplicationActive)
		{
			ResetWindowState();
			return;
		}

		TArray<EOpenMobileDeviceMonitoringGroup> Groups;
		GroupStates.GenerateKeyArray(Groups);
		for (EOpenMobileDeviceMonitoringGroup Group : Groups)
		{
			if (FGroupState* State = GroupStates.Find(Group))
			{
				State->ElapsedSeconds = 0.0f;
				RefreshGroup(Group);
			}
		}
	}

	void HandleApplicationWillEnterBackground()
	{
		SetApplicationActive(false);
	}

	void HandleApplicationHasEnteredForeground()
	{
		SetApplicationActive(true);
	}

	void HandleSafeFrameChanged()
	{
		check(IsInGameThread());
		if (bApplicationActive
			&& GroupStates.Contains(
				EOpenMobileDeviceMonitoringGroup::WindowDisplay
			))
		{
			RefreshGroup(
				EOpenMobileDeviceMonitoringGroup::WindowDisplay,
				true
			);
		}
	}

	void ReleaseAllRequests()
	{
		for (TPair<EOpenMobileDeviceMonitoringGroup, FGroupState>& Pair : GroupStates)
		{
			StopNativeObserver(Pair.Key, Pair.Value);
		}
		Requests.Reset();
		GroupStates.Reset();
		ResetNetworkState();
		ResetWindowState();
		StopTicker();
	}
}

void FOpenMobileDeviceMonitoringService::Start()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	if (bStarted)
	{
		return;
	}
	bStarted = true;
	bApplicationActive = true;
	BackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddStatic(
		&HandleApplicationWillEnterBackground
	);
	ForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddStatic(
		&HandleApplicationHasEnteredForeground
	);
	SafeFrameChangedHandle = FCoreDelegates::OnSafeFrameChangedEvent.AddStatic(
		&HandleSafeFrameChanged
	);
}

void FOpenMobileDeviceMonitoringService::Shutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	ReleaseAllRequests();
	if (BackgroundHandle.IsValid())
	{
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(BackgroundHandle);
		BackgroundHandle.Reset();
	}
	if (ForegroundHandle.IsValid())
	{
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ForegroundHandle);
		ForegroundHandle.Reset();
	}
	if (SafeFrameChangedHandle.IsValid())
	{
		FCoreDelegates::OnSafeFrameChangedEvent.Remove(SafeFrameChangedHandle);
		SafeFrameChangedHandle.Reset();
	}
	GroupChanged.Clear();
	NetworkPathChanged.Clear();
	Maintenance.Clear();
	bStarted = false;
}

FGuid FOpenMobileDeviceMonitoringService::AddSubscription(
	const TArray<EOpenMobileDeviceMonitoringGroup>& Groups,
	float FallbackPollingIntervalSeconds
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	if (FOpenMobileDeviceBackendRegistry::IsShuttingDown())
	{
		return {};
	}
	Start();

	FRequest Request;
	for (EOpenMobileDeviceMonitoringGroup Group : Groups)
	{
		if (IsValidGroup(Group))
		{
			Request.Groups.AddUnique(Group);
		}
	}
	if (Request.Groups.IsEmpty())
	{
		return {};
	}
	Request.PollingIntervalSeconds = ClampInterval(FallbackPollingIntervalSeconds);
	const FGuid RequestId = FGuid::NewGuid();
	Requests.Add(RequestId, Request);

	for (EOpenMobileDeviceMonitoringGroup Group : Request.Groups)
	{
		FGroupState& State = GroupStates.FindOrAdd(Group);
		++State.ReferenceCount;
		if (State.ReferenceCount == 1)
		{
			State.EffectiveIntervalSeconds = ApplyGroupIntervalBounds(
				Group,
				Request.PollingIntervalSeconds
			);
			ConfigureSource(Group, State);
			if (Group == EOpenMobileDeviceMonitoringGroup::Network)
			{
				PrimeNetworkState();
			}
		}
		else
		{
			RecalculateInterval(Group);
		}
	}
	EnsureTicker();
	return RequestId;
}

void FOpenMobileDeviceMonitoringService::RemoveSubscription(const FGuid& RequestId)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	FRequest Request;
	if (!Requests.RemoveAndCopyValue(RequestId, Request))
	{
		return;
	}

	for (EOpenMobileDeviceMonitoringGroup Group : Request.Groups)
	{
		FGroupState* State = GroupStates.Find(Group);
		if (!State)
		{
			continue;
		}
		--State->ReferenceCount;
		if (State->ReferenceCount <= 0)
		{
			StopNativeObserver(Group, *State);
			GroupStates.Remove(Group);
			if (Group == EOpenMobileDeviceMonitoringGroup::Network)
			{
				ResetNetworkState();
			}
			else if (Group == EOpenMobileDeviceMonitoringGroup::WindowDisplay)
			{
				ResetWindowState();
			}
		}
		else
		{
			RecalculateInterval(Group);
		}
	}
	if (Requests.IsEmpty())
	{
		StopTicker();
	}
}

void FOpenMobileDeviceMonitoringService::NotifyNativeChange(
	const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken,
	uint64 SourceSequence
)
{
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	if (!CallbackToken.IsValid() || SourceSequence == 0)
	{
		return;
	}
	if (!IsInGameThread())
	{
		OpenMobile::DispatchToGameThread([CallbackToken, SourceSequence]()
		{
			FOpenMobileDeviceMonitoringService::NotifyNativeChange(
				CallbackToken,
				SourceSequence
			);
		});
		return;
	}
	FGroupState* State = GroupStates.Find(CallbackToken.Group);
	if (!bApplicationActive
		|| !State
		|| State->CallbackToken != CallbackToken
		|| !FOpenMobileDeviceBackendRegistry::IsCallbackCurrent(
			CallbackToken.BackendToken
		)
		|| SourceSequence <= State->LastNativeSequence)
	{
		return;
	}
	State->LastNativeSequence = SourceSequence;
	RefreshGroup(
		CallbackToken.Group,
		CallbackToken.Group == EOpenMobileDeviceMonitoringGroup::WindowDisplay
	);
}

void FOpenMobileDeviceMonitoringService::NotifyWindowSettled()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	if (bApplicationActive
		&& GroupStates.Contains(EOpenMobileDeviceMonitoringGroup::WindowDisplay))
	{
		RefreshGroup(EOpenMobileDeviceMonitoringGroup::WindowDisplay, true);
	}
}

void FOpenMobileDeviceMonitoringService::RefreshActiveGroups()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	if (!bApplicationActive)
	{
		return;
	}
	TArray<EOpenMobileDeviceMonitoringGroup> Groups;
	GroupStates.GenerateKeyArray(Groups);
	for (EOpenMobileDeviceMonitoringGroup Group : Groups)
	{
		FGroupState* State = GroupStates.Find(Group);
		if (!State || State->ReferenceCount <= 0)
		{
			continue;
		}
		State->ElapsedSeconds = 0.0f;
		RefreshGroup(Group);
	}
}

FOpenMobileDeviceMonitoringGroupChanged&
FOpenMobileDeviceMonitoringService::OnGroupChanged()
{
	return OpenMobileDeviceMonitoringServicePrivate::GroupChanged;
}

FOpenMobileDeviceMonitoredNetworkPathChanged&
FOpenMobileDeviceMonitoringService::OnNetworkPathChanged()
{
	return OpenMobileDeviceMonitoringServicePrivate::NetworkPathChanged;
}

FOpenMobileDeviceMonitoringMaintenance&
FOpenMobileDeviceMonitoringService::OnMaintenance()
{
	return OpenMobileDeviceMonitoringServicePrivate::Maintenance;
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileDeviceMonitoringService::ResetForTests()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	ReleaseAllRequests();
	bApplicationActive = true;
}

int32 FOpenMobileDeviceMonitoringService::GetReferenceCountForTests(
	EOpenMobileDeviceMonitoringGroup Group
)
{
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	const FGroupState* State = GroupStates.Find(Group);
	return State ? State->ReferenceCount : 0;
}

bool FOpenMobileDeviceMonitoringService::IsTickerActiveForTests()
{
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	return !Requests.IsEmpty() && (TickerHandle.IsValid() || bInsideTicker);
}

bool FOpenMobileDeviceMonitoringService::UsesFallbackForTests(
	EOpenMobileDeviceMonitoringGroup Group
)
{
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	const FGroupState* State = GroupStates.Find(Group);
	return State && State->bUsesFallback;
}

float FOpenMobileDeviceMonitoringService::GetEffectiveIntervalForTests(
	EOpenMobileDeviceMonitoringGroup Group
)
{
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	const FGroupState* State = GroupStates.Find(Group);
	return State ? State->EffectiveIntervalSeconds : 0.0f;
}

uint64 FOpenMobileDeviceMonitoringService::GetLastNativeSequenceForTests(
	EOpenMobileDeviceMonitoringGroup Group
)
{
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	const FGroupState* State = GroupStates.Find(Group);
	return State ? State->LastNativeSequence : 0;
}

void FOpenMobileDeviceMonitoringService::NotifyNativeChangeForTests(
	EOpenMobileDeviceMonitoringGroup Group
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	if (const FGroupState* State = GroupStates.Find(Group);
		State && State->CallbackToken.IsValid())
	{
		NotifyNativeChange(State->CallbackToken, State->LastNativeSequence + 1);
	}
}

void FOpenMobileDeviceMonitoringService::TickForTests(float DeltaTime)
{
	OpenMobileDeviceMonitoringServicePrivate::ProcessTick(DeltaTime);
}

void FOpenMobileDeviceMonitoringService::SetApplicationActiveForTests(bool bActive)
{
	OpenMobileDeviceMonitoringServicePrivate::SetApplicationActive(bActive);
}
#endif
