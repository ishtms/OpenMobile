using UnrealBuildTool;

public class OpenMobileHapticsSequencerEditor : ModuleRules
{
	public OpenMobileHapticsSequencerEditor(ReadOnlyTargetRules Target)
		: base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"MovieScene",
			"MovieSceneTools",
			"OpenMobileHapticsSequencer",
			"Sequencer",
			"Slate",
			"SlateCore",
			"UnrealEd"
		});
	}
}
