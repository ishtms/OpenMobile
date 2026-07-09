import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SENSORS = ROOT / "Native" / "OpenMobileSensors"
ANDROID_BACKEND = (
	SENSORS
	/ "Source"
	/ "OpenMobileSensorsAndroid"
	/ "Private"
	/ "OpenMobileSensorsAndroidBackend.cpp"
)
IOS_BRIDGE = (
	SENSORS
	/ "Source"
	/ "OpenMobileSensorsIOS"
	/ "Private"
	/ "OpenMobileSensorsIOSBridge.mm"
)


class SensorsPedometerMetricTests(unittest.TestCase):
	def test_ios_maps_each_optional_metric_for_live_and_historical_results(self):
		bridge = IOS_BRIDGE.read_text(encoding="utf-8")
		for token in (
			"PopulatePedometerMetrics",
			"Data.distance",
			"Data.floorsAscended",
			"Data.floorsDescended",
			"Data.currentPace",
			"Data.currentCadence",
			"longLongValue",
		):
			self.assertIn(token, bridge)
		self.assertGreaterEqual(
			bridge.count("PopulatePedometerMetrics(Data, Sample)"),
			2,
		)

	def test_android_does_not_synthesize_fitness_metrics(self):
		backend = ANDROID_BACKEND.read_text(encoding="utf-8")
		step_block = backend[
			backend.index("if (Type == EOpenMobileSensorType::StepCounter"):
			backend.index("if (Type == EOpenMobileSensorType::Proximity")
		]
		self.assertNotIn("Sample.Metrics.", step_block)


if __name__ == "__main__":
	unittest.main()
