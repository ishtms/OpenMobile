import json
import tempfile
import unittest
from pathlib import Path

from Scripts.run_haptics_device_validation import (
	DeviceValidationError,
	new_report,
	save_report,
)


class HapticsDeviceValidationTests(unittest.TestCase):
	def test_report_stays_redacted_and_requires_manual_observation(self) -> None:
		with tempfile.TemporaryDirectory() as directory:
			root = Path(directory)
			report = new_report()
			report["device"] = {
				"developerMode": "enabled",
				"model": "Fixture Phone",
				"osVersion": "1.0",
				"platform": "iOS",
			}
			secrets = {"PRIVATE-DEVICE-NAME", "TEAM123456"}
			save_report(root, report, secrets)
			written = json.loads((root / "report.json").read_text(encoding="utf-8"))
			self.assertEqual(18, len(written["scenarios"]))
			self.assertTrue(any(item["status"] == "pending" for item in written["scenarios"]))
			self.assertTrue(
				any(item["status"] == "not_applicable" for item in written["scenarios"])
			)
			serialized = json.dumps(written)
			self.assertTrue(all(secret not in serialized for secret in secrets))

			report["capabilitySnapshot"]["actual"] = "PRIVATE-DEVICE-NAME"
			with self.assertRaisesRegex(DeviceValidationError, "private"):
				save_report(root, report, secrets)


if __name__ == "__main__":
	unittest.main()
