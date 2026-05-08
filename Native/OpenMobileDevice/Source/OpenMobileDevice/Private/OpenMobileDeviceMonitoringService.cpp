#include "OpenMobileDeviceMonitoringService.h"

#include "Containers/Ticker.h"
#include "IOpenMobileDeviceBackend.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileAsync.h"
#include "OpenMobileDeviceBackendRegistry.h"

namespace OpenMobileDeviceMonitoringServicePrivate
{
	constexpr float MinimumPollingIntervalSeconds = 0.1f;
	constexpr float MaximumPollingIntervalSeconds = 60.0f;
	constexpr float MaintenanceIntervalSeconds = 0.1f;

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
	};

	TMap<FGuid, FRequest> Requests;
	TMap<EOpenMobileDeviceMonitoringGroup, FGroupState> GroupStates;
	FOpenMobileDeviceMonitoringGroupChanged GroupChanged;
	FOpenMobileDeviceMonitoringMaintenance Maintenance;
	FTSTicker::FDelegateHandle TickerHandle;
	FDelegateHandle BackgroundHandle;
	FDelegateHandle ForegroundHandle;
	bool bStarted = false;
	bool bApplicationActive = true;
	bool bInsideTicker = false;

	float ClampInterval(float IntervalSeconds)
	{
		return FMath::Clamp(
			FMath::IsFinite(IntervalSeconds) ? IntervalSeconds : 1.0f,
			MinimumPollingIntervalSeconds,
			MaximumPollingIntervalSeconds
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
			return true;
		}
		return false;
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

		if (IOpenMobileDeviceBackend* CurrentBackend =
			FOpenMobileDeviceBackendRegistry::FindBackend())
		{
			if (CurrentBackend == State.Backend)
			{
				CurrentBackend->StopMonitoring(Group);
			}
		}
		State.bNativeObserverStarted = false;
		State.Backend = nullptr;
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
		State.bNativeObserverStarted = Backend && Backend->StartMonitoring(Group);
		State.bUsesFallback = !State.bNativeObserverStarted;
		State.ElapsedSeconds = 0.0f;
	}

	void RecalculateInterval(EOpenMobileDeviceMonitoringGroup Group)
	{
		FGroupState* State = GroupStates.Find(Group);
		if (!State)
		{
			return;
		}

		float Interval = MaximumPollingIntervalSeconds;
		for (const TPair<FGuid, FRequest>& Pair : Requests)
		{
			if (Pair.Value.Groups.Contains(Group))
			{
				Interval = FMath::Min(Interval, Pair.Value.PollingIntervalSeconds);
			}
		}
		State->EffectiveIntervalSeconds = Interval;
		State->ElapsedSeconds = FMath::Min(State->ElapsedSeconds, Interval);
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
			}
			if (!State->bUsesFallback)
			{
				continue;
			}

			State->ElapsedSeconds += FMath::Max(0.0f, DeltaTime);
			if (State->ElapsedSeconds >= State->EffectiveIntervalSeconds)
			{
				State->ElapsedSeconds = 0.0f;
				GroupChanged.Broadcast(Group);
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
			return;
		}

		TArray<EOpenMobileDeviceMonitoringGroup> Groups;
		GroupStates.GenerateKeyArray(Groups);
		for (EOpenMobileDeviceMonitoringGroup Group : Groups)
		{
			if (FGroupState* State = GroupStates.Find(Group))
			{
				State->ElapsedSeconds = 0.0f;
				GroupChanged.Broadcast(Group);
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

	void ReleaseAllRequests()
	{
		for (TPair<EOpenMobileDeviceMonitoringGroup, FGroupState>& Pair : GroupStates)
		{
			StopNativeObserver(Pair.Key, Pair.Value);
		}
		Requests.Reset();
		GroupStates.Reset();
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
	GroupChanged.Clear();
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
			State.EffectiveIntervalSeconds = Request.PollingIntervalSeconds;
			ConfigureSource(Group, State);
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
	EOpenMobileDeviceMonitoringGroup Group
)
{
	using namespace OpenMobileDeviceMonitoringServicePrivate;
	if (!IsInGameThread())
	{
		OpenMobile::DispatchToGameThread([Group]()
		{
			FOpenMobileDeviceMonitoringService::NotifyNativeChange(Group);
		});
		return;
	}
	if (bApplicationActive && GroupStates.Contains(Group))
	{
		GroupChanged.Broadcast(Group);
	}
}

FOpenMobileDeviceMonitoringGroupChanged&
FOpenMobileDeviceMonitoringService::OnGroupChanged()
{
	return OpenMobileDeviceMonitoringServicePrivate::GroupChanged;
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

void FOpenMobileDeviceMonitoringService::TickForTests(float DeltaTime)
{
	OpenMobileDeviceMonitoringServicePrivate::ProcessTick(DeltaTime);
}

void FOpenMobileDeviceMonitoringService::SetApplicationActiveForTests(bool bActive)
{
	OpenMobileDeviceMonitoringServicePrivate::SetApplicationActive(bActive);
}
#endif
