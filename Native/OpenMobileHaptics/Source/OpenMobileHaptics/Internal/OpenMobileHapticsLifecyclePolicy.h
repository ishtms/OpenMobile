#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsSettings.h"
#include "OpenMobileHapticsTypes.h"

enum class EOpenMobileHapticsApplicationState : uint8
{
	Active,
	Inactive,
	Background,
	Terminating
};

enum class EOpenMobileHapticsLifecycleEvent : uint8
{
	WillDeactivate,
	WillEnterBackground,
	HasEnteredForeground,
	HasReactivated,
	WillTerminate
};

enum class EOpenMobileHapticsLifecycleRequestKind : uint8
{
	Semantic,
	OneShot,
	NamedPattern
};

enum class EOpenMobileHapticsLifecycleRequestOutcome : uint8
{
	Allowed,
	BackgroundAlert,
	Suppressed
};

struct FOpenMobileHapticsLifecycleTransition
{
	EOpenMobileHapticsLifecycleEvent Event =
		EOpenMobileHapticsLifecycleEvent::WillDeactivate;
	EOpenMobileHapticsApplicationState PreviousState =
		EOpenMobileHapticsApplicationState::Active;
	EOpenMobileHapticsApplicationState CurrentState =
		EOpenMobileHapticsApplicationState::Active;
	bool bChanged = false;
	bool bInterruptsPlayback = false;
	bool bRefreshesNativeServices = false;
};

struct FOpenMobileHapticsLifecycleRequestContext
{
	EOpenMobileHapticBackgroundPolicy BackgroundPolicy =
		EOpenMobileHapticBackgroundPolicy::StopAll;
	EOpenMobileHapticSupportState BackgroundAlerts =
		EOpenMobileHapticSupportState::Unknown;
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;
	FName Category = TEXT("Gameplay");
	EOpenMobileHapticsLifecycleRequestKind Kind =
		EOpenMobileHapticsLifecycleRequestKind::Semantic;
	EOpenMobileHapticSemanticEffect SemanticEffect =
		EOpenMobileHapticSemanticEffect::Selection;
	bool bPatternSuitableForBackgroundPlayback = false;
};

class OPENMOBILEHAPTICS_API FOpenMobileHapticsLifecyclePolicy final
{
public:
	FOpenMobileHapticsLifecycleTransition Apply(
		EOpenMobileHapticsLifecycleEvent Event
	);
	void Reset();

	EOpenMobileHapticsApplicationState GetState() const { return State; }

	static EOpenMobileHapticsLifecycleRequestOutcome Evaluate(
		const FOpenMobileHapticsLifecycleRequestContext& Request,
		EOpenMobileHapticsApplicationState ApplicationState
	);

private:
	EOpenMobileHapticsApplicationState State =
		EOpenMobileHapticsApplicationState::Active;
};
