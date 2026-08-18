import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
JAVA = ROOT / 'Native/OpenMobileSensors/Source/OpenMobileSensorsAndroid/Private/Android/src/com/openmobile/sensors/OpenMobileSensorsBridgeV1.java'


def declaration(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class AndroidOperationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        javac = shutil.which('javac')
        homebrew = pathlib.Path('/opt/homebrew/opt/openjdk@11/bin/javac')
        if homebrew.exists():
            javac = str(homebrew)
        if not javac:
            raise unittest.SkipTest('A JDK is required for Android operation contracts')
        cls.java = str(pathlib.Path(javac).with_name('java'))
        cls.directory = tempfile.TemporaryDirectory(prefix='openmobile-sensor-operations-')
        source = JAVA.read_text()
        methods = [declaration(source, signature) for signature in (
            'private interface HandlerTask', 'private static final class SensorRecord',
            'public int startStream(', 'public int reconfigureStream(',
            'private void rebuildDiscovery()', 'private SensorRecord findSensor(',
            'private static String buildNativeIdentifier(', 'private static String safeText(',
        )]
        offset = 0
        while True:
            offset = source.find('private <T> T callOnHandler(', offset)
            if offset < 0:
                break
            methods.append(declaration(source[offset:], 'private <T> T callOnHandler('))
            offset += 1
        fixture = pathlib.Path(__file__).with_name('fixtures') / 'SensorsAndroidOperations.java'
        code = fixture.read_text().replace('// PRODUCTION_METHODS', '\n'.join(methods))
        path = pathlib.Path(cls.directory.name) / 'SensorsAndroidOperations.java'
        path.write_text(code)
        result = subprocess.run([javac, str(path)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stderr)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_case(self, name):
        result = subprocess.run([self.java, '-cp', self.directory.name,
                                 'SensorsAndroidOperations', name], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_timed_out_queued_start_does_not_register(self):
        self.run_case('queued')

    def test_timed_out_running_start_rolls_back_registration(self):
        self.run_case('running')

    def test_timed_out_reconfigure_restores_previous_configuration(self):
        self.run_case('reconfigure')

    def test_sensor_identifier_fallback_selects_each_instance(self):
        self.run_case('identity')

    def test_dynamic_inventory_is_merged_and_deduplicated(self):
        self.run_case('dynamic')


if __name__ == '__main__':
    unittest.main()
