import tempfile
import unittest
import zipfile
from pathlib import Path

from Scripts.validate_haptics_artifacts import STARTER_ASSETS, validate_android


class HapticsArtifactTests(unittest.TestCase):
	def test_android_payload_modes_are_isolated(self) -> None:
		with tempfile.TemporaryDirectory() as directory:
			for mode in ("custom", "semantic", "disabled"):
				root = Path(directory) / mode
				intermediate = root / "Intermediate" / "Android"
				intermediate.mkdir(parents=True)
				enabled = mode != "disabled"
				custom = mode == "custom"
				(intermediate / "ActiveUPL.txt").write_text(
					"OpenMobileHaptics_Android_UPL.xml\n" if enabled else "Core_APL.xml\n",
					encoding="utf-8",
				)
				permission = (
					'<uses-permission android:name="android.permission.VIBRATE" />'
					if custom
					else ""
				)
				(intermediate / "arm64_AndroidManifest.xml").write_text(
					f'''<manifest xmlns:android="http://schemas.android.com/apk/res/android">
{permission}<application android:label="Fixture" /></manifest>''',
					encoding="utf-8",
				)

				binaries = root / "Binaries" / "Android"
				binaries.mkdir(parents=True)
				modules = (
					'["OpenMobileHaptics", "OpenMobileHapticsAndroid"]'
					if enabled
					else '["OpenMobileCore"]'
				)
				(binaries / "Fixture.target").write_text(modules, encoding="utf-8")

				staged = root / "Saved" / "StagedBuilds" / "Android"
				staged.mkdir(parents=True)
				asset_lines = "\n".join(
					f"RemappedPlugins/OpenMobileHaptics/Content/StarterPresets/{asset}"
					for asset in STARTER_ASSETS
				) if enabled else "Fixture/AssetRegistry.bin"
				(staged / "Manifest_UFSFiles_Android.txt").write_text(
					asset_lines,
					encoding="utf-8",
				)

				package = root / "Package"
				package.mkdir()
				bridge = b"OpenMobileHapticsBridgeV1" if enabled else b"Fixture"
				jni = (
					b"Java_com_openmobile_haptics_OpenMobileHapticsBridgeV1_nativeCanStart"
					if enabled
					else b"Fixture"
				)
				with zipfile.ZipFile(package / "Fixture.apk", "w") as archive:
					archive.writestr("classes.dex", bridge)
					archive.writestr("lib/arm64-v8a/libUnreal.so", jni)

				validate_android(root, package, mode)


if __name__ == "__main__":
	unittest.main()
