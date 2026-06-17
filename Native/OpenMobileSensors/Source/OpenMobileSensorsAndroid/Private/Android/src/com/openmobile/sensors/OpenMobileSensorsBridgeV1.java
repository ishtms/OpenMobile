package com.openmobile.sensors;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener2;
import android.hardware.SensorManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.SystemClock;
import java.lang.ref.WeakReference;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

public final class OpenMobileSensorsBridgeV1 {
    static final int BRIDGE_VERSION = 1;
    static final int MAX_BATCH_SAMPLES = 64;
    static final int MAX_VALUES_PER_SAMPLE = 6;

    public static final int RESULT_OK = 0;
    public static final int RESULT_INVALID_ARGUMENT = -1;
    public static final int RESULT_SENSOR_MISSING = -2;
    public static final int RESULT_PERMISSION_DENIED = -3;
    public static final int RESULT_REGISTER_FAILED = -4;
    public static final int RESULT_STREAM_MISSING = -5;
    public static final int RESULT_FLUSH_FAILED = -6;
    public static final int RESULT_PAUSED = -7;
    public static final int RESULT_SHUTTING_DOWN = -8;
    public static final int RESULT_TIMEOUT = -9;

    private static final long HANDLER_TIMEOUT_MILLIS = 2000L;
    private static final Object ACTIVE_LOCK = new Object();
    private static WeakReference<OpenMobileSensorsBridgeV1> activeBridge =
        new WeakReference<>(null);

    private interface HandlerTask<T> {
        T run();
    }

    private static final class SensorRecord {
        final Sensor sensor;
        final String nativeIdentifier;
        final boolean preferred;

        SensorRecord(Sensor sensor, String nativeIdentifier, boolean preferred) {
            this.sensor = sensor;
            this.nativeIdentifier = nativeIdentifier;
            this.preferred = preferred;
        }
    }

    private final class StreamState implements SensorEventListener2 {
        final String streamId;
        final String nativeIdentifier;
        final long backendGeneration;
        final Sensor sensor;
        final long[] timestamps = new long[MAX_BATCH_SAMPLES];
        final float[] values = new float[
            MAX_BATCH_SAMPLES * MAX_VALUES_PER_SAMPLE
        ];
        final ArrayDeque<String> flushRequests = new ArrayDeque<>();
        final Runnable batchDeadline = this::flushPendingBatch;
        int samplingPeriodUs;
        int maxReportLatencyUs;
        int batchDelayMillis;
        int sampleCount;
        int valuesPerSample;
        int lastAccuracy = Integer.MIN_VALUE;
        boolean registered;

        StreamState(
            String streamId,
            String nativeIdentifier,
            long backendGeneration,
            Sensor sensor,
            int samplingPeriodUs,
            int maxReportLatencyUs,
            boolean lowLatency
        ) {
            this.streamId = streamId;
            this.nativeIdentifier = nativeIdentifier;
            this.backendGeneration = backendGeneration;
            this.sensor = sensor;
            applyConfiguration(
                samplingPeriodUs,
                maxReportLatencyUs,
                lowLatency
            );
        }

        void applyConfiguration(
            int newSamplingPeriodUs,
            int newMaxReportLatencyUs,
            boolean lowLatency
        ) {
            samplingPeriodUs = newSamplingPeriodUs;
            maxReportLatencyUs = sensor.getFifoMaxEventCount() > 0
                ? newMaxReportLatencyUs
                : 0;
            batchDelayMillis = lowLatency || maxReportLatencyUs <= 0
                ? 0
                : Math.max(1, Math.min(4, maxReportLatencyUs / 1000));
        }

        boolean register() {
            if (registered) {
                return true;
            }
            registered = sensorManager.registerListener(
                this,
                sensor,
                samplingPeriodUs,
                maxReportLatencyUs,
                handler
            );
            return registered;
        }

