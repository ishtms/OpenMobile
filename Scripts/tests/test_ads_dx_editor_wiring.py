from pathlib import Path
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


class AdsDxEditorWiringTests(unittest.TestCase):
	def test_editor_wires_live_blueprint_and_settings_guidance(self) -> None:
		core_editor = (
			REPOSITORY_ROOT
			/ "Services/OpenMobileAds/Source/OpenMobileAdsEditor/Private/OpenMobileAdsEditorModule.cpp"
		).read_text(encoding="utf-8")
		provider_editor = (
			REPOSITORY_ROOT
			/ "Providers/Ads/OpenMobileAdsAdMob/Source/OpenMobileAdsAdMobEditor/Private/OpenMobileAdsAdMobEditorModule.cpp"
		).read_text(encoding="utf-8")

		self.assertIn("RegisterCompilerExtension", core_editor)
		self.assertIn("RegisterCustomPropertyTypeLayout", core_editor)
		self.assertIn("FOpenMobileAdsPlacementSettings", core_editor)
		self.assertIn("Automatic provider:", core_editor)
		self.assertIn("RegisterCustomClassLayout", provider_editor)
		self.assertIn("Additional Plist Data", provider_editor)
		self.assertIn("Development/Test Mode", provider_editor)


if __name__ == "__main__":
	unittest.main()
