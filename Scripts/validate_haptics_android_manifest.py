#!/usr/bin/env python3

import argparse
import sys
import xml.etree.ElementTree as ElementTree
from pathlib import Path


ANDROID_NAMESPACE = "http://schemas.android.com/apk/res/android"
VIBRATE_PERMISSION = "android.permission.VIBRATE"


class ManifestValidationError(ValueError):
	pass


def _permission_names(manifest: Path) -> list[str]:
	try:
		root = ElementTree.parse(manifest).getroot()
	except (OSError, ElementTree.ParseError) as error:
		raise ManifestValidationError(
			f"could not read Android manifest {manifest}: {error}"
		) from error
	name_attribute = f"{{{ANDROID_NAMESPACE}}}name"
	return [
		element.attrib.get(name_attribute, "")
		for element in root.findall("uses-permission")
	]


def haptics_manifest_signature(manifest: Path) -> tuple[str, ...]:
	return tuple(
		name for name in _permission_names(manifest)
		if name == VIBRATE_PERMISSION
	)


def validate_haptics_manifest(
	manifest: Path,
	*,
	custom_vibration_enabled: bool,
) -> None:
	permission_count = len(haptics_manifest_signature(manifest))
	if custom_vibration_enabled and permission_count == 0:
		raise ManifestValidationError(
			"custom-vibration manifest is missing android.permission.VIBRATE"
		)
	if custom_vibration_enabled and permission_count > 1:
		raise ManifestValidationError(
			"custom-vibration manifest contains duplicate VIBRATE permissions"
		)
	if not custom_vibration_enabled and permission_count > 0:
		raise ManifestValidationError(
			"semantic-only manifest contains an unexpected VIBRATE permission"
		)


def parse_arguments() -> argparse.Namespace:
	parser = argparse.ArgumentParser(
		description="Validate the OpenMobile Haptics Android permission contract."
	)
	parser.add_argument("manifest", type=Path)
	parser.add_argument(
		"--custom-vibration",
		choices=("enabled", "disabled"),
		required=True,
	)
	parser.add_argument(
		"--compare",
		type=Path,
		help="Require another manifest to have the same Haptics signature.",
	)
	return parser.parse_args()


def main() -> int:
	arguments = parse_arguments()
	try:
		validate_haptics_manifest(
			arguments.manifest,
			custom_vibration_enabled=
				arguments.custom_vibration == "enabled",
		)
		if arguments.compare is not None and haptics_manifest_signature(
			arguments.manifest
		) != haptics_manifest_signature(arguments.compare):
			raise ManifestValidationError(
				"unrelated plugin state changed the Haptics manifest signature"
			)
	except ManifestValidationError as error:
		print(error, file=sys.stderr)
		return 1
	print("Haptics Android manifest contract passed.")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