        void unregister(boolean discardPendingBatch) {
            handler.removeCallbacks(batchDeadline);
            if (registered) {
                try {
                    sensorManager.unregisterListener(this, sensor);
                } catch (RuntimeException exception) {
                } finally {
                    registered = false;
                }
            }
            if (discardPendingBatch) {
                sampleCount = 0;
                valuesPerSample = 0;
            } else {
                flushPendingBatch();
            }
        }

        void failPendingFlushes(int result) {
            String requestId;
            while ((requestId = flushRequests.pollFirst()) != null) {
                nativeOnFlushCompleted(
                    backendGeneration,
                    streamId,
                    requestId,
                    result
                );
            }
        }

        @Override
        public void onSensorChanged(SensorEvent event) {
            if (
                shuttingDown
                || paused
                || !registered
                || event == null
                || event.sensor != sensor
                || event.values == null
                || event.values.length == 0
            ) {
                return;
            }
            int eventValueCount = Math.min(
                event.values.length,
                MAX_VALUES_PER_SAMPLE
            );
            if (
                sampleCount > 0
                && valuesPerSample != eventValueCount
            ) {
                flushPendingBatch();
            }
            if (sampleCount == MAX_BATCH_SAMPLES) {
                flushPendingBatch();
            }
            if (lastAccuracy != event.accuracy) {
                lastAccuracy = event.accuracy;
                nativeOnAccuracyChanged(
                    backendGeneration,
                    streamId,
                    sensor.getType(),
                    event.accuracy,
                    event.timestamp
                );
            }
            valuesPerSample = eventValueCount;
            timestamps[sampleCount] = event.timestamp;
            int valueOffset = sampleCount * MAX_VALUES_PER_SAMPLE;
            for (int index = 0; index < eventValueCount; ++index) {
                values[valueOffset + index] = event.values[index];
            }
            ++sampleCount;
            if (sampleCount == MAX_BATCH_SAMPLES || batchDelayMillis == 0) {
                flushPendingBatch();
            } else if (sampleCount == 1) {
                handler.postDelayed(batchDeadline, batchDelayMillis);
            }
        }

        @Override
        public void onAccuracyChanged(Sensor changedSensor, int accuracy) {
            if (
                shuttingDown
                || !registered
                || changedSensor != sensor
                || lastAccuracy == accuracy
            ) {
                return;
            }
            lastAccuracy = accuracy;
            nativeOnAccuracyChanged(
                backendGeneration,
                streamId,
                sensor.getType(),
                accuracy,
                SystemClock.elapsedRealtimeNanos()
            );
        }

        @Override
        public void onFlushCompleted(Sensor flushedSensor) {
            if (shuttingDown || flushedSensor != sensor) {
                return;
            }
            flushPendingBatch();
            String requestId = flushRequests.pollFirst();
            if (requestId != null) {
                nativeOnFlushCompleted(
                    backendGeneration,
                    streamId,
                    requestId,
                    RESULT_OK
                );
            }
        }

        void flushPendingBatch() {
            handler.removeCallbacks(batchDeadline);
            int deliveredSampleCount = sampleCount;
            int deliveredValueCount = valuesPerSample;
            sampleCount = 0;
            valuesPerSample = 0;
            if (deliveredSampleCount <= 0 || shuttingDown) {
                return;
            }
            nativeOnSampleBatch(
                backendGeneration,
                streamId,
                sensor.getType(),
                deliveredSampleCount,
                deliveredValueCount,
                MAX_VALUES_PER_SAMPLE,
                timestamps,
                values
            );
        }
    }

    private final Context applicationContext;
    private final SensorManager sensorManager;
    private final HandlerThread handlerThread;
    private final Handler handler;
    private final SensorManager.DynamicSensorCallback dynamicSensorCallback;
    private final Map<String, StreamState> streams = new HashMap<>();
    private List<SensorRecord> sensorRecords = Collections.emptyList();
    private WeakReference<Activity> activityReference;
    private volatile boolean paused;
    private volatile boolean shuttingDown;

