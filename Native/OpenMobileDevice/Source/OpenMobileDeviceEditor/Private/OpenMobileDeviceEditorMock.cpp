#include "OpenMobileDeviceEditorMock.h"

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "IOpenMobileDeviceBackend.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceMockSettings.h"
#include "OpenMobileDeviceMonitoringCallback.h"
#include "OpenMobileDeviceMonitoringService.h"

namespace OpenMobileDeviceEditorMockPrivate
{
	constexpr int32 MockPriority = 100000;

	FOpenMobileDeviceMockState MakeStateFromSettings()
	{
		const UOpenMobileDeviceMockSettings* Settings =
			GetDefault<UOpenMobileDeviceMockSettings>();
		FOpenMobileDeviceMockState State;
		const float BatteryPercent = FMath::Clamp(
			Settings->BatteryPercent,
			0.0f,
			100.0f
		);
		State.Power.BatteryPercent =
			FOpenMobileDeviceOptionalFloat::MakeAvailable(BatteryPercent);
		State.Power.NativeBatteryLevel =
			FOpenMobileDeviceOptionalFloat::MakeAvailable(BatteryPercent / 100.0f);
		State.Power.ChargingState = Settings->ChargingState;
		State.Power.ChargingSource = Settings->ChargingSource;
		State.Power.ThermalState = Settings->ThermalState;

		State.Memory.TotalPhysicalBytes =
			FOpenMobileDeviceOptionalInt64::MakeAvailable(
				FMath::Max<int64>(0, Settings->TotalPhysicalBytes)
			);
		State.Memory.AvailablePhysicalBytes =
			FOpenMobileDeviceOptionalInt64::MakeAvailable(
				FMath::Max<int64>(0, Settings->AvailablePhysicalBytes)
			);
		State.Memory.PressureState = Settings->MemoryPressureState;
		State.Memory.LatestPressureEventState = Settings->MemoryPressureState;

		State.Storage.Scope = EOpenMobileStorageScope::ApplicationContainer;
		State.Storage.TotalBytes = FOpenMobileDeviceOptionalInt64::MakeAvailable(
			FMath::Max<int64>(0, Settings->TotalStorageBytes)
		);
		State.Storage.AvailableBytes =
			FOpenMobileDeviceOptionalInt64::MakeAvailable(
				FMath::Max<int64>(0, Settings->AvailableStorageBytes)
			);
		State.Storage.bIsLowStorage =
			FOpenMobileDeviceOptionalBool::MakeAvailable(Settings->bLowStorage);

		State.Network.PathState = Settings->NetworkPathState;
		State.Network.ValidationSource =
			EOpenMobileNetworkValidationSource::OsValidatedPath;
		State.Network.bTransportsAvailable = true;
		State.Network.Transports.Add(Settings->DefaultNetworkTransport);
		State.Network.bDefaultTransportAvailable = true;
		State.Network.DefaultTransport = Settings->DefaultNetworkTransport;
		State.Network.bIsMetered =
			FOpenMobileDeviceOptionalBool::MakeAvailable(Settings->bNetworkMetered);

		State.Window.bDisplayCutoutsAvailable = true;
		for (const FVector4& Cutout : Settings->DisplayCutouts)
		{
			FOpenMobileDeviceRect Rect;
			Rect.Left = Cutout.X;
			Rect.Top = Cutout.Y;
			Rect.Right = Cutout.Z;
			Rect.Bottom = Cutout.W;
			State.Window.DisplayCutouts.Add(Rect);
		}
		State.Window.FoldablePosture = Settings->FoldablePosture;
		State.Appearance.Appearance = Settings->Appearance;

		State.Accessibility.PreferredTextScale =
			FOpenMobileDeviceOptionalFloat::MakeAvailable(
				FMath::Max(0.1f, Settings->PreferredTextScale)
			);
		State.Accessibility.bReducedAnimationPreferred =
			FOpenMobileDeviceOptionalBool::MakeAvailable(
				Settings->bReducedAnimationPreferred
			);
		State.Accessibility.bScreenReaderActive =
			FOpenMobileDeviceOptionalBool::MakeAvailable(
				Settings->bScreenReaderActive
			);
		State.Accessibility.bTouchExplorationActive =
			FOpenMobileDeviceOptionalBool::MakeAvailable(
				Settings->bTouchExplorationActive
			);
		if (Settings->bInjectFailure && !Settings->FailedCapability.IsNone())
		{
			State.Failures.Add(
				Settings->FailedCapability,
				FOpenMobileError::Make(
					Settings->FailureCode,
					Settings->FailureMessage
				)
			);
		}
		return State;
	}

