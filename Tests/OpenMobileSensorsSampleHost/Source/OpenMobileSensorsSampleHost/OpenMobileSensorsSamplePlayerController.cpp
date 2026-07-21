#include "OpenMobileSensorsSamplePlayerController.h"

#include "OpenMobileSensorsDemoWidget.h"

void AOpenMobileSensorsSamplePlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	SensorsWidget = CreateWidget<UOpenMobileSensorsDemoWidget>(
		this,
		UOpenMobileSensorsDemoWidget::StaticClass()
	);
	if (SensorsWidget)
	{
		SensorsWidget->AddToViewport();
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(SensorsWidget->TakeWidget());
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}
}
