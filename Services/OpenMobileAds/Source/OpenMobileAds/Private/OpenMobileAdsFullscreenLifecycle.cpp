#include "OpenMobileAdsFullscreenLifecycle.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"

namespace OpenMobileAdsFullscreenLifecyclePrivate
{
	struct FWorldPauseChange
	{
		TWeakObjectPtr<UWorld> World;
		bool bPausedByAds = false;
	};

	struct FControllerInputChange
	{
		TWeakObjectPtr<APlayerController> Controller;
		bool bMoveBlockedByAds = false;
		bool bLookBlockedByAds = false;
	};

	class FUnrealFullscreenLifecycleTarget final
		: public IOpenMobileAdsFullscreenLifecycleTarget
	{
	public:
		explicit FUnrealFullscreenLifecycleTarget(UGameInstance& InGameInstance)
			: GameInstance(&InGameInstance)
		{
		}

		virtual void Apply() override
		{
			ApplyPause();
			ApplyInputBlock();
			ApplyAudioMute();
			ApplyFocusRelease();
		}

		virtual void RestoreGameplay() override
		{
			for (FWorldPauseChange& Change : PauseChanges)
			{
				if (
					Change.bPausedByAds
					&& Change.World.IsValid()
					&& UGameplayStatics::IsGamePaused(Change.World.Get())
				)
				{
					UGameplayStatics::SetGamePaused(Change.World.Get(), false);
				}
			}
			PauseChanges.Reset();

			for (FControllerInputChange& Change : InputChanges)
			{
				if (APlayerController* Controller = Change.Controller.Get())
				{
					if (Change.bMoveBlockedByAds)
					{
						Controller->SetIgnoreMoveInput(false);
					}
					if (Change.bLookBlockedByAds)
					{
						Controller->SetIgnoreLookInput(false);
					}
				}
			}
			InputChanges.Reset();

			if (
				bAudioMutedByAds
				&& FMath::IsNearlyZero(FApp::GetVolumeMultiplier())
			)
			{
				FApp::SetVolumeMultiplier(PreviousVolumeMultiplier);
			}
			bAudioStateCaptured = false;
			bAudioMutedByAds = false;
			PreviousVolumeMultiplier = 1.0f;
		}

		virtual void RestoreFocus() override
		{
			if (
				bFocusStateCaptured
				&& FSlateApplication::IsInitialized()
				&& !FSlateApplication::Get().GetKeyboardFocusedWidget().IsValid()
			)
			{
				if (const TSharedPtr<SWidget> Widget = PreviousKeyboardFocus.Pin())
				{
					FSlateApplication::Get().SetKeyboardFocus(
						Widget,
						EFocusCause::SetDirectly
					);
				}
			}
			PreviousKeyboardFocus.Reset();
			bFocusStateCaptured = false;
		}

	private:
		void ApplyPause()
		{
			UGameInstance* Instance = GameInstance.Get();
			UWorld* World = Instance ? Instance->GetWorld() : nullptr;
			if (!World)
			{
				return;
			}

			FWorldPauseChange* Existing = PauseChanges.FindByPredicate(
				[World](const FWorldPauseChange& Change)
				{
					return Change.World.Get() == World;
				}
			);
			if (!Existing)
			{
				Existing = &PauseChanges.Emplace_GetRef();
				Existing->World = World;
				if (!UGameplayStatics::IsGamePaused(World))
				{
					Existing->bPausedByAds = UGameplayStatics::SetGamePaused(
						World,
						true
					);
				}
			}
			else if (!UGameplayStatics::IsGamePaused(World))
			{
				Existing->bPausedByAds = UGameplayStatics::SetGamePaused(
					World,
					true
				);
			}
		}

		void ApplyInputBlock()
		{
			UGameInstance* Instance = GameInstance.Get();
			UWorld* World = Instance ? Instance->GetWorld() : nullptr;
			if (!Instance || !World)
			{
				return;
			}

			for (ULocalPlayer* LocalPlayer : Instance->GetLocalPlayers())
			{
				APlayerController* Controller = LocalPlayer
					? LocalPlayer->GetPlayerController(World)
					: nullptr;
				if (!Controller)
				{
					continue;
				}

				FControllerInputChange* Existing = InputChanges.FindByPredicate(
					[Controller](const FControllerInputChange& Change)
					{
						return Change.Controller.Get() == Controller;
					}
				);
				if (!Existing)
				{
					Existing = &InputChanges.Emplace_GetRef();
					Existing->Controller = Controller;
				}
				if (!Controller->IsMoveInputIgnored())
				{
					Controller->SetIgnoreMoveInput(true);
					Existing->bMoveBlockedByAds = true;
				}
				if (!Controller->IsLookInputIgnored())
				{
					Controller->SetIgnoreLookInput(true);
					Existing->bLookBlockedByAds = true;
				}
			}
		}

		void ApplyAudioMute()
		{
			const float CurrentVolumeMultiplier = FApp::GetVolumeMultiplier();
			if (!bAudioStateCaptured)
			{
				PreviousVolumeMultiplier = CurrentVolumeMultiplier;
				bAudioStateCaptured = true;
			}
			if (!bAudioMutedByAds && !FMath::IsNearlyZero(CurrentVolumeMultiplier))
			{
				PreviousVolumeMultiplier = CurrentVolumeMultiplier;
				bAudioMutedByAds = true;
			}
			if (bAudioMutedByAds && !FMath::IsNearlyZero(CurrentVolumeMultiplier))
			{
				FApp::SetVolumeMultiplier(0.0f);
			}
		}

		void ApplyFocusRelease()
		{
			if (!FSlateApplication::IsInitialized())
			{
				return;
			}
			if (!bFocusStateCaptured)
			{
				PreviousKeyboardFocus =
					FSlateApplication::Get().GetKeyboardFocusedWidget();
				bFocusStateCaptured = true;
			}
			if (FSlateApplication::Get().GetKeyboardFocusedWidget().IsValid())
			{
				FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
			}
		}

		TWeakObjectPtr<UGameInstance> GameInstance;
		TArray<FWorldPauseChange> PauseChanges;
		TArray<FControllerInputChange> InputChanges;
		TWeakPtr<SWidget> PreviousKeyboardFocus;
		float PreviousVolumeMultiplier = 1.0f;
		bool bAudioStateCaptured = false;
		bool bAudioMutedByAds = false;
		bool bFocusStateCaptured = false;
	};
}