	class FMockBackend final : public IOpenMobileDeviceBackend
	{
	public:
		virtual FName GetBackendName() const override
		{
			return TEXT("OpenMobileDeviceEditorMock");
		}

		virtual int32 GetPriority() const override { return MockPriority; }
		virtual bool IsAvailable() const override { return !bShuttingDown; }

		virtual FOpenMobileCapability GetDomainCapability(
			EOpenMobileDeviceBackendDomain Domain
		) const override
		{
			FOpenMobileCapability Capability;
			Capability.Name = GetDomainCapabilityName(Domain);
			Capability.State = EOpenMobileCapabilityState::Available;
			return Capability;
		}

		virtual FOpenMobileDeviceCapability GetCapability(
			FName CapabilityName
		) const override
		{
			FOpenMobileDeviceCapability Capability;
			Capability.Name = CapabilityName;
			Capability.BackendName = GetBackendName();
			if (const FOpenMobileError* Failure = State.Failures.Find(CapabilityName))
			{
				Capability.State =
					EOpenMobileCapabilityState::TemporarilyUnavailable;
				Capability.Detail = Failure->Message;
			}
			else
			{
				Capability.State = EOpenMobileCapabilityState::Available;
			}
			return Capability;
		}

		virtual FOpenMobilePowerSnapshot GetPowerSnapshot() const override
		{
			return State.Power;
		}

		virtual FOpenMobileMemorySnapshot GetMemorySnapshot() const override
		{
			return State.Memory;
		}

		virtual FOpenMobileStorageSnapshot GetStorageSnapshot() const override
		{
			return State.Storage;
		}

		virtual bool QueryStorageSnapshot(
			FOpenMobileStorageSnapshot& OutSnapshot,
			FOpenMobileError& OutError
		) const override
		{
			if (const FOpenMobileError* Failure = State.Failures.Find(
				FOpenMobileDeviceCapabilityNames::StorageSpace
			))
			{
				OutSnapshot = {};
				OutError = *Failure;
				return false;
			}
			OutSnapshot = State.Storage;
			OutError = {};
			return true;
		}

		virtual FOpenMobileNetworkPathSnapshot GetNetworkPathSnapshot() const override
		{
			return State.Network;
		}

		virtual FOpenMobileWindowDisplaySnapshot GetWindowDisplaySnapshot() const override
		{
			return State.Window;
		}

		virtual FOpenMobileAppearanceSnapshot GetAppearanceSnapshot() const override
		{
			return State.Appearance;
		}

		virtual FOpenMobileAccessibilitySnapshot GetAccessibilitySnapshot() const override
		{
			return State.Accessibility;
		}

		virtual bool StartMonitoring(
			EOpenMobileDeviceMonitoringGroup Group,
			const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
		) override
		{
			if (bShuttingDown || !CallbackToken.IsValid())
			{
				return false;
			}
			Callbacks.Add(Group, CallbackToken);
			SourceSequences.Add(Group, 0);
			return true;
		}

		virtual void StopMonitoring(
			EOpenMobileDeviceMonitoringGroup Group
		) override
		{
			Callbacks.Remove(Group);
			SourceSequences.Remove(Group);
		}

		virtual void BeginShutdown() override
		{
			bShuttingDown = true;
			Callbacks.Reset();
			SourceSequences.Reset();
		}

		void PrepareForRegistration()
		{
			bShuttingDown = false;
		}

		void SetState(const FOpenMobileDeviceMockState& InState)
		{
			State = InState;
		}

		const FOpenMobileDeviceMockState& GetState() const { return State; }

		const FOpenMobileDeviceMonitoringCallbackToken* FindCallback(
			EOpenMobileDeviceMonitoringGroup Group
		) const
		{
			return Callbacks.Find(Group);
		}

		uint64 NextSequence(EOpenMobileDeviceMonitoringGroup Group)
		{
			uint64& Sequence = SourceSequences.FindOrAdd(Group);
			if (Sequence >= MAX_uint64 - 2)
			{
				Sequence = 0;
			}
			return ++Sequence;
		}

		void AdvanceSequence(
			EOpenMobileDeviceMonitoringGroup Group,
			uint64 Sequence
		)
		{
			SourceSequences.FindOrAdd(Group) = Sequence;
		}

	private:
		FOpenMobileDeviceMockState State;
		TMap<
			EOpenMobileDeviceMonitoringGroup,
			FOpenMobileDeviceMonitoringCallbackToken
		> Callbacks;
		TMap<EOpenMobileDeviceMonitoringGroup, uint64> SourceSequences;
		bool bShuttingDown = false;
	};

