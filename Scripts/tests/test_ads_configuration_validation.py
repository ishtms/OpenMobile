import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT / "Scripts"))

from validate_ads_plugins import (
	ArtifactExpectation,
	PluginDescriptor,
	inspect_artifact,
	resolve_configuration,
	validate_artifact,
)


def repository_descriptor(relative_path: str) -> PluginDescriptor:
	path = REPOSITORY_ROOT / relative_path
	return PluginDescriptor.load(path)


class AdsConfigurationValidationTests(unittest.TestCase):
	def setUp(self) -> None:
		self.core = repository_descriptor("Foundation/OpenMobileCore/OpenMobileCore.uplugin")
		self.service = repository_descriptor("Services/OpenMobileAds/OpenMobileAds.uplugin")
		self.admob = repository_descriptor(
			"Providers/Ads/OpenMobileAdsAdMob/OpenMobileAdsAdMob.uplugin"
		)
		mock_data = {
			"Modules": [
				{
					"Name": "OpenMobileAdsMock",
					"Type": "DeveloperTool",
					"LoadingPhase": "Default",
				}
			],
			"Plugins": [
				{"Name": "OpenMobileCore", "Enabled": True},
				{"Name": "OpenMobileAds", "Enabled": True},
			],
		}
		self.mock = PluginDescriptor("OpenMobileAdsMock", Path("mock.uplugin"), mock_data)
		adapter_data = {
			"OpenMobileAdsType": "MediationAdapter",
			"Modules": [
				{
					"Name": "OpenMobileAdsMockAdapter",
					"Type": "Runtime",
					"LoadingPhase": "Default",
				}
			],
			"Plugins": [
				{"Name": "OpenMobileCore", "Enabled": True},
				{"Name": "OpenMobileAds", "Enabled": True},
				{"Name": "OpenMobileAdsMock", "Enabled": True},
			],
		}
		self.mock_adapter = PluginDescriptor(
			"OpenMobileAdsMockAdapter",
			Path("mock-adapter.uplugin"),
			adapter_data,
		)
		self.descriptors = {
			descriptor.name: descriptor
			for descriptor in (
				self.core,
				self.service,
				self.admob,
				self.mock,
				self.mock_adapter,
			)
		}

	def test_service_only_configuration_has_no_provider_payload(self) -> None:
		configuration = resolve_configuration(
			self.descriptors,
			["OpenMobileAds"],
			platform="Android",
			target_type="Game",
		)

		self.assertEqual({"OpenMobileAds", "OpenMobileCore"}, configuration.plugins)
		self.assertEqual({"OpenMobileAds", "OpenMobileCore"}, configuration.modules)
		self.assertEqual(set(), configuration.ads_providers)
		self.assertEqual(set(), configuration.ads_adapters)

	def test_single_provider_configuration_selects_one_platform_module(self) -> None:
		configuration = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsAdMob"],
			platform="Android",
			target_type="Game",
		)

		self.assertEqual({"OpenMobileAdsAdMob"}, configuration.ads_providers)
		self.assertIn("OpenMobileAdsAdMobAndroid", configuration.modules)
		self.assertNotIn("OpenMobileAdsAdMobIOS", configuration.modules)
		self.assertNotIn("OpenMobileAdsAdMobEditor", configuration.modules)
		self.assertEqual(set(), configuration.ads_adapters)

	def test_multi_provider_configuration_keeps_providers_independent(self) -> None:
		configuration = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsAdMob", "OpenMobileAdsMock"],
			platform="Mac",
			target_type="Editor",
		)

		self.assertEqual(
			{"OpenMobileAdsAdMob", "OpenMobileAdsMock"},
			configuration.ads_providers,
		)
		self.assertNotIn("OpenMobileAdsAdMob", self.mock.dependencies)

	def test_mediation_adapter_is_opt_in(self) -> None:
		provider_only = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsMock"],
			platform="Android",
			target_type="Game",
		)
		self.assertEqual(set(), provider_only.ads_adapters)
		self.assertNotIn("OpenMobileAdsMockAdapter", provider_only.modules)

		with_adapter = resolve_configuration(
			self.descriptors,
			["OpenMobileAdsMockAdapter"],
			platform="Android",
			target_type="Game",
		)
		self.assertEqual({"OpenMobileAdsMock"}, with_adapter.ads_providers)
		self.assertEqual({"OpenMobileAdsMockAdapter"}, with_adapter.ads_adapters)
		self.assertIn("OpenMobileAdsMockAdapter", with_adapter.modules)

	def test_artifact_validation_checks_provider_payload_isolation(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			provider_artifact = Path(temporary_directory) / "provider-app.zip"
			with zipfile.ZipFile(provider_artifact, "w") as archive:
				archive.writestr("Payload/Game.app/Frameworks/GoogleMobileAds.framework/GoogleMobileAds", b"sdk")

			provider_inventory = inspect_artifact(provider_artifact)
			self.assertEqual(
				[],
				validate_artifact(
					provider_inventory,
					ArtifactExpectation(required_providers={"OpenMobileAdsAdMob"}),
				),
			)
			self.assertNotEqual(
				[],
				validate_artifact(
					provider_inventory,
					ArtifactExpectation(forbidden_providers={"OpenMobileAdsAdMob"}),
				),
			)

			service_artifact = Path(temporary_directory) / "service-app.zip"
			with zipfile.ZipFile(service_artifact, "w") as archive:
				archive.writestr("Payload/Game.app/Game", b"game")
			self.assertEqual(
				[],
				validate_artifact(
					inspect_artifact(service_artifact),
					ArtifactExpectation(forbidden_providers={"OpenMobileAdsAdMob"}),
				),
			)

	def test_multi_provider_and_optional_adapter_artifacts(self) -> None:
		with tempfile.TemporaryDirectory() as temporary_directory:
			multi_artifact = Path(temporary_directory) / "multi-provider-app.zip"
			with zipfile.ZipFile(multi_artifact, "w") as archive:
				archive.writestr(
					"Payload/Game.app/Frameworks/GoogleMobileAds.framework/GoogleMobileAds",
					b"sdk",
				)
				archive.writestr(
					"Payload/Game.app/Game.modules",
					b"openmobileadsmockpayload",
				)
			inventory = inspect_artifact(multi_artifact)
			self.assertEqual(
				[],
				validate_artifact(
					inventory,
					ArtifactExpectation(
						required_providers={"OpenMobileAdsAdMob", "OpenMobileAdsMock"},
						forbidden_adapters={"OpenMobileAdsMockAdapter"},
					),
				),
			)

			adapter_artifact = Path(temporary_directory) / "adapter-app.zip"
			with zipfile.ZipFile(adapter_artifact, "w") as archive:
				archive.writestr(
					"Payload/Game.app/Game.modules",
					b"openmobileadsmockpayload openmobileadsmockadapterpayload",
				)
			self.assertEqual(
				[],
				validate_artifact(
					inspect_artifact(adapter_artifact),
					ArtifactExpectation(
						required_providers={"OpenMobileAdsMock"},
						required_adapters={"OpenMobileAdsMockAdapter"},
					),
				),
			)


if __name__ == "__main__":
	unittest.main()
