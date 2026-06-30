import csv
import tempfile
import unittest
from pathlib import Path

from Scripts.validate_device_release_matrix import MatrixValidationError, validate_matrix


FIELDNAMES = (
	"date",
	"platform",
	"model_class",
	"os_version",
	"environment",
	"scenario",
	"capability_result",
	"outcome",
	"evidence",
	"sanitized_details",
)


class DeviceReleaseMatrixTests(unittest.TestCase):
	def write_matrix(self, rows: list[dict[str, str]]) -> Path:
		temporary = tempfile.NamedTemporaryFile(
			mode="w",
			encoding="utf-8",
			newline="",
			delete=False,
		)
		with temporary:
			writer = csv.DictWriter(temporary, fieldnames=FIELDNAMES)
			writer.writeheader()
			writer.writerows(rows)
		return Path(temporary.name)

	def test_accepts_sanitized_pass_and_unavailable_rows(self) -> None:
		path = self.write_matrix(
			[
				{
					"date": "2026-08-22",
					"platform": "iOS",
					"model_class": "current iPhone",
					"os_version": "26.5.2",
					"environment": "physical",
					"scenario": "cold_start",
					"capability_result": "Available",
					"outcome": "Pass",
					"evidence": "signed validation host launch log",
					"sanitized_details": "",
				},
				{
					"date": "2026-08-22",
					"platform": "Android",
					"model_class": "foldable",
					"os_version": "unavailable",
					"environment": "physical",
					"scenario": "foldable_posture",
					"capability_result": "Not tested",
					"outcome": "Not run",
					"evidence": "no compatible device in local inventory",
					"sanitized_details": "hardware unavailable",
				},
			]
		)
		self.addCleanup(path.unlink, missing_ok=True)

		validate_matrix(path)

	def test_rejects_missing_required_column(self) -> None:
		path = self.write_matrix([])
		contents = path.read_text(encoding="utf-8").replace(
			",sanitized_details",
			"",
		)
		path.write_text(contents, encoding="utf-8")
		self.addCleanup(path.unlink, missing_ok=True)

		with self.assertRaisesRegex(MatrixValidationError, "missing columns"):
			validate_matrix(path)

	def test_rejects_raw_identifiers_and_urls(self) -> None:
		path = self.write_matrix(
			[
				{
					"date": "2026-08-22",
					"platform": "iOS",
					"model_class": "iPhone",
					"os_version": "26.5.2",
					"environment": "physical",
					"scenario": "network_handoff",
					"capability_result": "Available",
					"outcome": "Fail",
					"evidence": "device ABCDEF12-1234567890ABCDEF",
					"sanitized_details": "request to https://example.com failed",
				},
			]
		)
		self.addCleanup(path.unlink, missing_ok=True)

		with self.assertRaisesRegex(MatrixValidationError, "unsanitized"):
			validate_matrix(path)


if __name__ == "__main__":
	unittest.main()