	struct FDelayedEvent
	{
		FOpenMobileDeviceMockScriptStep Step;
		FOpenMobileDeviceMonitoringCallbackToken CallbackToken;
		uint64 SourceSequence = 0;
		float RemainingSeconds = 0.0f;
		uint64 EventGeneration = 0;
	};

	class FController final
	{
	public:
		void Startup()
		{
			if (bStarted)
			{
				return;
			}
			bStarted = true;
			Backend.SetState(MakeStateFromSettings());
			SetEnabled(GetDefault<UOpenMobileDeviceMockSettings>()->bEnableMockBackend);
			UpdateVisibleState();
		}

		void Shutdown()
		{
			if (!bStarted)
			{
				return;
			}
			CancelPendingEvents();
			if (FOpenMobileDeviceBackendRegistry::IsBackendRegistered(&Backend))
			{
				FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
			}
			bStarted = false;
		}

		void ApplySettings()
		{
			const UOpenMobileDeviceMockSettings* Settings =
				GetDefault<UOpenMobileDeviceMockSettings>();
			Backend.SetState(MakeStateFromSettings());
			SetEnabled(Settings->bEnableMockBackend);
			UpdateVisibleState();
		}

		void SetEnabled(bool bEnabled)
		{
			UOpenMobileDeviceMockSettings* Settings =
				GetMutableDefault<UOpenMobileDeviceMockSettings>();
			Settings->bEnableMockBackend = bEnabled;
			const bool bRegistered =
				FOpenMobileDeviceBackendRegistry::IsBackendRegistered(&Backend);
			if (bEnabled && !bRegistered)
			{
				Backend.PrepareForRegistration();
				FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend);
			}
			else if (!bEnabled && bRegistered)
			{
				CancelPendingEvents();
				FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
			}
			UpdateVisibleState();
		}

		bool IsEnabled() const
		{
			return FOpenMobileDeviceBackendRegistry::IsBackendRegistered(&Backend);
		}

		bool IsSelected() const
		{
			return IsEnabled()
				&& FOpenMobileDeviceBackendRegistry::FindBackend() == &Backend;
		}

		void SetState(const FOpenMobileDeviceMockState& State)
		{
			Backend.SetState(State);
			UpdateVisibleState();
		}

		const FOpenMobileDeviceMockState& GetState() const
		{
			return Backend.GetState();
		}

		void QueueScriptStep(const FOpenMobileDeviceMockScriptStep& Step)
		{
			ScriptSteps.Add(Step);
			UpdateVisibleState();
		}

		bool RunNextScriptStep()
		{
			if (ScriptSteps.IsEmpty())
			{
				return false;
			}
			const FOpenMobileDeviceMockScriptStep Step = ScriptSteps[0];
			ScriptSteps.RemoveAt(0);
			Dispatch(Step);
			UpdateVisibleState();
			return true;
		}

		int32 GetQueuedScriptStepCount() const { return ScriptSteps.Num(); }

		void ResetOverrides()
		{
			CancelPendingEvents();
			ScriptSteps.Reset();
			Backend.SetState(MakeStateFromSettings());
			UpdateVisibleState();
		}

		bool Tick(float DeltaTime)
		{
			for (FDelayedEvent& Event : DelayedEvents)
			{
				Event.RemainingSeconds -= FMath::Max(0.0f, DeltaTime);
			}
			for (int32 Index = 0; Index < DelayedEvents.Num();)
			{
				if (DelayedEvents[Index].RemainingSeconds > 0.0f)
				{
					++Index;
					continue;
				}
				const FDelayedEvent Event = DelayedEvents[Index];
				DelayedEvents.RemoveAt(Index);
				if (Event.EventGeneration == EventGeneration->Load())
				{
					Backend.SetState(Event.Step.State);
					FOpenMobileDeviceMonitoringService::NotifyNativeChange(
						Event.CallbackToken,
						Event.SourceSequence
					);
				}
			}
			if (DelayedEvents.IsEmpty())
			{
				TickerHandle.Reset();
			}
			UpdateVisibleState();
			return !DelayedEvents.IsEmpty();
		}

#if WITH_DEV_AUTOMATION_TESTS
		void TickForTests(float DeltaTime)
		{
			if (TickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
				TickerHandle.Reset();
			}
			Tick(DeltaTime);
			EnsureTicker();
		}

