using UnrealBuildTool;

public class OpenMobileSensorsSampleHostTarget : TargetRules
{
	public OpenMobileSensorsSampleHostTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("OpenMobileSensorsSampleHost");
	}
}