    private OpenMobileSensorsBridgeV1(Activity activity) {
        applicationContext = activity.getApplicationContext();
        activityReference = new WeakReference<>(activity);
        sensorManager = (SensorManager)applicationContext.getSystemService(
            Context.SENSOR_SERVICE
        );
        if (sensorManager == null) {
            throw new IllegalStateException("SensorManagerUnavailable");
        }
        handlerThread = new HandlerThread("OpenMobileSensorsHandler");
        handlerThread.start();
        handler = new Handler(handlerThread.getLooper());
        dynamicSensorCallback = new SensorManager.DynamicSensorCallback() {
            @Override
            public void onDynamicSensorConnected(Sensor sensor) {
                rebuildDiscovery();
                nativeOnSensorsChanged();
            }

            @Override
            public void onDynamicSensorDisconnected(Sensor sensor) {
                handleSensorDisconnected(sensor);
            }
        };
        Boolean initialized = callOnHandler(() -> {
            rebuildDiscovery();
            if (sensorManager.isDynamicSensorDiscoverySupported()) {
                sensorManager.registerDynamicSensorCallback(
                    dynamicSensorCallback,
                    handler
                );
            }
            return true;
        }, false);
        if (!initialized) {
            handlerThread.quitSafely();
            throw new IllegalStateException("HandlerThreadUnavailable");
        }
    }

    public static OpenMobileSensorsBridgeV1 create(Activity activity) {
        if (activity == null) {
            return null;
        }
        OpenMobileSensorsBridgeV1 previous;
        synchronized (ACTIVE_LOCK) {
            previous = activeBridge.get();
        }
        if (previous != null) {
            previous.shutdown();
        }
        OpenMobileSensorsBridgeV1 bridge;
        try {
            bridge = new OpenMobileSensorsBridgeV1(activity);
        } catch (RuntimeException exception) {
            return null;
        }
        synchronized (ACTIVE_LOCK) {
            activeBridge = new WeakReference<>(bridge);
        }
        return bridge;
    }

    public static boolean hasHighSamplingRateDeclaration(Activity activity) {
        if (activity == null || Build.VERSION.SDK_INT < 31) {
            return activity != null;
        }
        try {
            PackageInfo info;
            if (Build.VERSION.SDK_INT >= 33) {
                info = activity.getPackageManager().getPackageInfo(
                    activity.getPackageName(),
                    PackageManager.PackageInfoFlags.of(
                        PackageManager.GET_PERMISSIONS
                    )
                );
            } else {
                info = activity.getPackageManager().getPackageInfo(
                    activity.getPackageName(),
                    PackageManager.GET_PERMISSIONS
                );
            }
            if (info.requestedPermissions == null) {
                return false;
            }
            for (String permission : info.requestedPermissions) {
                if (Manifest.permission.HIGH_SAMPLING_RATE_SENSORS.equals(
                    permission
                )) {
                    return true;
                }
            }
        } catch (PackageManager.NameNotFoundException | RuntimeException exception) {
            return false;
        }
        return false;
    }

    public static void onActivityResumed(Activity activity) {
        OpenMobileSensorsBridgeV1 bridge = getActiveBridge();
        if (bridge != null) {
            bridge.resume(activity);
        }
    }

    public static void onActivityPaused(Activity activity) {
        OpenMobileSensorsBridgeV1 bridge = getActiveBridge();
        if (bridge != null) {
            bridge.pause(activity);
        }
    }

    public static void onActivityDestroyed(Activity activity) {
        OpenMobileSensorsBridgeV1 bridge = getActiveBridge();
        if (bridge != null) {
            bridge.detachActivity(activity);
        }
    }

    public static void onPermissionsChanged(Activity activity) {
        OpenMobileSensorsBridgeV1 bridge = getActiveBridge();
        if (bridge != null) {
            bridge.activityReference = new WeakReference<>(activity);
            bridge.handler.post(bridge::refreshPermissionState);
        }
    }

    public Object[] querySensorSnapshot() {
        return callOnHandler(this::buildSensorSnapshot, new Object[] {
            new String[0], new long[0], new float[0]
        });
    }