		void FlushBackgroundEventsForTests()
		{
			const double Deadline = FPlatformTime::Seconds() + 5.0;
			while (PendingBackgroundEvents->Load() > 0
				&& FPlatformTime::Seconds() < Deadline)
			{
				FPlatformProcess::SleepNoStats(0.001f);
			}
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(
				ENamedThreads::GameThread
			);
			UpdateVisibleState();
		}
#endif

	private:
		void Dispatch(const FOpenMobileDeviceMockScriptStep& Step)
		{
			const FOpenMobileDeviceMonitoringCallbackToken* ActiveCallback =
				Backend.FindCallback(Step.Group);
			if (!ActiveCallback)
			{
				Backend.SetState(Step.State);
				return;
			}
			const FOpenMobileDeviceMonitoringCallbackToken CallbackToken =
				*ActiveCallback;
			const uint64 SourceSequence = Backend.NextSequence(Step.Group);
			switch (Step.Delivery)
			{
			case EOpenMobileDeviceMockEventDelivery::Immediate:
				Backend.SetState(Step.State);
				FOpenMobileDeviceMonitoringService::NotifyNativeChange(
					CallbackToken,
					SourceSequence
				);
				break;
			case EOpenMobileDeviceMockEventDelivery::Delayed:
			{
				FDelayedEvent& Event = DelayedEvents.AddDefaulted_GetRef();
				Event.Step = Step;
				Event.CallbackToken = CallbackToken;
				Event.SourceSequence = SourceSequence;
				Event.RemainingSeconds = FMath::Max(0.0f, Step.DelaySeconds);
				Event.EventGeneration = EventGeneration->Load();
				EnsureTicker();
				break;
			}
			case EOpenMobileDeviceMockEventDelivery::Duplicate:
				Backend.SetState(Step.State);
				FOpenMobileDeviceMonitoringService::NotifyNativeChange(
					CallbackToken,
					SourceSequence
				);
				FOpenMobileDeviceMonitoringService::NotifyNativeChange(
					CallbackToken,
					SourceSequence
				);
				break;
			case EOpenMobileDeviceMockEventDelivery::OutOfOrder:
			{
				Backend.SetState(Step.State);
				const uint64 LaterSequence = SourceSequence + 1;
				Backend.AdvanceSequence(Step.Group, LaterSequence);
				FOpenMobileDeviceMonitoringService::NotifyNativeChange(
					CallbackToken,
					LaterSequence
				);
				FOpenMobileDeviceMonitoringService::NotifyNativeChange(
					CallbackToken,
					SourceSequence
				);
				break;
			}
			case EOpenMobileDeviceMockEventDelivery::OffThread:
			{
				Backend.SetState(Step.State);
				const uint64 ScheduledGeneration = EventGeneration->Load();
				TSharedRef<TAtomic<uint64>> SharedGeneration = EventGeneration;
				TSharedRef<TAtomic<int32>> SharedPending = PendingBackgroundEvents;
				SharedPending->IncrementExchange();
				AsyncTask(
					ENamedThreads::AnyBackgroundThreadNormalTask,
					[CallbackToken, SourceSequence, ScheduledGeneration,
						SharedGeneration, SharedPending]()
					{
						if (SharedGeneration->Load() == ScheduledGeneration)
						{
							FOpenMobileDeviceMonitoringService::NotifyNativeChange(
								CallbackToken,
								SourceSequence
							);
						}
						SharedPending->DecrementExchange();
					}
				);
				break;
			}
			case EOpenMobileDeviceMockEventDelivery::Stale:
			{
				Backend.SetState(Step.State);
				FOpenMobileDeviceMonitoringCallbackToken StaleToken = CallbackToken;
				++StaleToken.ObserverGeneration;
				if (StaleToken.ObserverGeneration == 0)
				{
					StaleToken.ObserverGeneration = 1;
				}
				FOpenMobileDeviceMonitoringService::NotifyNativeChange(
					StaleToken,
					SourceSequence
				);
				break;
			}
			}
		}

