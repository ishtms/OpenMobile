#include "OpenMobileHapticsSamplePlayerController.h"

#include "OpenMobileHapticsSampleWidget.h"

void AOpenMobileHapticsSamplePlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}

	HapticsWidget = CreateWidget<UOpenMobileHapticsSampleWidget>(
		this,
		UOpenMobileHapticsSampleWidget::StaticClass()
	);
	if (HapticsWidget)
	{
		HapticsWidget->AddToViewport();
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(HapticsWidget->TakeWidget());
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}
}
