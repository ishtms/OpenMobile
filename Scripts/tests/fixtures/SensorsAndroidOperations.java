import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

public class SensorsAndroidOperations {
    static final int RESULT_OK = 0, RESULT_INVALID_ARGUMENT = -1,
        RESULT_SENSOR_MISSING = -2, RESULT_PERMISSION_DENIED = -3,
        RESULT_REGISTER_FAILED = -4, RESULT_STREAM_MISSING = -5,
        RESULT_PAUSED = -7, RESULT_SHUTTING_DOWN = -8, RESULT_TIMEOUT = -9;
    static final long HANDLER_TIMEOUT_MILLIS = 40;
    static class Looper { static Object myLooper() { return null; } }
    static class Handler {
        Runnable queued;
        Thread worker;
        boolean runImmediately;
        Object getLooper() { return this; }
        boolean post(Runnable task) {
            queued = task;
            if (runImmediately) { worker = new Thread(task); worker.start(); }
            return true;
        }
        void removeCallbacks(Runnable task) { if (queued == task) queued = null; }
        void drain() throws Exception {
            if (worker != null) worker.join();
            else if (queued != null) queued.run();
        }
    }
    static class Sensor {
        static final int TYPE_ALL = -1;
        final int id;
        final String name;
        Sensor(int id, String name) { this.id = id; this.name = name; }
        int getType() { return 1; }
        int getId() { return id; }
        String getName() { return name; }
        boolean isWakeUpSensor() { return false; }
    }
    static class SensorManager {
        List<Sensor> fixed = new ArrayList<>(), dynamic = new ArrayList<>();
        List<Sensor> getSensorList(int type) { return fixed; }
        List<Sensor> getDynamicSensorList(int type) { return dynamic; }
        Sensor getDefaultSensor(int type) { return fixed.isEmpty() ? null : fixed.get(0); }
    }
    final Handler handler = new Handler();
    final SensorManager sensorManager = new SensorManager();
    final Map<String, StreamState> streams = new HashMap<>();
    List<SensorRecord> sensorRecords = Collections.emptyList();
    boolean shuttingDown, paused;
    long registrationDelay;
    int registrations;
    class StreamState {
        final String streamId;
        final long backendGeneration;
        final Sensor sensor;
        int samplingPeriodUs, maxReportLatencyUs, batchDelayMillis;
        boolean registered;
        final List<String> flushRequests = new ArrayList<>();
        StreamState(String id, String nativeId, long generation, Sensor sensor,
                    int period, int latency, boolean lowLatency) {
            streamId = id; backendGeneration = generation; this.sensor = sensor;
            applyConfiguration(period, latency, lowLatency);
        }
        void applyConfiguration(int period, int latency, boolean lowLatency) {
            samplingPeriodUs = period; maxReportLatencyUs = latency;
            batchDelayMillis = lowLatency ? 1 : 20;
        }
        boolean register() {
            long delay = registrationDelay;
            registrationDelay = 0;
            try { Thread.sleep(delay); } catch (InterruptedException e) { throw new RuntimeException(e); }
            if (!registered) ++registrations;
            registered = true;
            return true;
        }
        void unregister(boolean discard) { if (registered) --registrations; registered = false; }
    }
    boolean hasRequiredPermission(Sensor sensor) { return true; }
    boolean isSupportedNativeType(int type) { return true; }
    void nativeOnStreamRestarted(long generation, String id) {}
    void nativeOnStreamError(long generation, String id, int failure) {}
    // PRODUCTION_METHODS
    static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }
    public static void main(String[] args) throws Exception {
        SensorsAndroidOperations bridge = new SensorsAndroidOperations();
        Sensor sensor = new Sensor(1, "primary");
        bridge.sensorManager.fixed.add(sensor);
        bridge.rebuildDiscovery();
        String nativeId = buildNativeIdentifier(sensor);
        switch (args[0]) {
            case "queued":
                check(bridge.startStream("stream", nativeId, 1, 100, 0, false) == RESULT_TIMEOUT, "start must time out");
                bridge.handler.drain();
                check(bridge.registrations == 0 && bridge.streams.isEmpty(), "timed-out queued start registered an orphan");
                break;
            case "running":
                bridge.registrationDelay = 160;
                bridge.handler.runImmediately = true;
                check(bridge.startStream("stream", nativeId, 1, 100, 0, false) == RESULT_TIMEOUT, "running start must time out");
                bridge.handler.drain();
                check(bridge.registrations == 0 && bridge.streams.isEmpty(), "timed-out running start was not rolled back");
                break;
            case "reconfigure":
                bridge.handler.runImmediately = true;
                check(bridge.startStream("stream", nativeId, 1, 100, 0, false) == RESULT_OK, "initial start failed");
                bridge.registrationDelay = 160;
                check(bridge.reconfigureStream("stream", 200, 50, true) == RESULT_TIMEOUT, "update must time out");
                bridge.handler.drain();
                StreamState state = bridge.streams.get("stream");
                check(bridge.registrations == 1 && state != null && state.samplingPeriodUs == 100
                    && state.maxReportLatencyUs == 0 && state.batchDelayMillis == 20, "failed update changed native configuration");
                break;
            case "identity":
                for (int id : new int[] {0, -1}) {
                    Sensor left = new Sensor(id, "Sensor");
                    Sensor right = new Sensor(id, "sensor");
                    bridge.sensorManager.fixed = Arrays.asList(left, right);
                    bridge.rebuildDiscovery();
                    check(!buildNativeIdentifier(left).equalsIgnoreCase(buildNativeIdentifier(right)), "fallback identifiers collide");
                    check(bridge.findSensor(buildNativeIdentifier(right)).sensor == right, "selection returned wrong sensor");
                }
                break;
            case "dynamic":
                Sensor dynamic = new Sensor(2, "dynamic");
                bridge.sensorManager.dynamic = Arrays.asList(sensor, dynamic);
                bridge.rebuildDiscovery();
                check(bridge.sensorRecords.size() == 2, "dynamic inventory missing or duplicated");
                check(bridge.findSensor(buildNativeIdentifier(dynamic)).sensor == dynamic, "dynamic sensor not selectable");
                bridge.sensorManager.dynamic = Collections.emptyList();
                bridge.rebuildDiscovery();
                check(bridge.findSensor(buildNativeIdentifier(dynamic)) == null, "disconnected sensor remains selectable");
                break;
        }
    }
}
