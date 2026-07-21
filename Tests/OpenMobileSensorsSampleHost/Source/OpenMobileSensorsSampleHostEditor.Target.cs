using UnrealBuildTool;

public class OpenMobileSensorsSampleHostEditorTarget : TargetRules
{
	public OpenMobileSensorsSampleHostEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("OpenMobileSensorsSampleHost");
	}
}
