import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
COMMON = SENSORS / "Source" / "OpenMobileSensors"
IOS = SENSORS / "Source" / "OpenMobileSensorsIOS" / "Private"


class SensorsRelativeAltitudeTests(unittest.TestCase):
	def test_sample_reports_baseline_source_and_limitations(self):
		samples = (
			COMMON / "Public" / "OpenMobileSensorSamples.h"
		).read_text(encoding="utf-8")
		for token in (
			"EOpenMobileRelativeAltitudeSource",
			"NativePlatform",
			"PressureBaseline",
			"BaselineTimestampSeconds",
			"bHasBaselinePressure",
			"BaselinePressureHectopascals",
			"bUsesStandardAtmosphereModel",
			"QualityLimitationFlags",
			"WeatherSensitive",
		):
			self.assertIn(token, samples)

	def test_subsystem_exposes_an_explicit_session_lifecycle(self):
		header = (
			COMMON / "Public" / "OpenMobileSensorsSubsystem.h"
		).read_text(encoding="utf-8")
		implementation = (
			COMMON / "Private" / "OpenMobileSensorsSubsystem.cpp"
		).read_text(encoding="utf-8")
		for operation in (
			"BeginRelativeAltitudeSessionNative",
			"RecenterRelativeAltitudeBaselineNative",
			"ReadRelativeAltitudeSessionNative",
			"StopRelativeAltitudeSessionNative",
		):
			self.assertIn(operation, header)
			self.assertIn(operation, implementation)

	def test_android_fallback_uses_only_a_pressure_baseline(self):
		capabilities = (
			COMMON / "Private" / "OpenMobileSensorsCapabilityService.cpp"
		).read_text(encoding="utf-8")
		estimator = (
			COMMON
			/ "Private"
			/ "OpenMobileSensorRelativeAltitudeEstimator.cpp"
		).read_text(encoding="utf-8")
		for token in (
			"ApplyRelativeAltitudeFallback",
			"EOpenMobileSensorType::BarometricPressure",
			"EOpenMobileSensorAvailabilitySource::Derived",
		):
			self.assertIn(token, capabilities)
		self.assertIn("StandardAtmosphereHeightMeters = 44330.0", estimator)
		self.assertIn("StandardAtmosphereExponent = 1.0 / 5.255", estimator)
		self.assertIn("EOpenMobileSensorSourceFlags::PluginDerived", estimator)

	def test_ios_keeps_the_native_relative_altitude_source(self):
		backend = (IOS / "OpenMobileSensorsIOSBackend.mm").read_text(
			encoding="utf-8"
		)
		bridge = (IOS / "OpenMobileSensorsIOSBridge.mm").read_text(
			encoding="utf-8"
		)
		sources = (
			COMMON / "Private" / "OpenMobileSensorSourcePolicy.cpp"
		).read_text(encoding="utf-8")
		self.assertIn("IOS-RelativeAltitude", backend)
		self.assertIn("Data.relativeAltitude.doubleValue", bridge)
		self.assertIn("Sensor == EOpenMobileSensorType::RelativeAltitude", sources)
		self.assertIn("EOpenMobileSensorSourceFlags::NativeFused", sources)

	def test_public_contract_defines_relative_altitude_sessions(self):
		contract = (SENSORS / "Docs" / "PublicContract.md").read_text(
			encoding="utf-8"
		)
		for token in (
			"## Relative altitude sessions",
			"standard-atmosphere pressure formula",
			"independent baseline",
			"weather-sensitive",
			"BeginRelativeAltitudeSessionNative",
			"RecenterRelativeAltitudeBaselineNative",
			"ReadRelativeAltitudeSessionNative",
			"StopRelativeAltitudeSessionNative",
			"physical-device acceptance",
		):
			self.assertIn(token, contract)


if __name__ == "__main__":
	unittest.main()
