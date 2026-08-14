#pragma once

#include "CoreMinimal.h"

class UGameInstance;

enum class EOpenMobileAdsFullscreenSurface : uint8
{
	None,
	Ad,
	Consent,
	TrackingAuthorization,
	Inspector
};

/** Applies and restores Unreal gameplay state around one provider-owned fullscreen surface. */
class IOpenMobileAdsFullscreenLifecycleTarget
{
public:
	virtual ~IOpenMobileAdsFullscreenLifecycleTarget() = default;
	/** Pauses or blocks gameplay immediately before native presentation starts. */
	virtual void Apply() = 0;
	/** Restores gameplay state as soon as the matching surface ends. */
	virtual void RestoreGameplay() = 0;
	/** Restores input focus only after the application is active and foreground again. */
	virtual void RestoreFocus() = 0;
};

class FOpenMobileAdsFullscreenLifecycleCoordinator
{
public:
	/** Owns one target so every Ads, consent, prompt, or inspector surface shares the same restoration path. */
	explicit FOpenMobileAdsFullscreenLifecycleCoordinator(
		TUniquePtr<IOpenMobileAdsFullscreenLifecycleTarget> InTarget
	);

	/** Reserves fullscreen ownership before asynchronous SDK presentation can begin. */
	bool TryReserve(EOpenMobileAdsFullscreenSurface Surface, FGuid OwnerId);
	/** Applies gameplay state only for the surface and owner that hold the reservation. */
	bool BeginPresentation(EOpenMobileAdsFullscreenSurface Surface, FGuid OwnerId);
	/** Releases matching ownership and defers focus restoration when the app isn't ready. */
	bool End(EOpenMobileAdsFullscreenSurface Surface, FGuid OwnerId);
	/** Unblocks pending focus restoration only when the application is active. */
	void SetApplicationActive(bool bActive);
	/** Unblocks pending focus restoration only when the application is foreground. */
	void SetApplicationInForeground(bool bInForeground);
	/** Restores any applied gameplay state and drops ownership during subsystem teardown. */
	void Shutdown();

	/** Reports reservations as occupied even before native presentation actually starts. */
	bool IsOccupied() const
	{
		return ActiveSurface != EOpenMobileAdsFullscreenSurface::None;
	}

	/** Separates active presentation from a reservation waiting on the provider. */
	bool IsPresenting() const { return bPresentationActive; }

private:
	/** Requires both surface kind and owner identity so a stale callback can't end newer work. */
	bool Matches(EOpenMobileAdsFullscreenSurface Surface, FGuid OwnerId) const;
	/** Restores focus once both application lifecycle gates allow it. */
	void RestorePendingFocusIfReady();

	TUniquePtr<IOpenMobileAdsFullscreenLifecycleTarget> Target;
	EOpenMobileAdsFullscreenSurface ActiveSurface =
		EOpenMobileAdsFullscreenSurface::None;
	FGuid ActiveOwnerId;
	bool bApplicationActive = true;
	bool bApplicationInForeground = true;
	bool bPresentationActive = false;
	bool bFocusRestorePending = false;
};

/** Creates the Unreal gameplay and focus target for one Game Instance. */
TUniquePtr<IOpenMobileAdsFullscreenLifecycleTarget>
OpenMobileAdsCreateUnrealFullscreenLifecycleTarget(UGameInstance& GameInstance);
