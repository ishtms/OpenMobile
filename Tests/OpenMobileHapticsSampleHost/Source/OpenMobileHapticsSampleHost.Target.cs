using UnrealBuildTool;

public class OpenMobileHapticsSampleHostTarget : TargetRules
{
	public OpenMobileHapticsSampleHostTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("OpenMobileHapticsSampleHost");
	}
}
