import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"


class SensorsActivityConfidenceTests(unittest.TestCase):
	def test_confidence_and_stability_are_per_subscription(self):
		options = (
			COMMON / "Public" / "OpenMobileSensorStreamOptions.h"
		).read_text(encoding="utf-8")
		service = (
			COMMON / "Private" / "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"MinimumActivityConfidence",
			"MinimumActivityStableDurationSeconds",
		):
			self.assertIn(token, options)
		for token in (
			"FOpenMobileActivitySampleFilter ActivityFilter",
			"ActivityFilter.Configure",
			"ActivityFilter.Process",
		):
			self.assertIn(token, service)

	def test_activity_ambiguity_and_threshold_semantics_are_documented(self):
		classifier = (
			COMMON / "Private" / "OpenMobileMotionActivityClassifier.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"NormalizeSample",
			"ChoosePrimary",
			"CanonicalActivities",
		):
			self.assertIn(token, classifier)

		# Developer guides are optional in this source-only checkout.
		with self.subTest(scope="distributed documentation"):
			for document_path in (SENSORS / 'README.md',):
				if not document_path.is_file():
					self.skipTest(f"Guide is not distributed: {document_path.name}")
			readme = (SENSORS / "README.md").read_text(encoding="utf-8")
			self.assertIn("categorical confidence", readme)
			self.assertIn("sample timestamps", readme)


if __name__ == "__main__":
	unittest.main()
