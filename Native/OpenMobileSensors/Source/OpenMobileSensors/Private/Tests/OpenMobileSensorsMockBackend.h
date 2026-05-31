#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "IOpenMobileSensorsBackend.h"

enum class EOpenMobileSensorsMockEventType : uint8
{
	Capability,
	SampleBatch,
	Permission,
	Lifecycle,
	Error,
	Delay
};

struct FOpenMobileSensorsMockEvent
{
	EOpenMobileSensorsMockEventType Type =
		EOpenMobileSensorsMockEventType::Capability;
	FOpenMobileCapability Capability;
	TArray<FVector3d> SampleBatch;
	FName Permission;
	EOpenMobileCapabilityState PermissionState =
		EOpenMobileCapabilityState::Unavailable;
	bool bForeground = true;
	FOpenMobileError Error;
	double DelaySeconds = 0.0;
};

class FOpenMobileSensorsMockBackend final : public IOpenMobileSensorsBackend
{
public:
	explicit FOpenMobileSensorsMockBackend(
		FName InName = TEXT("Mock"),
		int32 InPriority = 100
	)
		: Name(InName)
		, Priority(InPriority)
	{
		BackendCapability.Name = GetModularFeatureName();
		BackendCapability.State = EOpenMobileCapabilityState::Available;
	}

	virtual FName GetBackendName() const override
	{
		return Name;
	}

	virtual int32 GetPriority() const override
	{
		return Priority;
	}

	virtual bool IsAvailable() const override
	{
		return bAvailable;
	}

	virtual FOpenMobileCapability GetBackendCapability() const override
	{
		++CapabilityQueryCount;
		return BackendCapability;
	}

	virtual TArray<FOpenMobileSensorCapability>
	GetSensorCapabilities() const override
	{
		++SensorCapabilityQueryCount;
		return SensorCapabilities;
	}

	virtual void BeginShutdown() override
	{
		bShutdown = true;
		Script.Reset();
		NextEventIndex = 0;
		RemainingDelaySeconds = 0.0;
	}

	void SetAvailable(bool bInAvailable)
	{
		bAvailable = bInAvailable;
	}

	void SetSensorCapabilities(
		TArray<FOpenMobileSensorCapability> InCapabilities
	)
	{
		SensorCapabilities = MoveTemp(InCapabilities);
	}

	void AddCapability(FOpenMobileCapability Capability)
	{
		FOpenMobileSensorsMockEvent Event;
		Event.Type = EOpenMobileSensorsMockEventType::Capability;
		Event.Capability = MoveTemp(Capability);
		Script.Add(MoveTemp(Event));
	}

	void AddSampleBatch(TArray<FVector3d> Samples)
	{
		FOpenMobileSensorsMockEvent Event;
		Event.Type = EOpenMobileSensorsMockEventType::SampleBatch;
		Event.SampleBatch = MoveTemp(Samples);
		Script.Add(MoveTemp(Event));
	}

	void AddPermission(
		FName Permission,
		EOpenMobileCapabilityState PermissionState
	)
	{
		FOpenMobileSensorsMockEvent Event;
		Event.Type = EOpenMobileSensorsMockEventType::Permission;
		Event.Permission = Permission;
		Event.PermissionState = PermissionState;
		Script.Add(MoveTemp(Event));
	}

	void AddLifecycle(bool bForeground)
	{
		FOpenMobileSensorsMockEvent Event;
		Event.Type = EOpenMobileSensorsMockEventType::Lifecycle;
		Event.bForeground = bForeground;
		Script.Add(MoveTemp(Event));
	}

	void AddError(FOpenMobileError Error)
	{
		FOpenMobileSensorsMockEvent Event;
		Event.Type = EOpenMobileSensorsMockEventType::Error;
		Event.Error = MoveTemp(Error);
		Script.Add(MoveTemp(Event));
	}

	void AddDelay(double DelaySeconds)
	{
		FOpenMobileSensorsMockEvent Event;
		Event.Type = EOpenMobileSensorsMockEventType::Delay;
		Event.DelaySeconds = FMath::Max(0.0, DelaySeconds);
		Script.Add(MoveTemp(Event));
	}

	void AdvanceScript(
		double DeltaSeconds,
		TFunctionRef<void(const FOpenMobileSensorsMockEvent&)> OnEvent
	)
	{
		constexpr double DelayToleranceSeconds = 1.e-9;
		double RemainingDeltaSeconds = FMath::Max(0.0, DeltaSeconds);
		if (RemainingDelaySeconds
			> RemainingDeltaSeconds + DelayToleranceSeconds)
		{
			RemainingDelaySeconds -= RemainingDeltaSeconds;
			return;
		}
		RemainingDeltaSeconds = FMath::Max(
			0.0,
			RemainingDeltaSeconds - RemainingDelaySeconds
		);
		RemainingDelaySeconds = 0.0;

		while (NextEventIndex < Script.Num())
		{
			const FOpenMobileSensorsMockEvent& Event = Script[NextEventIndex++];
			if (Event.Type != EOpenMobileSensorsMockEventType::Delay)
			{
				OnEvent(Event);
				continue;
			}

			if (Event.DelaySeconds
				> RemainingDeltaSeconds + DelayToleranceSeconds)
			{
				RemainingDelaySeconds =
					Event.DelaySeconds - RemainingDeltaSeconds;
				return;
			}
			RemainingDeltaSeconds -= Event.DelaySeconds;
		}
	}

	bool WasShutdown() const
	{
		return bShutdown;
	}

	int32 GetCapabilityQueryCount() const
	{
		return CapabilityQueryCount;
	}

	int32 GetSensorCapabilityQueryCount() const
	{
		return SensorCapabilityQueryCount;
	}

private:
	FName Name;
	int32 Priority = 100;
	bool bAvailable = true;
	bool bShutdown = false;
	mutable int32 CapabilityQueryCount = 0;
	FOpenMobileCapability BackendCapability;
	TArray<FOpenMobileSensorCapability> SensorCapabilities;
	mutable int32 SensorCapabilityQueryCount = 0;
	TArray<FOpenMobileSensorsMockEvent> Script;
	int32 NextEventIndex = 0;
	double RemainingDelaySeconds = 0.0;
};

#endif