    public int startStream(
        String streamId,
        String nativeIdentifier,
        long backendGeneration,
        int samplingPeriodUs,
        int maxReportLatencyUs,
        boolean lowLatency
    ) {
        if (
            streamId == null
            || streamId.isEmpty()
            || nativeIdentifier == null
            || nativeIdentifier.isEmpty()
            || backendGeneration <= 0L
            || samplingPeriodUs <= 0
            || maxReportLatencyUs < 0
        ) {
            return RESULT_INVALID_ARGUMENT;
        }
        return callOnHandler(() -> {
            if (shuttingDown) {
                return RESULT_SHUTTING_DOWN;
            }
            if (paused) {
                return RESULT_PAUSED;
            }
            if (streams.containsKey(streamId)) {
                return RESULT_INVALID_ARGUMENT;
            }
            SensorRecord record = findSensor(nativeIdentifier);
            if (record == null) {
                return RESULT_SENSOR_MISSING;
            }
            if (!hasRequiredPermission(record.sensor)) {
                return RESULT_PERMISSION_DENIED;
            }
            StreamState state = new StreamState(
                streamId,
                nativeIdentifier,
                backendGeneration,
                record.sensor,
                samplingPeriodUs,
                maxReportLatencyUs,
                lowLatency
            );
            try {
                if (!state.register()) {
                    return RESULT_REGISTER_FAILED;
                }
            } catch (SecurityException exception) {
                return RESULT_PERMISSION_DENIED;
            } catch (RuntimeException exception) {
                return RESULT_REGISTER_FAILED;
            }
            streams.put(streamId, state);
            return RESULT_OK;
        }, RESULT_TIMEOUT);
    }

    public int reconfigureStream(
        String streamId,
        int samplingPeriodUs,
        int maxReportLatencyUs,
        boolean lowLatency
    ) {
        if (
            streamId == null
            || streamId.isEmpty()
            || samplingPeriodUs <= 0
            || maxReportLatencyUs < 0
        ) {
            return RESULT_INVALID_ARGUMENT;
        }
        return callOnHandler(() -> {
            if (shuttingDown) {
                return RESULT_SHUTTING_DOWN;
            }
            if (paused) {
                return RESULT_PAUSED;
            }
            StreamState state = streams.get(streamId);
            if (state == null) {
                return RESULT_STREAM_MISSING;
            }
            if (!hasRequiredPermission(state.sensor)) {
                return RESULT_PERMISSION_DENIED;
            }
            int previousSamplingPeriodUs = state.samplingPeriodUs;
            int previousMaxReportLatencyUs = state.maxReportLatencyUs;
            int previousBatchDelayMillis = state.batchDelayMillis;
            state.unregister(true);
            state.flushRequests.clear();
            state.applyConfiguration(
                samplingPeriodUs,
                maxReportLatencyUs,
                lowLatency
            );
            int reconfigureFailure = RESULT_REGISTER_FAILED;
            try {
                if (state.register()) {
                    nativeOnStreamRestarted(
                        state.backendGeneration,
                        state.streamId
                    );
                    return RESULT_OK;
                }
            } catch (SecurityException exception) {
                state.registered = false;
                reconfigureFailure = RESULT_PERMISSION_DENIED;
            } catch (RuntimeException exception) {
                state.registered = false;
            }
            state.samplingPeriodUs = previousSamplingPeriodUs;
            state.maxReportLatencyUs = previousMaxReportLatencyUs;
            state.batchDelayMillis = previousBatchDelayMillis;
            try {
                state.register();
            } catch (SecurityException exception) {
                state.registered = false;
                reconfigureFailure = RESULT_PERMISSION_DENIED;
            } catch (RuntimeException exception) {
                state.registered = false;
            }
            if (!state.registered) {
                streams.remove(streamId);
                nativeOnStreamError(
                    state.backendGeneration,
                    state.streamId,
                    reconfigureFailure
                );
            }
            return reconfigureFailure;
        }, RESULT_TIMEOUT);
    }

    public int stopStream(String streamId) {
        if (streamId == null || streamId.isEmpty()) {
            return RESULT_INVALID_ARGUMENT;
        }
        return callOnHandler(() -> {
            StreamState state = streams.remove(streamId);
            if (state != null) {
                state.unregister(true);
                state.flushRequests.clear();
            }
            return RESULT_OK;
        }, RESULT_TIMEOUT);
    }

