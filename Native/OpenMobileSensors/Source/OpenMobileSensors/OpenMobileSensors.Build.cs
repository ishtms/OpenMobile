using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;
using UnrealBuildTool;

public class OpenMobileSensors : ModuleRules
{
	public OpenMobileSensors(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		ValidatePackaging(Target);

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"DeveloperSettings",
			"Engine",
			"OpenMobileCore",
			"OpenMobilePermissions"
		});

		PrivateDependencyModuleNames.Add("Projects");
	}

	private void ValidatePackaging(ReadOnlyTargetRules Target)
	{
		if (Target.ProjectFile == null)
		{
			return;
		}

		ConfigHierarchy Config = ConfigCache.ReadHierarchy(
			ConfigHierarchyType.Engine,
			Target.ProjectFile.Directory,
			Target.Platform
		);
		const string SensorsSection =
			"/Script/OpenMobileSensors.OpenMobileSensorsSettings";
		string DevelopmentInputMode;
		Config.GetString(
			SensorsSection,
			"DevelopmentInputMode",
			out DevelopmentInputMode
		);
		if (Target.Configuration == UnrealTargetConfiguration.Shipping
			&& !IsDevelopmentInputDisabled(DevelopmentInputMode))
		{
			throw new BuildException(
				"OpenMobile Sensors DevelopmentInputMode must be Disabled for Shipping targets."
			);
		}

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			ValidateAndroidPackaging(Config, SensorsSection, Target.ProjectFile.Directory);
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			ValidateIOSPackaging(Target, Config, SensorsSection);
		}
	}

	private void ValidateAndroidPackaging(
		ConfigHierarchy Config,
		string SensorsSection,
		DirectoryReference ProjectDirectory
	)
	{
		bool PermissionSensitiveEnabled = false;
		bool HighSamplingEnabled = false;
		Config.GetBool(
			SensorsSection,
			"bEnablePermissionSensitiveSensors",
			out PermissionSensitiveEnabled
		);
		Config.GetBool(
			SensorsSection,
			"bAllowHighSamplingRate",
			out HighSamplingEnabled
		);
		string ActivityRationale;
		Config.GetString(
			SensorsSection,
			"AndroidActivityRecognitionRationale",
			out ActivityRationale
		);
		if (PermissionSensitiveEnabled
			&& string.IsNullOrWhiteSpace(ActivityRationale))
		{
			throw new BuildException(
				"OpenMobile Sensors AndroidActivityRecognitionRationale is required when permission-sensitive Sensors features are enabled."
			);
		}

		string UPLPath = Path.Combine(
			PluginDirectory,
			"Source",
			"OpenMobileSensorsAndroid",
			"Private",
			"Android",
			"OpenMobileSensors_Android_UPL.xml"
		);
		RequirePackagingTokens(
			UPLPath,
			new[]
			{
				"bEnablePermissionSensitiveSensors",
				"android.permission.ACTIVITY_RECOGNITION",
				"bAllowHighSamplingRate",
				"android.permission.HIGH_SAMPLING_RATE_SENSORS"
			}
		);

		List<string> HostDeclarations = new List<string>();
		const string AndroidSection =
			"/Script/AndroidRuntimeSettings.AndroidRuntimeSettings";
		List<string> ExtraPermissions;
		if (Config.GetArray(AndroidSection, "ExtraPermissions", out ExtraPermissions))
		{
			HostDeclarations.AddRange(ExtraPermissions);
		}
		foreach (string FileName in new[]
		{
			"ManifestRequirementsAdditions.txt",
			"ManifestRequirementsOverride.txt"
		})
		{
			string FilePath = Path.Combine(
				ProjectDirectory.FullName,
				"Build",
				"Android",
				FileName
			);
			if (File.Exists(FilePath))
			{
				HostDeclarations.Add(File.ReadAllText(FilePath));
			}
		}

		RejectAndroidHostDeclaration(
			HostDeclarations,
			"android.permission.ACTIVITY_RECOGNITION",
			PermissionSensitiveEnabled
		);
		RejectAndroidHostDeclaration(
			HostDeclarations,
			"android.permission.HIGH_SAMPLING_RATE_SENSORS",
			HighSamplingEnabled
		);
	}

	private void ValidateIOSPackaging(
		ReadOnlyTargetRules Target,
		ConfigHierarchy Config,
		string SensorsSection
	)
	{
		string MotionUsageDescription;
		if (!Config.GetString(
			SensorsSection,
			"IOSMotionUsageDescription",
			out MotionUsageDescription
			))
		{
			MotionUsageDescription =
				"This app uses motion sensors for gameplay features.";
		}
		MotionUsageDescription = MotionUsageDescription.Trim();
		if (MotionUsageDescription.Length == 0)
		{
			throw new BuildException(
				"OpenMobile Sensors IOSMotionUsageDescription is required for iOS packaging."
			);
		}

		string UPLPath = Path.Combine(
			PluginDirectory,
			"Source",
			"OpenMobileSensorsIOS",
			"Private",
			"IOS",
			"OpenMobileSensors_IOS_UPL.xml"
		);
		RequirePackagingTokens(
			UPLPath,
			new[] { "IOSMotionUsageDescription", "NSMotionUsageDescription" }
		);

		string AdditionalPlistData;
		Config.GetString(
			"/Script/IOSRuntimeSettings.IOSRuntimeSettings",
			"AdditionalPlistData",
			out AdditionalPlistData
		);
		string HostPlistData = AdditionalPlistData ?? string.Empty;
		MatchCollection HostMotionKeys = Regex.Matches(
			HostPlistData,
			"<key>\\s*NSMotionUsageDescription\\s*</key>",
			RegexOptions.Singleline
		);
		MatchCollection HostMotionUsages = Regex.Matches(
			HostPlistData,
			"<key>\\s*NSMotionUsageDescription\\s*</key>\\s*<string>(.*?)</string>",
			RegexOptions.Singleline
		);
		if (HostMotionKeys.Count > 1)
		{
			throw new BuildException(
				"The host declares NSMotionUsageDescription more than once. Keep one value that matches OpenMobile Sensors IOSMotionUsageDescription."
			);
		}

		bool RequiresHostFallback =
			Target.Version.MajorVersion == 5
			&& Target.Version.MinorVersion == 8;
		if (HostMotionKeys.Count == 1
			&& HostMotionUsages.Count == 1)
		{
			string HostValue = DecodeXMLText(
				HostMotionUsages[0].Groups[1].Value
			).Trim();
			if (!string.Equals(
				HostValue,
				MotionUsageDescription,
				StringComparison.Ordinal
			))
			{
				throw new BuildException(
					"The host NSMotionUsageDescription differs from OpenMobile Sensors IOSMotionUsageDescription. Keep both values identical."
				);
			}
			if (RequiresHostFallback)
			{
				return;
			}
			throw new BuildException(
				"Remove the duplicate host NSMotionUsageDescription. OpenMobile Sensors owns this key."
			);
		}
		if (HostMotionKeys.Count == 1
			|| HostPlistData.Contains(
			"NSMotionUsageDescription",
			StringComparison.Ordinal
			))
		{
			throw new BuildException(
				RequiresHostFallback
					? "The UE 5.8 host NSMotionUsageDescription fallback is malformed. Set iOS AdditionalPlistData to one string matching OpenMobile Sensors IOSMotionUsageDescription."
					: "The host NSMotionUsageDescription is malformed. Remove it and configure IOSMotionUsageDescription under OpenMobile Sensors."
			);
		}
		if (RequiresHostFallback)
		{
			throw new BuildException(
				"UE 5.8 modern Xcode packaging ignores iOS UPL plist updates. Add NSMotionUsageDescription to iOS AdditionalPlistData with the exact OpenMobile Sensors IOSMotionUsageDescription value."
			);
		}
	}

	private static bool IsDevelopmentInputDisabled(string Value)
	{
		if (string.IsNullOrWhiteSpace(Value))
		{
			return true;
		}
		string Normalized = Value.Trim();
		return Normalized == "0"
			|| Normalized.Equals("Disabled", StringComparison.OrdinalIgnoreCase)
			|| Normalized.EndsWith("::Disabled", StringComparison.OrdinalIgnoreCase);
	}

	private static void RejectAndroidHostDeclaration(
		IEnumerable<string> HostDeclarations,
		string Permission,
		bool CanonicalEnabled
	)
	{
		foreach (string Declaration in HostDeclarations)
		{
			if (Declaration != null
				&& Declaration.Contains(Permission, StringComparison.Ordinal))
			{
				string Detail = CanonicalEnabled
					? "duplicates the enabled Sensors packaging path"
					: "contradicts the disabled Sensors project setting";
				throw new BuildException(
					"Host Android declaration {0} {1}. Remove the host declaration and use OpenMobile Sensors settings.",
					Permission,
					Detail
				);
			}
		}
	}

	private static void RequirePackagingTokens(
		string PathName,
		IEnumerable<string> RequiredTokens
	)
	{
		if (!File.Exists(PathName))
		{
			throw new BuildException(
				"OpenMobile Sensors packaging file is missing: {0}",
				PathName
			);
		}
		string Contents = File.ReadAllText(PathName);
		foreach (string Token in RequiredTokens)
		{
			if (!Contents.Contains(Token, StringComparison.Ordinal))
			{
				throw new BuildException(
					"OpenMobile Sensors packaging file {0} is missing {1}.",
					PathName,
					Token
				);
			}
		}
	}

	private static string DecodeXMLText(string Value)
	{
		return Value
			.Replace("&quot;", "\"")
			.Replace("&apos;", "'")
			.Replace("&lt;", "<")
			.Replace("&gt;", ">")
			.Replace("&amp;", "&");
	}
}
