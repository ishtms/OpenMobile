using UnrealBuildTool;

public class OpenMobileAdsIOS : ModuleRules
{
	public OpenMobileAdsIOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bEnableObjCAutomaticReferenceCounting = true;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"OpenMobileAds"
		});

		PublicFrameworks.Add("AppTrackingTransparency");
	}
}
