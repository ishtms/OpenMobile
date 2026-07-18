#include "ISequencerModule.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileHapticsTrackEditor.h"

class FOpenMobileHapticsSequencerEditorModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		ISequencerModule& Sequencer =
			FModuleManager::LoadModuleChecked<ISequencerModule>(TEXT("Sequencer"));
		TrackEditorHandle = Sequencer.RegisterTrackEditor(
			FOnCreateTrackEditor::CreateStatic(
				&FOpenMobileHapticsTrackEditor::CreateTrackEditor
			)
		);
	}

	virtual void ShutdownModule() override
	{
		if (ISequencerModule* Sequencer =
			FModuleManager::GetModulePtr<ISequencerModule>(TEXT("Sequencer")))
		{
			Sequencer->UnRegisterTrackEditor(TrackEditorHandle);
		}
	}

private:
	FDelegateHandle TrackEditorHandle;
};

IMPLEMENT_MODULE(
	FOpenMobileHapticsSequencerEditorModule,
	OpenMobileHapticsSequencerEditor
)
