#include "OpenMobileSensorsEditorMockSettings.h"

void UOpenMobileSensorsEditorMockSettings::ApplyConfiguredInput()
{
	UOpenMobileSensorsDevelopmentLibrary::ApplyMockInput(Input);
}

void UOpenMobileSensorsEditorMockSettings::ApplyConfiguredPreset()
{
	UOpenMobileSensorsDevelopmentLibrary::ApplyMockPreset(Preset);
}

void UOpenMobileSensorsEditorMockSettings::PlayConfiguredTimeline()
{
	UOpenMobileSensorsDevelopmentLibrary::PlayMockTimeline(Timeline);
}

void UOpenMobileSensorsEditorMockSettings::StopConfiguredTimeline()
{
	UOpenMobileSensorsDevelopmentLibrary::StopMockTimeline();
}

void UOpenMobileSensorsEditorMockSettings::AdvanceConfiguredTimeline()
{
	UOpenMobileSensorsDevelopmentLibrary::AdvanceMockTimeline(
		TimelineAdvanceSeconds
	);
}

void UOpenMobileSensorsEditorMockSettings::InjectConfiguredError()
{
	UOpenMobileSensorsDevelopmentLibrary::InjectMockError(
		ErrorSensor,
		ErrorReason,
		NativeErrorCode
	);
}
