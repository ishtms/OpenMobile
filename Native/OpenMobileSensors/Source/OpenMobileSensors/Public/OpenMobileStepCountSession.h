#pragma once

#include "CoreMinimal.h"
#include "OpenMobileStepCountSession.generated.h"

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileStepCountSessionPolicy
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Confirmed count survives app pause even when native delivery is suspended."))
	bool bPersistsAcrossApplicationPause = true;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "The plugin does not promise to recover steps taken while native delivery is suspended."))
	bool bBackfillsStepsWhilePaused = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "A session belongs to one Game Instance and is not transferred to its replacement."))
	bool bPersistsAcrossGameInstanceRecreation = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Session state is memory-only and is not restored after process restart."))
	bool bPersistsAcrossProcessRestart = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Unrecoverable motion-permission loss ends the session and requires a new baseline after access returns."))
	bool bPersistsAcrossPermissionLoss = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "A native-origin change retains the last confirmed session count and rebases future deltas."))
	bool bCarriesConfirmedCountAcrossNativeReset = true;
};
