using UnrealBuildTool;

public class OpenMobileHapticsSampleHostEditorTarget : TargetRules
{
	public OpenMobileHapticsSampleHostEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("OpenMobileHapticsSampleHost");
	}
}
