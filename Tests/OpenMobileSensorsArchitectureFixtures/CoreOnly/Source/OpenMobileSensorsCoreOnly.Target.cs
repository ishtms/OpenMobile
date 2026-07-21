using UnrealBuildTool;

public class OpenMobileSensorsCoreOnlyTarget : TargetRules
{
	public OpenMobileSensorsCoreOnlyTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("OpenMobileSensorsCoreOnly");
	}
}
