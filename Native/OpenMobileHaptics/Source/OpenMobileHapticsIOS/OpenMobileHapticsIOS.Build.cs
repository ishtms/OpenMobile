using UnrealBuildTool;

public class OpenMobileHapticsIOS : ModuleRules
{
	/** Links Apple feedback and audio frameworks only where the Objective-C bridge is compiled. */
	public OpenMobileHapticsIOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileHaptics"
		});

		PublicFrameworks.AddRange(new[]
		{
			"AudioToolbox",
			"AVFoundation",
			"CoreHaptics",
			"Foundation",
			"UIKit"
		});
	}
}