    public int flushStream(String streamId, String requestId) {
        if (
            streamId == null
            || streamId.isEmpty()
            || requestId == null
            || requestId.isEmpty()
        ) {
            return RESULT_INVALID_ARGUMENT;
        }
        return callOnHandler(() -> {
            if (shuttingDown) {
                return RESULT_SHUTTING_DOWN;
            }
            if (paused) {
                return RESULT_PAUSED;
            }
            StreamState state = streams.get(streamId);
            if (state == null || !state.registered) {
                return RESULT_STREAM_MISSING;
            }
            state.flushPendingBatch();
            state.flushRequests.addLast(requestId);
            boolean accepted;
            try {
                accepted = sensorManager.flush(state);
            } catch (RuntimeException exception) {
                accepted = false;
            }
            if (!accepted) {
                state.flushRequests.removeLastOccurrence(requestId);
                return RESULT_FLUSH_FAILED;
            }
            return RESULT_OK;
        }, RESULT_TIMEOUT);
    }

    public void shutdown() {
        if (shuttingDown) {
            return;
        }
        shuttingDown = true;
        callOnHandler(() -> {
            try {
                if (sensorManager.isDynamicSensorDiscoverySupported()) {
                    sensorManager.unregisterDynamicSensorCallback(
                        dynamicSensorCallback
                    );
                }
            } catch (RuntimeException exception) {
            }
            for (StreamState state : streams.values()) {
                state.unregister(true);
                state.flushRequests.clear();
            }
            streams.clear();
            sensorRecords = Collections.emptyList();
            handler.removeCallbacksAndMessages(null);
            return true;
        }, false);
        synchronized (ACTIVE_LOCK) {
            if (activeBridge.get() == this) {
                activeBridge = new WeakReference<>(null);
            }
        }
        handlerThread.quitSafely();
        if (Looper.myLooper() != handlerThread.getLooper()) {
            try {
                handlerThread.join(HANDLER_TIMEOUT_MILLIS);
            } catch (InterruptedException exception) {
                Thread.currentThread().interrupt();
            }
        }
        activityReference.clear();
    }

    private static OpenMobileSensorsBridgeV1 getActiveBridge() {
        synchronized (ACTIVE_LOCK) {
            return activeBridge.get();
        }
    }

    private <T> T callOnHandler(HandlerTask<T> task, T fallback) {
        if (Looper.myLooper() == handler.getLooper()) {
            try {
                return task.run();
            } catch (RuntimeException exception) {
                return fallback;
            }
        }
        CountDownLatch completed = new CountDownLatch(1);
        AtomicReference<T> result = new AtomicReference<>(fallback);
        boolean posted = handler.post(() -> {
            try {
                result.set(task.run());
            } finally {
                completed.countDown();
            }
        });
        if (!posted) {
            return fallback;
        }
        try {
            if (!completed.await(HANDLER_TIMEOUT_MILLIS, TimeUnit.MILLISECONDS)) {
                return fallback;
            }
        } catch (InterruptedException exception) {
            Thread.currentThread().interrupt();
            return fallback;
        }
        return result.get();
    }

    private void rebuildDiscovery() {
        List<SensorRecord> rebuilt = new ArrayList<>();
        for (Sensor sensor : sensorManager.getSensorList(Sensor.TYPE_ALL)) {
            if (!isSupportedNativeType(sensor.getType())) {
                continue;
            }
            Sensor defaultSensor = sensorManager.getDefaultSensor(
                sensor.getType()
            );
            rebuilt.add(new SensorRecord(
                sensor,
                buildNativeIdentifier(sensor),
                sensor == defaultSensor
            ));
        }
        rebuilt.sort(new Comparator<SensorRecord>() {
            @Override
            public int compare(SensorRecord left, SensorRecord right) {
                int typeOrder = Integer.compare(
                    left.sensor.getType(),
                    right.sensor.getType()
                );
                if (typeOrder != 0) {
                    return typeOrder;
                }
                if (left.preferred != right.preferred) {
                    return left.preferred ? -1 : 1;
                }
                if (
                    left.sensor.isWakeUpSensor()
                    != right.sensor.isWakeUpSensor()
                ) {
                    return left.sensor.isWakeUpSensor() ? 1 : -1;
                }
                int idOrder = Integer.compare(
                    left.sensor.getId(),
                    right.sensor.getId()
                );
                if (idOrder != 0) {
                    return idOrder;
                }
                return left.nativeIdentifier.compareTo(right.nativeIdentifier);
            }
        });
        sensorRecords = rebuilt;
    }

