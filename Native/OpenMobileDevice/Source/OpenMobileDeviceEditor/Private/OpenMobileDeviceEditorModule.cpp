#include "Editor.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileDeviceBuildValidation.h"
#include "OpenMobileDeviceDiagnosticsScreen.h"
#include "OpenMobileDeviceEditorMock.h"
#include "OpenMobileDeviceMockSettings.h"

class FOpenMobileDeviceEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(
			IOpenMobileDeviceBuildValidationContributor::GetModularFeatureName(),
			&BuildValidation
		);
		FOpenMobileDeviceEditorMock::Startup();
		FOpenMobileDeviceDiagnosticsScreen::Register();
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
		FOpenMobileDeviceDiagnosticsScreen::Unregister();
		IModularFeatures::Get().UnregisterModularFeature(
			IOpenMobileDeviceBuildValidationContributor::GetModularFeatureName(),
			&BuildValidation
		);
		if (EndPieHandle.IsValid())
		{
			FEditorDelegates::EndPIE.Remove(EndPieHandle);
			EndPieHandle.Reset();
		}
		FOpenMobileDeviceEditorMock::Shutdown();
	}

private:
	FOpenMobileDeviceBuildValidation BuildValidation;
	FDelegateHandle EndPieHandle;
};

IMPLEMENT_MODULE(FOpenMobileDeviceEditorModule, OpenMobileDeviceEditor)
