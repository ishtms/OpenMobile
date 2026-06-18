import sys
import tempfile
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY_ROOT / "Scripts"))

from validate_haptics_android_manifest import (
	ManifestValidationError,
	haptics_manifest_signature,
	validate_haptics_manifest,
)


def write_manifest(root: Path, permissions: list[str]) -> Path:
	permission_xml = "".join(
		f'<uses-permission android:name="{permission}" />'
		for permission in permissions
	)
	path = root / "AndroidManifest.xml"
	path.write_text(
		f'''<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android">
{permission_xml}
<application android:label="Fixture" />
</manifest>
''',
		encoding="utf-8",
	)
	return path


class HapticsAndroidManifestTests(unittest.TestCase):
	def test_custom_vibration_requires_exactly_one_permission(self) -> None:
		with tempfile.TemporaryDirectory() as directory:
			root = Path(directory)
			valid = write_manifest(root, ["android.permission.VIBRATE"])
			validate_haptics_manifest(valid, custom_vibration_enabled=True)

			missing = write_manifest(root, [])
			with self.assertRaisesRegex(ManifestValidationError, "missing"):
				validate_haptics_manifest(
					missing,
					custom_vibration_enabled=True,
				)

			duplicate = write_manifest(
				root,
				["android.permission.VIBRATE", "android.permission.VIBRATE"],
			)
			with self.assertRaisesRegex(ManifestValidationError, "duplicate"):
				validate_haptics_manifest(
					duplicate,
					custom_vibration_enabled=True,
				)

	def test_semantic_only_manifest_omits_vibration_permission(self) -> None:
		with tempfile.TemporaryDirectory() as directory:
			root = Path(directory)
			semantic_only = write_manifest(root, [])
			validate_haptics_manifest(
				semantic_only,
				custom_vibration_enabled=False,
			)

			unexpected = write_manifest(root, ["android.permission.VIBRATE"])
			with self.assertRaisesRegex(ManifestValidationError, "unexpected"):
				validate_haptics_manifest(
					unexpected,
					custom_vibration_enabled=False,
				)

	def test_unrelated_permissions_do_not_change_haptics_signature(self) -> None:
		with tempfile.TemporaryDirectory() as directory:
			root = Path(directory)
			minimal = write_manifest(root, ["android.permission.VIBRATE"])
			minimal_signature = haptics_manifest_signature(minimal)
			with_unrelated = write_manifest(
				root,
				[
					"android.permission.INTERNET",
					"android.permission.VIBRATE",
					"android.permission.ACCESS_NETWORK_STATE",
				],
			)
			self.assertEqual(
				minimal_signature,
				haptics_manifest_signature(with_unrelated),
			)


if __name__ == "__main__":
	unittest.main()
