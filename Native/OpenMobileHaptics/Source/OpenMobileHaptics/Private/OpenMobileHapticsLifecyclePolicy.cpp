#include "OpenMobileHapticsLifecyclePolicy.h"

namespace OpenMobileHapticsLifecyclePolicyPrivate
{
	/** Restricts background-alert exceptions to notification semantics, selection and impact should remain suppressed. */
	bool IsNotificationEffect(EOpenMobileHapticSemanticEffect Effect)
	{
		return Effect == EOpenMobileHapticSemanticEffect::NotificationSuccess
			|| Effect == EOpenMobileHapticSemanticEffect::NotificationWarning
			|| Effect == EOpenMobileHapticSemanticEffect::NotificationError;
	}
}

FOpenMobileHapticsLifecycleTransition FOpenMobileHapticsLifecyclePolicy::Apply(
	EOpenMobileHapticsLifecycleEvent Event
)
{
	FOpenMobileHapticsLifecycleTransition Result;
	Result.Event = Event;
	Result.PreviousState = State;
	Result.CurrentState = State;
	if (State == EOpenMobileHapticsApplicationState::Terminating)
	{
		return Result;
	}

	switch (Event)
	{
	case EOpenMobileHapticsLifecycleEvent::WillDeactivate:
		if (State == EOpenMobileHapticsApplicationState::Active)
		{
			State = EOpenMobileHapticsApplicationState::Inactive;
			Result.bInterruptsPlayback = true;
		}
		break;
	case EOpenMobileHapticsLifecycleEvent::WillEnterBackground:
		if (State != EOpenMobileHapticsApplicationState::Background)
		{
			Result.bInterruptsPlayback =
				State == EOpenMobileHapticsApplicationState::Active;
			State = EOpenMobileHapticsApplicationState::Background;
		}
		break;
	case EOpenMobileHapticsLifecycleEvent::HasEnteredForeground:
		if (State == EOpenMobileHapticsApplicationState::Background)
		{
			State = EOpenMobileHapticsApplicationState::Inactive;
			Result.bInterruptsPlayback = true;
		}
		break;
	case EOpenMobileHapticsLifecycleEvent::HasReactivated:
		if (State != EOpenMobileHapticsApplicationState::Active)
		{
			Result.bInterruptsPlayback =
				State == EOpenMobileHapticsApplicationState::Background;
			State = EOpenMobileHapticsApplicationState::Active;
			Result.bRefreshesNativeServices = true;
		}
		break;
	case EOpenMobileHapticsLifecycleEvent::WillTerminate:
		State = EOpenMobileHapticsApplicationState::Terminating;
		Result.bInterruptsPlayback = true;
		break;
	default:
		break;
	}

	Result.CurrentState = State;
	Result.bChanged = Result.CurrentState != Result.PreviousState;
	return Result;
}

void FOpenMobileHapticsLifecyclePolicy::Reset()
{
	State = EOpenMobileHapticsApplicationState::Active;
}

EOpenMobileHapticsLifecycleRequestOutcome
FOpenMobileHapticsLifecyclePolicy::Evaluate(
	const FOpenMobileHapticsLifecycleRequestContext& Request,
	EOpenMobileHapticsApplicationState ApplicationState
)
{
	if (ApplicationState == EOpenMobileHapticsApplicationState::Active)
	{
		return EOpenMobileHapticsLifecycleRequestOutcome::Allowed;
	}
	if (ApplicationState != EOpenMobileHapticsApplicationState::Background
		|| Request.BackgroundPolicy
			!= EOpenMobileHapticBackgroundPolicy::CriticalOnly
		|| Request.BackgroundAlerts
			!= EOpenMobileHapticSupportState::Supported
		|| Request.Priority != EOpenMobileHapticChannelPriority::Critical
		|| Request.Category != TEXT("Alerts"))
	{
		return EOpenMobileHapticsLifecycleRequestOutcome::Suppressed;
	}

	if (Request.Kind == EOpenMobileHapticsLifecycleRequestKind::Semantic
		&& !OpenMobileHapticsLifecyclePolicyPrivate::IsNotificationEffect(
			Request.SemanticEffect
		))
	{
		return EOpenMobileHapticsLifecycleRequestOutcome::Suppressed;
	}
	if (Request.Kind == EOpenMobileHapticsLifecycleRequestKind::NamedPattern
		&& !Request.bPatternSuitableForBackgroundPlayback)
	{
		return EOpenMobileHapticsLifecycleRequestOutcome::Suppressed;
	}
	return EOpenMobileHapticsLifecycleRequestOutcome::BackgroundAlert;
}
