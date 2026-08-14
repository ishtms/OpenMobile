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
	/** Applies one application event and records whether playback or native services need action, duplicate events stay harmless. */
	FOpenMobileHapticsLifecycleTransition Apply(
		EOpenMobileHapticsLifecycleEvent Event
	);
	/** Returns lifecycle tracking to active when the subsystem is recreated or deliberately refreshed. */
	void Reset();

	/** Exposes the last accepted application state used for request decisions. */
	EOpenMobileHapticsApplicationState GetState() const { return State; }

	/** Decides whether a request may run outside the foreground, including the narrow background-alert exception. */
	static EOpenMobileHapticsLifecycleRequestOutcome Evaluate(
		const FOpenMobileHapticsLifecycleRequestContext& Request,
		EOpenMobileHapticsApplicationState ApplicationState
	);

private:
	EOpenMobileHapticsApplicationState State =
		EOpenMobileHapticsApplicationState::Active;
};
