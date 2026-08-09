import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
PLUGIN = ROOT / "Native" / "OpenMobileSensors"


class SensorsDocumentationTests(unittest.TestCase):
	def test_complete_developer_guides_exist(self):
		for relative_path in (
			"README.md",
			"CHANGELOG.md",
			"Docs/BlueprintQuickStart.md",
			"Docs/SensorReference.md",
			"Docs/PlatformSupport.md",
			"Docs/Packaging.md",
			"Docs/Migration.md",
			"Docs/ArchitectureValidation.md",
			"Docs/PublicContract.md",
			"Docs/DeviceValidation.md",
		):
			self.assertTrue((PLUGIN / relative_path).is_file(), relative_path)

	def test_readme_links_every_distributed_guide(self):
		readme = (PLUGIN / "README.md").read_text(encoding="utf-8")
		for relative_path in (
			"Docs/BlueprintQuickStart.md",
			"Docs/SensorReference.md",
			"Docs/PlatformSupport.md",
			"Docs/Packaging.md",
			"Docs/Migration.md",
			"Docs/ArchitectureValidation.md",
			"Docs/PublicContract.md",
			"Docs/DeviceValidation.md",
			"CHANGELOG.md",
		):
			self.assertIn(f"]({relative_path})", readme, relative_path)

	def test_plugin_package_filter_includes_guides_and_recorded_fixture(self):
		package_filter = (PLUGIN / "Config" / "FilterPlugin.ini").read_text(
			encoding="utf-8"
		)
		for entry in (
			"/README.md",
			"/CHANGELOG.md",
			"/Docs/...",
			"/Tests/Fixtures/ShakeLinearAcceleration.csv",
		):
			self.assertIn(entry, package_filter)

	def test_blueprint_guide_covers_complete_workflows(self):
		guide = (PLUGIN / "Docs" / "BlueprintQuickStart.md").read_text(
			encoding="utf-8"
		)
		for topic in (
			"Accelerometer",
			"Gyroscope",
			"Attitude",
			"Magnetic Heading",
			"True Heading",
			"Step Count",
			"Permission Required",
			"Expected values",
			"Failure paths",
			"Cleanup",
			"Recording lifecycle",
			"Replay lifecycle",
		):
			self.assertIn(topic, guide)


if __name__ == "__main__":
	unittest.main()