		void EnsureTicker()
		{
			if (DelayedEvents.IsEmpty() || TickerHandle.IsValid())
			{
				return;
			}
			TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([this](float DeltaTime)
				{
					return Tick(DeltaTime);
				})
			);
		}

		void CancelPendingEvents()
		{
			EventGeneration->IncrementExchange();
			DelayedEvents.Reset();
			if (TickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
				TickerHandle.Reset();
			}
		}

		void UpdateVisibleState() const
		{
			UOpenMobileDeviceMockSettings* Settings =
				GetMutableDefault<UOpenMobileDeviceMockSettings>();
			Settings->bMockBackendSelected = IsSelected();
			Settings->ActiveScriptStepCount =
				ScriptSteps.Num() + DelayedEvents.Num()
				+ PendingBackgroundEvents->Load();
			const FOpenMobileDeviceMockState& State = Backend.GetState();
			const FString Battery = State.Power.BatteryPercent.bIsAvailable
				? FString::Printf(TEXT("%.1f%%"), State.Power.BatteryPercent.Value)
				: TEXT("unknown");
			Settings->ActiveOverrideSummary = FString::Printf(
				TEXT("Battery %s, memory %d, network %d, posture %d, appearance %d, failures %d"),
				*Battery,
				static_cast<int32>(State.Memory.PressureState),
				static_cast<int32>(State.Network.PathState),
				static_cast<int32>(State.Window.FoldablePosture),
				static_cast<int32>(State.Appearance.Appearance),
				State.Failures.Num()
			);
		}

		FMockBackend Backend;
		TArray<FOpenMobileDeviceMockScriptStep> ScriptSteps;
		TArray<FDelayedEvent> DelayedEvents;
		FTSTicker::FDelegateHandle TickerHandle;
		TSharedRef<TAtomic<uint64>> EventGeneration =
			MakeShared<TAtomic<uint64>>(1);
		TSharedRef<TAtomic<int32>> PendingBackgroundEvents =
			MakeShared<TAtomic<int32>>(0);
		bool bStarted = false;
	};

	FController Controller;
}

void FOpenMobileDeviceEditorMock::Startup()
{
	check(IsInGameThread());
	OpenMobileDeviceEditorMockPrivate::Controller.Startup();
}

void FOpenMobileDeviceEditorMock::Shutdown()
{
	check(IsInGameThread());
	OpenMobileDeviceEditorMockPrivate::Controller.Shutdown();
}

void FOpenMobileDeviceEditorMock::ApplySettings()
{
	check(IsInGameThread());
	OpenMobileDeviceEditorMockPrivate::Controller.ApplySettings();
}

void FOpenMobileDeviceEditorMock::SetEnabled(bool bEnabled)
{
	check(IsInGameThread());
	OpenMobileDeviceEditorMockPrivate::Controller.SetEnabled(bEnabled);
}

bool FOpenMobileDeviceEditorMock::IsEnabled()
{
	check(IsInGameThread());
	return OpenMobileDeviceEditorMockPrivate::Controller.IsEnabled();
}

bool FOpenMobileDeviceEditorMock::IsSelected()
{
	check(IsInGameThread());
	return OpenMobileDeviceEditorMockPrivate::Controller.IsSelected();
}

void FOpenMobileDeviceEditorMock::SetState(
	const FOpenMobileDeviceMockState& State
)
{
	check(IsInGameThread());
	OpenMobileDeviceEditorMockPrivate::Controller.SetState(State);
}

const FOpenMobileDeviceMockState& FOpenMobileDeviceEditorMock::GetState()
{
	check(IsInGameThread());
	return OpenMobileDeviceEditorMockPrivate::Controller.GetState();
}

void FOpenMobileDeviceEditorMock::QueueScriptStep(
	const FOpenMobileDeviceMockScriptStep& Step
)
{
	check(IsInGameThread());
	OpenMobileDeviceEditorMockPrivate::Controller.QueueScriptStep(Step);
}

bool FOpenMobileDeviceEditorMock::RunNextScriptStep()
{
	check(IsInGameThread());
	return OpenMobileDeviceEditorMockPrivate::Controller.RunNextScriptStep();
}

int32 FOpenMobileDeviceEditorMock::GetQueuedScriptStepCount()
{
	check(IsInGameThread());
	return OpenMobileDeviceEditorMockPrivate::Controller.GetQueuedScriptStepCount();
}

void FOpenMobileDeviceEditorMock::ResetOverrides()
{
	check(IsInGameThread());
	OpenMobileDeviceEditorMockPrivate::Controller.ResetOverrides();
}

#if WITH_DEV_AUTOMATION_TESTS
void FOpenMobileDeviceEditorMock::ResetForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceEditorMockPrivate::Controller.SetEnabled(false);
	OpenMobileDeviceEditorMockPrivate::Controller.ResetOverrides();
}

void FOpenMobileDeviceEditorMock::TickForTests(float DeltaTime)
{
	check(IsInGameThread());
	OpenMobileDeviceEditorMockPrivate::Controller.TickForTests(DeltaTime);
}

void FOpenMobileDeviceEditorMock::FlushBackgroundEventsForTests()
{
	check(IsInGameThread());
	OpenMobileDeviceEditorMockPrivate::Controller.FlushBackgroundEventsForTests();
}
#endif
