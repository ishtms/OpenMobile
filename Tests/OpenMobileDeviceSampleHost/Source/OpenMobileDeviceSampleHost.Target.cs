using UnrealBuildTool;

public class OpenMobileDeviceSampleHostTarget : TargetRules
{
	public OpenMobileDeviceSampleHostTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("OpenMobileDeviceSampleHost");
	}
}
