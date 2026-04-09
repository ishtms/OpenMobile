import json
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
ADS_PLUGIN = REPOSITORY_ROOT / "Services" / "OpenMobileAds"
ADMOB_PLUGIN = REPOSITORY_ROOT / "Providers" / "Ads" / "OpenMobileAdsAdMob"


def load_descriptor(plugin_root: Path) -> dict:
	with next(plugin_root.glob("*.uplugin")).open(encoding="utf-8") as descriptor_file:
		return json.load(descriptor_file)


class AdsPluginBoundaryTests(unittest.TestCase):
	def test_service_plugin_has_no_vendor_payload(self) -> None:
		text_suffixes = {".cs", ".cpp", ".h", ".ini", ".md", ".uplugin", ".xml"}
		for path in ADS_PLUGIN.rglob("*"):
			if not path.is_file() or path.suffix not in text_suffixes:
				continue
			self.assertNotIn("GoogleMobileAds", path.read_text(encoding="utf-8"))
			self.assertNotIn("play-services-ads", path.read_text(encoding="utf-8"))

		self.assertFalse((ADS_PLUGIN / "ThirdParty").exists())
		self.assertEqual([], list(ADS_PLUGIN.rglob("*_UPL.xml")))

	def test_admob_modules_match_their_platform_and_editor_boundaries(self) -> None:
		descriptor = load_descriptor(ADMOB_PLUGIN)
		modules = {module["Name"]: module for module in descriptor["Modules"]}

		self.assertEqual("Runtime", modules["OpenMobileAdsAdMob"]["Type"])
		self.assertNotIn("PlatformAllowList", modules["OpenMobileAdsAdMob"])
		self.assertEqual(["Android"], modules["OpenMobileAdsAdMobAndroid"]["PlatformAllowList"])
		self.assertEqual(["IOS"], modules["OpenMobileAdsAdMobIOS"]["PlatformAllowList"])
		self.assertEqual("Editor", modules["OpenMobileAdsAdMobEditor"]["Type"])

		module_root = ADMOB_PLUGIN / "Source" / "OpenMobileAdsAdMobEditor"
		self.assertTrue((module_root / "OpenMobileAdsAdMobEditor.Build.cs").is_file())
		self.assertTrue((module_root / "Private" / "OpenMobileAdsAdMobEditorModule.cpp").is_file())

	def test_admob_declares_service_dependency_without_reverse_dependency(self) -> None:
		service_dependencies = {
			plugin["Name"] for plugin in load_descriptor(ADS_PLUGIN).get("Plugins", [])
		}
		provider_dependencies = {
			plugin["Name"] for plugin in load_descriptor(ADMOB_PLUGIN).get("Plugins", [])
		}

		self.assertNotIn("OpenMobileAdsAdMob", service_dependencies)
		self.assertIn("OpenMobileAds", provider_dependencies)
		self.assertIn("OpenMobileCore", provider_dependencies)

	def test_native_payload_is_owned_by_a_provider_plugin(self) -> None:
		providers_root = REPOSITORY_ROOT / "Providers" / "Ads"
		payload_paths = [
			path
			for path in providers_root.rglob("*")
			if path.is_file()
			and (
				path.name.endswith("_UPL.xml")
				or path.suffix in {".aar", ".framework", ".zip"}
			)
		]
		self.assertNotEqual([], payload_paths)

		for payload_path in payload_paths:
			owner = next(
				(
					parent
					for parent in payload_path.parents
					if list(parent.glob("*.uplugin"))
				),
				None,
			)
			self.assertIsNotNone(owner, f"{payload_path} has no provider descriptor")
			self.assertEqual(ADMOB_PLUGIN, owner)


if __name__ == "__main__":
	unittest.main()
