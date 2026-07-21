using UnrealBuildTool;

public class OpenMobileSensorsSensorsOnlyTarget : TargetRules
{
	public OpenMobileSensorsSensorsOnlyTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("OpenMobileSensorsSensorsOnly");
	}
}