FOpenMobileAdsFullscreenLifecycleCoordinator::
	FOpenMobileAdsFullscreenLifecycleCoordinator(
		TUniquePtr<IOpenMobileAdsFullscreenLifecycleTarget> InTarget
	)
	: Target(MoveTemp(InTarget))
{
	check(Target);
}

bool FOpenMobileAdsFullscreenLifecycleCoordinator::TryReserve(
	EOpenMobileAdsFullscreenSurface Surface,
	FGuid OwnerId
)
{
	if (
		Surface == EOpenMobileAdsFullscreenSurface::None
		|| !OwnerId.IsValid()
		|| IsOccupied()
		|| bFocusRestorePending
		|| !bApplicationActive
		|| !bApplicationInForeground
	)
	{
		return false;
	}
	ActiveSurface = Surface;
	ActiveOwnerId = OwnerId;
	return true;
}

bool FOpenMobileAdsFullscreenLifecycleCoordinator::BeginPresentation(
	EOpenMobileAdsFullscreenSurface Surface,
	FGuid OwnerId
)
{
	if (!Matches(Surface, OwnerId))
	{
		return false;
	}
	if (!bPresentationActive)
	{
		bPresentationActive = true;
		Target->Apply();
	}
	return true;
}

bool FOpenMobileAdsFullscreenLifecycleCoordinator::End(
	EOpenMobileAdsFullscreenSurface Surface,
	FGuid OwnerId
)
{
	if (!Matches(Surface, OwnerId))
	{
		return false;
	}
	ActiveSurface = EOpenMobileAdsFullscreenSurface::None;
	ActiveOwnerId.Invalidate();
	if (bPresentationActive)
	{
		Target->RestoreGameplay();
		bPresentationActive = false;
		if (bApplicationActive && bApplicationInForeground)
		{
			Target->RestoreFocus();
			bFocusRestorePending = false;
		}
		else
		{
			bFocusRestorePending = true;
		}
	}
	return true;
}

void FOpenMobileAdsFullscreenLifecycleCoordinator::SetApplicationActive(
	bool bActive
)
{
	bApplicationActive = bActive;
	if (bPresentationActive)
	{
		Target->Apply();
	}
	RestorePendingFocusIfReady();
}

void FOpenMobileAdsFullscreenLifecycleCoordinator::SetApplicationInForeground(
	bool bInForeground
)
{
	bApplicationInForeground = bInForeground;
	if (bPresentationActive)
	{
		Target->Apply();
	}
	RestorePendingFocusIfReady();
}

void FOpenMobileAdsFullscreenLifecycleCoordinator::Shutdown()
{
	ActiveSurface = EOpenMobileAdsFullscreenSurface::None;
	ActiveOwnerId.Invalidate();
	if (bPresentationActive)
	{
		Target->RestoreGameplay();
		bPresentationActive = false;
	}
	Target->RestoreFocus();
	bFocusRestorePending = false;
}

bool FOpenMobileAdsFullscreenLifecycleCoordinator::Matches(
	EOpenMobileAdsFullscreenSurface Surface,
	FGuid OwnerId
) const
{
	return ActiveSurface == Surface
		&& ActiveSurface != EOpenMobileAdsFullscreenSurface::None
		&& ActiveOwnerId == OwnerId;
}

void FOpenMobileAdsFullscreenLifecycleCoordinator::RestorePendingFocusIfReady()
{
	if (
		bFocusRestorePending
		&& !bPresentationActive
		&& bApplicationActive
		&& bApplicationInForeground
	)
	{
		Target->RestoreFocus();
		bFocusRestorePending = false;
	}
}

TUniquePtr<IOpenMobileAdsFullscreenLifecycleTarget>
OpenMobileAdsCreateUnrealFullscreenLifecycleTarget(UGameInstance& GameInstance)
{
	return MakeUnique<
		OpenMobileAdsFullscreenLifecyclePrivate::FUnrealFullscreenLifecycleTarget
	>(GameInstance);
}
