using UnrealBuildTool;

public class OpenMobileDevice : ModuleRules
{
	public OpenMobileDevice(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"OpenMobileCore"
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"HTTP",
			"Sockets"
		});
		bool bEndpointCurl = Target.Platform == UnrealTargetPlatform.Win64
			|| Target.Platform == UnrealTargetPlatform.Android
			|| Target.Platform == UnrealTargetPlatform.Linux
			|| Target.Platform == UnrealTargetPlatform.Mac;
		PrivateDefinitions.Add("OPENMOBILE_ENDPOINT_CURL=" + (bEndpointCurl ? "1" : "0"));
		if (bEndpointCurl)
		{
			PrivateDependencyModuleNames.Add("SSL");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl", "OpenSSL", "nghttp2", "zlib");
		}
	}
}