    private Object[] buildSensorSnapshot() {
        int count = sensorRecords.size();
        String[] strings = new String[count * 4];
        long[] integers = new long[count * 10];
        float[] numbers = new float[count * 3];
        for (int index = 0; index < count; ++index) {
            SensorRecord record = sensorRecords.get(index);
            Sensor sensor = record.sensor;
            int stringOffset = index * 4;
            strings[stringOffset] = record.nativeIdentifier;
            strings[stringOffset + 1] = safeText(sensor.getName());
            strings[stringOffset + 2] = safeText(sensor.getVendor());
            strings[stringOffset + 3] = safeText(sensor.getStringType());
            int integerOffset = index * 10;
            integers[integerOffset] = sensor.getType();
            integers[integerOffset + 1] = sensor.getId();
            integers[integerOffset + 2] = sensor.getVersion();
            integers[integerOffset + 3] = sensor.getMinDelay();
            integers[integerOffset + 4] = sensor.getMaxDelay();
            integers[integerOffset + 5] = sensor.getFifoMaxEventCount();
            integers[integerOffset + 6] = sensor.getReportingMode();
            integers[integerOffset + 7] = sensor.isWakeUpSensor() ? 1L : 0L;
            integers[integerOffset + 8] = record.preferred ? 1L : 0L;
            integers[integerOffset + 9] = sensor.isDynamicSensor() ? 1L : 0L;
            int numberOffset = index * 3;
            numbers[numberOffset] = sensor.getMaximumRange();
            numbers[numberOffset + 1] = sensor.getResolution();
            numbers[numberOffset + 2] = sensor.getPower();
        }
        return new Object[] {strings, integers, numbers};
    }

    private SensorRecord findSensor(String nativeIdentifier) {
        for (SensorRecord record : sensorRecords) {
            if (record.nativeIdentifier.equals(nativeIdentifier)) {
                return record;
            }
        }
        return null;
    }

    private void pause(Activity activity) {
        Activity current = activityReference.get();
        if (current != null && current != activity) {
            return;
        }
        paused = true;
        handler.post(() -> {
            for (StreamState state : streams.values()) {
                state.unregister(false);
                state.failPendingFlushes(RESULT_PAUSED);
            }
        });
    }

    private void resume(Activity activity) {
        if (activity == null || shuttingDown) {
            return;
        }
        activityReference = new WeakReference<>(activity);
        paused = false;
        handler.post(() -> {
            Iterator<Map.Entry<String, StreamState>> iterator =
                streams.entrySet().iterator();
            while (iterator.hasNext()) {
                StreamState state = iterator.next().getValue();
                if (!hasRequiredPermission(state.sensor)) {
                    iterator.remove();
                    nativeOnStreamError(
                        state.backendGeneration,
                        state.streamId,
                        RESULT_PERMISSION_DENIED
                    );
                    continue;
                }
                try {
                    if (state.register()) {
                        nativeOnStreamRestarted(
                            state.backendGeneration,
                            state.streamId
                        );
                        continue;
                    }
                } catch (RuntimeException exception) {
                    state.registered = false;
                }
                iterator.remove();
                nativeOnStreamError(
                    state.backendGeneration,
                    state.streamId,
                    RESULT_REGISTER_FAILED
                );
            }
        });
    }

    private void detachActivity(Activity activity) {
        if (activityReference.get() == activity) {
            activityReference.clear();
        }
    }

