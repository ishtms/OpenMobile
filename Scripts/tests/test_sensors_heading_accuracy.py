import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"


class SensorsHeadingAccuracyTests(unittest.TestCase):
	def test_stream_options_expose_an_opt_in_callback_threshold(self):
		options = (
			COMMON / "Public" / "OpenMobileSensorStreamOptions.h"
		).read_text(encoding="utf-8")
		self.assertIn("EOpenMobileSensorAccuracy MinimumCallbackAccuracy", options)
		self.assertIn("EOpenMobileSensorAccuracy::Unknown", options)
		service = (
			COMMON / "Private" / "OpenMobileSensorsSampleService.cpp"
		).read_text(encoding="utf-8")
		enqueue = service[
			service.index("bool EnqueueEventSample("):
			service.index("bool EnqueueBufferedSample(")
		]
		self.assertIn("MeetsMinimum", enqueue)
		self.assertIn("MinimumCallbackAccuracy", enqueue)

	def test_platform_estimates_remain_optional(self):
		android = (
			SENSORS
			/ "Source"
			/ "OpenMobileSensorsAndroid"
			/ "Private"
			/ "OpenMobileSensorsAndroidBackend.cpp"
		).read_text(encoding="utf-8")
		mapper = (
			COMMON / "Private" / "OpenMobileSensorAccuracyMapper.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("ValueAt(Index, 4) >= 0.0", android)
		self.assertIn("FMath::RadiansToDegrees", android)
		self.assertIn("AccuracyDegrees >= 0.0", mapper)
		self.assertIn("Snapshot.bHasEstimatedError = true", mapper)
		self.assertIn("Snapshot.Accuracy = EOpenMobileSensorAccuracy::Unreliable", mapper)

	def test_public_contract_defines_heading_quality_behavior(self):

		# Developer guides are optional in this source-only checkout.
		with self.subTest(scope="distributed documentation"):
			for document_path in (SENSORS / 'Docs' / 'PublicContract.md',):
				if not document_path.is_file():
					self.skipTest(f"Guide is not distributed: {document_path.name}")
			contract = (SENSORS / "Docs" / "PublicContract.md").read_text(
				encoding="utf-8"
			)
			for token in (
				"## Heading accuracy",
				"MinimumCallbackAccuracy",
				"latest-value polling",
				"attitude fusion quality",
				"calibration requirement",
				"30 seconds",
				"negative native sentinel",
				"does not invent",
			):
				self.assertIn(token, contract)


if __name__ == "__main__":
	unittest.main()
