using UnrealBuildTool;

public class OpenMobileDeviceSampleHostEditorTarget : TargetRules
{
	public OpenMobileDeviceSampleHostEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("OpenMobileDeviceSampleHost");
	}
}
