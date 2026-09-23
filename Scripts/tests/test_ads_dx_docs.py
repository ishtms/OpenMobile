from pathlib import Path
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


class AdsDxDocumentationTests(unittest.TestCase):
	def test_blueprint_first_run_flow_is_consistent(self) -> None:

		# Developer guides are optional in this source-only checkout.
		with self.subTest(scope="distributed documentation"):
			for document_path in (REPOSITORY_ROOT / 'README.md', REPOSITORY_ROOT / 'Services/OpenMobileAds/README.md', REPOSITORY_ROOT / 'Providers/Ads/OpenMobileAdsAdMob/README.md', REPOSITORY_ROOT / 'Docs/MIGRATION_0.1.md', REPOSITORY_ROOT / 'Examples/OpenMobileAds/BlueprintFirstAd.md',):
				if not document_path.is_file():
					self.skipTest(f"Guide is not distributed: {document_path.name}")
			root_readme = (REPOSITORY_ROOT / "README.md").read_text(encoding="utf-8")
			service_readme = (
				REPOSITORY_ROOT / "Services/OpenMobileAds/README.md"
			).read_text(encoding="utf-8")
			provider_readme = (
				REPOSITORY_ROOT / "Providers/Ads/OpenMobileAdsAdMob/README.md"
			).read_text(encoding="utf-8")
			migration = (
				REPOSITORY_ROOT / "Docs/MIGRATION_0.1.md"
			).read_text(encoding="utf-8")
			example = (
				REPOSITORY_ROOT / "Examples/OpenMobileAds/BlueprintFirstAd.md"
			).read_text(encoding="utf-8")
			for document in (
				root_readme,
				service_readme,
				provider_readme,
				migration,
				example,
			):
				for step in (
					"Refresh Ads Consent Async",
					"Request iOS Tracking Authorization Async",
					"Initialize Ads Async",
					"Load Ad Async",
					"Show Rewarded Ad Async",
				):
					self.assertIn(step, document)
			self.assertIn("## First test ad in Blueprint", root_readme)
			self.assertNotIn("rewarded-ad API", root_readme)
			self.assertNotIn("consent UI is not yet exposed", root_readme)

if __name__ == "__main__":
	unittest.main()
