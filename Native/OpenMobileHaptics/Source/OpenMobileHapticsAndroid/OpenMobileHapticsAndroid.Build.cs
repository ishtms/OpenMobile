using System.IO;
using UnrealBuildTool;

public class OpenMobileHapticsAndroid : ModuleRules
{
	/** Adds JNI and the Android plugin receipt only to this platform module, the portable runtime doesn't need Launch. */
	public OpenMobileHapticsAndroid(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"Launch",
			"OpenMobileHaptics"
		});

		PrivateIncludePathModuleNames.Add("Launch");

		string ModulePath = Utils.MakePathRelativeTo(
			ModuleDirectory,
			Target.RelativeEnginePath
		);
		AdditionalPropertiesForReceipt.Add(
			"AndroidPlugin",
			Path.Combine(
				ModulePath,
				"Private/Android/OpenMobileHaptics_Android_UPL.xml"
			)
		);
	}
}
