#include "OpenMobileDeviceSamplePlayerController.h"

#include "OpenMobileDeviceDemoWidget.h"

void AOpenMobileDeviceSamplePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	DeviceWidget = CreateWidget<UOpenMobileDeviceDemoWidget>(
		this,
		UOpenMobileDeviceDemoWidget::StaticClass()
	);
	if (DeviceWidget)
	{
		DeviceWidget->AddToViewport();
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(DeviceWidget->TakeWidget());
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}
}
