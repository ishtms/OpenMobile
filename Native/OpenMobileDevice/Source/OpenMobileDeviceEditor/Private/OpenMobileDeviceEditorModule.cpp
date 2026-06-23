#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileDeviceEditorMock.h"
#include "OpenMobileDeviceMockSettings.h"

class FOpenMobileDeviceEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FOpenMobileDeviceEditorMock::Startup();
		EndPieHandle = FEditorDelegates::EndPIE.AddLambda([](bool bSimulating)
		{
			static_cast<void>(bSimulating);
			if (GetDefault<UOpenMobileDeviceMockSettings>()
				->bResetOverridesAfterPlayInEditor)
			{
				FOpenMobileDeviceEditorMock::ResetOverrides();
			}
		});
	}

	virtual void ShutdownModule() override
	{
		if (EndPieHandle.IsValid())
		{
			FEditorDelegates::EndPIE.Remove(EndPieHandle);
			EndPieHandle.Reset();
		}
		FOpenMobileDeviceEditorMock::Shutdown();
	}

private:
	FDelegateHandle EndPieHandle;
};

IMPLEMENT_MODULE(FOpenMobileDeviceEditorModule, OpenMobileDeviceEditor)