    private void refreshPermissionState() {
        Iterator<Map.Entry<String, StreamState>> iterator =
            streams.entrySet().iterator();
        while (iterator.hasNext()) {
            StreamState state = iterator.next().getValue();
            if (!hasRequiredPermission(state.sensor)) {
                state.unregister(true);
                state.failPendingFlushes(RESULT_PERMISSION_DENIED);
                iterator.remove();
                nativeOnStreamError(
                    state.backendGeneration,
                    state.streamId,
                    RESULT_PERMISSION_DENIED
                );
            }
        }
        nativeOnSensorsChanged();
    }

    private void handleSensorDisconnected(Sensor disconnectedSensor) {
        Iterator<Map.Entry<String, StreamState>> iterator =
            streams.entrySet().iterator();
        while (iterator.hasNext()) {
            StreamState state = iterator.next().getValue();
            if (state.sensor == disconnectedSensor) {
                state.unregister(true);
                state.failPendingFlushes(RESULT_SENSOR_MISSING);
                iterator.remove();
                nativeOnSensorDisconnected(
                    state.backendGeneration,
                    state.streamId,
                    state.sensor.getType()
                );
            }
        }
        rebuildDiscovery();
        nativeOnSensorsChanged();
    }

    private boolean hasRequiredPermission(Sensor sensor) {
        if (
            Build.VERSION.SDK_INT >= 29
            && (sensor.getType() == Sensor.TYPE_STEP_COUNTER
                || sensor.getType() == Sensor.TYPE_STEP_DETECTOR)
        ) {
            return applicationContext.checkSelfPermission(
                Manifest.permission.ACTIVITY_RECOGNITION
            ) == PackageManager.PERMISSION_GRANTED;
        }
        return true;
    }

    private static boolean isSupportedNativeType(int type) {
        switch (type) {
            case Sensor.TYPE_ACCELEROMETER:
            case Sensor.TYPE_ACCELEROMETER_UNCALIBRATED:
            case Sensor.TYPE_GYROSCOPE:
            case Sensor.TYPE_GYROSCOPE_UNCALIBRATED:
            case Sensor.TYPE_MAGNETIC_FIELD:
            case Sensor.TYPE_MAGNETIC_FIELD_UNCALIBRATED:
            case Sensor.TYPE_GRAVITY:
            case Sensor.TYPE_LINEAR_ACCELERATION:
            case Sensor.TYPE_ROTATION_VECTOR:
            case Sensor.TYPE_GAME_ROTATION_VECTOR:
            case Sensor.TYPE_GEOMAGNETIC_ROTATION_VECTOR:
            case Sensor.TYPE_PRESSURE:
            case Sensor.TYPE_LIGHT:
            case Sensor.TYPE_PROXIMITY:
            case Sensor.TYPE_STEP_COUNTER:
            case Sensor.TYPE_STEP_DETECTOR:
            case 42:
                return type != 42 || Build.VERSION.SDK_INT >= 35;
            default:
                return false;
        }
    }

    private static String buildNativeIdentifier(Sensor sensor) {
        return "Android-" + sensor.getType() + "-" + sensor.getId();
    }

    private static String safeText(String value) {
        return value == null ? "" : value;
    }

    private static native void nativeOnSampleBatch(
        long backendGeneration,
        String streamId,
        int sensorType,
        int sampleCount,
        int valuesPerSample,
        int valueStride,
        long[] timestamps,
        float[] values
    );

    private static native void nativeOnAccuracyChanged(
        long backendGeneration,
        String streamId,
        int sensorType,
        int accuracy,
        long timestampNanoseconds
    );

    private static native void nativeOnFlushCompleted(
        long backendGeneration,
        String streamId,
        String requestId,
        int result
    );

    private static native void nativeOnSensorDisconnected(
        long backendGeneration,
        String streamId,
        int sensorType
    );

    private static native void nativeOnStreamError(
        long backendGeneration,
        String streamId,
        int result
    );

    private static native void nativeOnStreamRestarted(
        long backendGeneration,
        String streamId
    );

    private static native void nativeOnSensorsChanged();
}
