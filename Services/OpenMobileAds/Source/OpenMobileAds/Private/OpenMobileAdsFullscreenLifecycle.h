#pragma once

#include "CoreMinimal.h"

class UGameInstance;

enum class EOpenMobileAdsFullscreenSurface : uint8
{
	None,
	Ad,
	Consent,
	Inspector
};

class IOpenMobileAdsFullscreenLifecycleTarget
{
public:
	virtual ~IOpenMobileAdsFullscreenLifecycleTarget() = default;
	virtual void Apply() = 0;
	virtual void RestoreGameplay() = 0;
	virtual void RestoreFocus() = 0;
};

class FOpenMobileAdsFullscreenLifecycleCoordinator
{
public:
	explicit FOpenMobileAdsFullscreenLifecycleCoordinator(
		TUniquePtr<IOpenMobileAdsFullscreenLifecycleTarget> InTarget
	);

	bool TryReserve(EOpenMobileAdsFullscreenSurface Surface, FGuid OwnerId);
	bool BeginPresentation(EOpenMobileAdsFullscreenSurface Surface, FGuid OwnerId);
	bool End(EOpenMobileAdsFullscreenSurface Surface, FGuid OwnerId);
	void SetApplicationActive(bool bActive);
	void SetApplicationInForeground(bool bInForeground);
	void Shutdown();

	bool IsOccupied() const
	{
		return ActiveSurface != EOpenMobileAdsFullscreenSurface::None;
	}

	bool IsPresenting() const { return bPresentationActive; }

private:
	bool Matches(EOpenMobileAdsFullscreenSurface Surface, FGuid OwnerId) const;
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

TUniquePtr<IOpenMobileAdsFullscreenLifecycleTarget>
OpenMobileAdsCreateUnrealFullscreenLifecycleTarget(UGameInstance& GameInstance);
