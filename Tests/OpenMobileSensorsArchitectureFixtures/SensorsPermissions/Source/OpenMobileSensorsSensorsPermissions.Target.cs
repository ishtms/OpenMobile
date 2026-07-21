using UnrealBuildTool;

public class OpenMobileSensorsSensorsPermissionsTarget : TargetRules
{
	public OpenMobileSensorsSensorsPermissionsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("OpenMobileSensorsSensorsPermissions");
	}
}
