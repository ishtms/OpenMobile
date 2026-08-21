using UnrealBuildTool;

public class OpenMobileMediaIOS : ModuleRules
{
	public OpenMobileMediaIOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bEnableObjCAutomaticReferenceCounting = true;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"ApplicationCore",
			"Core",
			"OpenMobileMedia"
		});

		PublicFrameworks.AddRange(new[]
		{
			"CoreGraphics",
			"ImageIO",
			"Photos",
			"PhotosUI",
			"UIKit",
			"UniformTypeIdentifiers"
		});
	}
}
