using UnrealBuildTool;

public class OpenMobileSensorsSensorsActivityProviderTarget : TargetRules
{
	public OpenMobileSensorsSensorsActivityProviderTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("OpenMobileSensorsSensorsActivityProvider");
	}
}
