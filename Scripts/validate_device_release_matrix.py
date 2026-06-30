#!/usr/bin/env python3

import argparse
import csv
import re
import sys
from datetime import date
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MATRIX = (
	REPOSITORY_ROOT / "Native" / "OpenMobileDevice" / "DEVICE_RELEASE_MATRIX.csv"
)
REQUIRED_COLUMNS = (
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
ALLOWED_VALUES = {
	"platform": {"Android", "iOS"},
	"environment": {"physical", "emulator", "simulator"},
	"capability_result": {
		"Available",
		"Unavailable",
		"Restricted",
		"Not configured",
		"Not tested",
		"Varies",
	},
	"outcome": {"Pass", "Fail", "Blocked", "Not run", "Not applicable"},
}
UNSANITIZED_PATTERNS = (
	re.compile(r"\b[0-9a-f]{8}-[0-9a-f]{16}\b", re.IGNORECASE),
	re.compile(
		r"\b[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\b",
		re.IGNORECASE,
	),
	re.compile(r"\b[0-9a-f]{24,}\b", re.IGNORECASE),
	re.compile(r"\bhttps?://", re.IGNORECASE),
	re.compile(r"\b[^\s@]+@[^\s@]+\b"),
	re.compile(r"\b(?:\d{1,3}\.){3}\d{1,3}\b"),
)


class MatrixValidationError(ValueError):
	pass


def validate_row(row: dict[str, str], row_number: int) -> None:
	for field in REQUIRED_COLUMNS:
		if field != "sanitized_details" and not row[field].strip():
			raise MatrixValidationError(f"row {row_number} has empty {field}")

	try:
		date.fromisoformat(row["date"])
	except ValueError as error:
		raise MatrixValidationError(
			f"row {row_number} has invalid date"
		) from error

	for field, allowed in ALLOWED_VALUES.items():
		if row[field] not in allowed:
			raise MatrixValidationError(
				f"row {row_number} has invalid {field}: {row[field]}"
			)

	if row["outcome"] == "Pass" and row["capability_result"] == "Not tested":
		raise MatrixValidationError(
			f"row {row_number} cannot pass an untested capability"
		)
	if row["outcome"] == "Not run" and row["capability_result"] != "Not tested":
		raise MatrixValidationError(
			f"row {row_number} must mark an unrun capability as Not tested"
		)

	for field in ("model_class", "evidence", "sanitized_details"):
		value = row[field]
		if len(value) > 240 or any(pattern.search(value) for pattern in UNSANITIZED_PATTERNS):
			raise MatrixValidationError(
				f"row {row_number} contains unsanitized {field}"
			)


def validate_matrix(path: Path) -> int:
	with path.open(encoding="utf-8", newline="") as matrix_file:
		reader = csv.DictReader(matrix_file)
		columns = tuple(reader.fieldnames or ())
		missing = [field for field in REQUIRED_COLUMNS if field not in columns]
		if missing:
			raise MatrixValidationError(
				"missing columns: " + ", ".join(missing)
			)

		row_count = 0
		for row_count, row in enumerate(reader, start=1):
			validate_row(row, row_count + 1)
	if row_count == 0:
		raise MatrixValidationError("matrix has no evidence rows")
	return row_count


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("matrix", type=Path, nargs="?", default=DEFAULT_MATRIX)
	arguments = parser.parse_args()
	try:
		row_count = validate_matrix(arguments.matrix.resolve())
	except (OSError, MatrixValidationError) as error:
		print(f"Device release matrix validation failed: {error}", file=sys.stderr)
		return 1
	print(f"OpenMobile Device release matrix passed ({row_count} rows).")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
