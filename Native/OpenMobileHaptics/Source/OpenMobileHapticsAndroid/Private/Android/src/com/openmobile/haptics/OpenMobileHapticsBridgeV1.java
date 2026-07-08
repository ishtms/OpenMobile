package com.openmobile.haptics;

import android.app.Activity;
import android.content.Context;
import android.media.AudioAttributes;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.os.VibrationAttributes;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.provider.Settings;
import android.view.HapticFeedbackConstants;
import android.view.View;
import java.lang.ref.WeakReference;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicLong;

public final class OpenMobileHapticsBridgeV1 {
    static final int BRIDGE_VERSION = 1;
    static final int RESULT_FAILED = 0;
    static final int RESULT_ACCEPTED = 1;
    static final int RESULT_SUPPRESSED = 2;
    static final int RESULT_FALLBACK = 3;
    static final int RESULT_UNSUPPORTED = 4;
    static final int RESULT_DEFAULT_AMPLITUDE = 5;
    static final int RESULT_PENDING = 6;
    static final int RESULT_STALE = 7;
    private static final long MAXIMUM_SCHEDULED_DELAY_MILLIS = 60000L;
    private static final long MAXIMUM_CONTROLLED_DURATION_MILLIS = 300000L;
    private static final int CONTROLLED_EVENT_STARTED = 1;
    private static final int CONTROLLED_EVENT_COMPLETED = 2;
    private static final int CONTROLLED_EVENT_INTERRUPTED = 3;
    private static final int CONTROLLED_EVENT_FAILED = 4;

    private static final AtomicLong submissionCount = new AtomicLong();
    private static final AtomicLong callbackCount = new AtomicLong();
    private static final AtomicLong lastRequestId = new AtomicLong();
    private static final Handler SCHEDULED_HANDLER =
        new Handler(Looper.getMainLooper());
    private static final ConcurrentHashMap<Long, Runnable> SCHEDULED_REQUESTS =
        new ConcurrentHashMap<Long, Runnable>();
    private static final Object CONTROLLED_WAVEFORM_LOCK = new Object();
    private static ControlledWaveform controlledWaveform;
    private static volatile EnvelopeApi36 envelopeApi36;
    private static final LinkedHashMap<Long, PreparedWaveform>
        PREPARED_WAVEFORMS = new LinkedHashMap<Long, PreparedWaveform>(
            16,
            0.75f,
            true
        );
    private static long preparedWaveformBytes;
    private static int maximumPreparedWaveforms = 32;
    private static long maximumPreparedWaveformBytes = 4L * 1024L * 1024L;
    private static long preparedWaveformIdleMillis = 30000L;

    private interface ScheduledPlayback {
        int play(Activity activity);
    }

    private static final class PreparedWaveform {
        final VibrationEffect effect;
        final boolean usedDefaultAmplitude;
        final long estimatedBytes;
        long lastAccessMillis;

        PreparedWaveform(
            VibrationEffect effect,
            boolean usedDefaultAmplitude,
            long estimatedBytes,
            long lastAccessMillis
        ) {
            this.effect = effect;
            this.usedDefaultAmplitude = usedDefaultAmplitude;
            this.estimatedBytes = Math.max(1L, estimatedBytes);
            this.lastAccessMillis = lastAccessMillis;
        }
    }

    private static final class ControlledWaveform {
        final long requestId;
        final WeakReference<Activity> activity;
        Runnable startRunnable;
        Runnable completionRunnable;
        long controlRevision;
        boolean started;
        boolean paused;

        ControlledWaveform(Activity owner, long id) {
            requestId = id;
            activity = new WeakReference<Activity>(owner);
        }
    }

    private static final class EnvelopeApi36 {
        final Method areEnvelopeEffectsSupported;
        final Method getEnvelopeEffectInfo;
        final Method getFrequencyProfile;
        final Method getMaxSize;
        final Method getMinControlPointDurationMillis;
        final Method getMaxControlPointDurationMillis;
        final Method getMaxDurationMillis;
        final Method getMinFrequencyHz;
        final Method getMaxFrequencyHz;
        final Constructor<?> basicConstructor;
        final Method basicSetInitialSharpness;
        final Method basicAddControlPoint;
        final Method basicBuild;
        final Constructor<?> waveformConstructor;
        final Method waveformSetInitialFrequencyHz;
        final Method waveformAddControlPoint;
        final Method waveformBuild;

        EnvelopeApi36() throws ReflectiveOperationException {
            areEnvelopeEffectsSupported = Vibrator.class.getMethod(
                "areEnvelopeEffectsSupported"
            );
            getEnvelopeEffectInfo = Vibrator.class.getMethod(
                "getEnvelopeEffectInfo"
            );
            getFrequencyProfile = Vibrator.class.getMethod(
                "getFrequencyProfile"
            );

            Class<?> infoClass = Class.forName(
                "android.os.vibrator.VibratorEnvelopeEffectInfo"
            );
            getMaxSize = infoClass.getMethod("getMaxSize");
            getMinControlPointDurationMillis = infoClass.getMethod(
                "getMinControlPointDurationMillis"
            );
            getMaxControlPointDurationMillis = infoClass.getMethod(
                "getMaxControlPointDurationMillis"
            );
            getMaxDurationMillis = infoClass.getMethod(
                "getMaxDurationMillis"
            );

            Class<?> profileClass = Class.forName(
                "android.os.vibrator.VibratorFrequencyProfile"
            );
            getMinFrequencyHz = profileClass.getMethod("getMinFrequencyHz");
            getMaxFrequencyHz = profileClass.getMethod("getMaxFrequencyHz");

            Class<?> basicClass = Class.forName(
                "android.os.VibrationEffect$BasicEnvelopeBuilder"
            );
            basicConstructor = basicClass.getDeclaredConstructor();
            basicSetInitialSharpness = basicClass.getMethod(
                "setInitialSharpness",
                float.class
            );
            basicAddControlPoint = basicClass.getMethod(
                "addControlPoint",
                float.class,
                float.class,
                long.class
            );
            basicBuild = basicClass.getMethod("build");

            Class<?> waveformClass = Class.forName(
                "android.os.VibrationEffect$WaveformEnvelopeBuilder"
            );
            waveformConstructor = waveformClass.getDeclaredConstructor();
            waveformSetInitialFrequencyHz = waveformClass.getMethod(
                "setInitialFrequencyHz",
                float.class
            );
            waveformAddControlPoint = waveformClass.getMethod(
                "addControlPoint",
                float.class,
                float.class,
                long.class
            );
            waveformBuild = waveformClass.getMethod("build");
        }
    }

    private OpenMobileHapticsBridgeV1() {
    }

    private static int schedulePlayback(
        Activity activity,
        final long requestId,
        long startDelayMillis,
        final ScheduledPlayback playback
    ) {
        if (activity == null
            || requestId == 0L
            || startDelayMillis <= 0L
            || startDelayMillis > MAXIMUM_SCHEDULED_DELAY_MILLIS) {
            return RESULT_UNSUPPORTED;
        }
        final WeakReference<Activity> weakActivity =
            new WeakReference<Activity>(activity);
        final Runnable request = new Runnable() {
            @Override
            public void run() {
                if (SCHEDULED_REQUESTS.remove(requestId) != this) {
                    return;
                }
                Activity current = weakActivity.get();
                int result = RESULT_SUPPRESSED;
                if (current != null) {
                    boolean canStart = false;
                    try {
                        canStart = nativeCanStart(requestId);
                    } catch (UnsatisfiedLinkError ignored) {
                    }
                    result = canStart
                        ? playback.play(current)
                        : RESULT_STALE;
                }
                incrementBounded(callbackCount);
                try {
                    nativeOnBridgeResult(requestId, result);
                } catch (UnsatisfiedLinkError ignored) {
                }
            }
        };
        SCHEDULED_REQUESTS.put(requestId, request);
        long startUptimeMillis = SystemClock.uptimeMillis() + startDelayMillis;
        if (!SCHEDULED_HANDLER.postAtTime(request, startUptimeMillis)) {
            SCHEDULED_REQUESTS.remove(requestId);
            return RESULT_FAILED;
        }
        return RESULT_PENDING;
    }

    static boolean cancelScheduledRequest(long requestId) {
        Runnable request = SCHEDULED_REQUESTS.remove(requestId);
        if (request == null) {
            return false;
        }
        SCHEDULED_HANDLER.removeCallbacks(request);
        return true;
    }

    private static void cancelScheduledRequests() {
        for (Map.Entry<Long, Runnable> entry : SCHEDULED_REQUESTS.entrySet()) {
            SCHEDULED_HANDLER.removeCallbacks(entry.getValue());
        }
        SCHEDULED_REQUESTS.clear();
    }

    private static void emitControlledWaveformEvent(
        ControlledWaveform state,
        int event
    ) {
        final long requestId = state.requestId;
        final long controlRevision = state.controlRevision;
        final int controlledEvent = event;
        SCHEDULED_HANDLER.post(new Runnable() {
            @Override
            public void run() {
                incrementBounded(callbackCount);
                try {
                    nativeOnControlledWaveformEvent(
                        requestId,
                        controlRevision,
                        controlledEvent
                    );
                } catch (UnsatisfiedLinkError ignored) {
                }
            }
        });
    }

    private static void removeControlledCallbacks(ControlledWaveform state) {
        if (state.startRunnable != null) {
            SCHEDULED_HANDLER.removeCallbacks(state.startRunnable);
            state.startRunnable = null;
        }
        if (state.completionRunnable != null) {
            SCHEDULED_HANDLER.removeCallbacks(state.completionRunnable);
            state.completionRunnable = null;
        }
    }

    private static void cancelControlledOutput(ControlledWaveform state) {
        Activity activity = state.activity.get();
        try {
            Vibrator vibrator = vibrator(activity);
            if (vibrator != null) {
                vibrator.cancel();
            }
        } catch (SecurityException ignored) {
        } catch (Exception ignored) {
        }
    }

    private static void finishControlledWaveformLocked(
        ControlledWaveform state,
        int event,
        boolean cancelOutput,
        boolean emitEvent
    ) {
        if (controlledWaveform != state) {
            return;
        }
        removeControlledCallbacks(state);
        if (cancelOutput) {
            cancelControlledOutput(state);
        }
        controlledWaveform = null;
        if (emitEvent) {
            emitControlledWaveformEvent(state, event);
        }
    }

    private static void interruptControlledWaveform() {
        synchronized (CONTROLLED_WAVEFORM_LOCK) {
            if (controlledWaveform != null) {
                finishControlledWaveformLocked(
                    controlledWaveform,
                    CONTROLLED_EVENT_INTERRUPTED,
                    true,
                    true
                );
            }
        }
    }

    private static int startControlledOutputLocked(
        final ControlledWaveform state,
        long preparedResourceId,
        long[] timingsMilliseconds,
        int[] amplitudes,
        int repeatIndex,
        int purpose,
        long completionDurationMillis,
        boolean emitStarted
    ) {
        if (controlledWaveform != state
            || completionDurationMillis <= 0L
            || completionDurationMillis > MAXIMUM_CONTROLLED_DURATION_MILLIS) {
            return RESULT_FAILED;
        }
        Activity activity = state.activity.get();
        if (activity == null) {
            return RESULT_SUPPRESSED;
        }
        try {
            Vibrator vibrator = vibrator(activity);
            Context context = applicationContext(activity);
            if (!systemHapticsEnabled(context)) {
                return RESULT_SUPPRESSED;
            }
            long nowMillis = SystemClock.elapsedRealtime();
            PreparedWaveform prepared = null;
            if (preparedResourceId != 0L) {
                synchronized (PREPARED_WAVEFORMS) {
                    prunePreparedWaveformsLocked(nowMillis);
                    prepared = PREPARED_WAVEFORMS.get(preparedResourceId);
                    if (prepared != null) {
                        prepared.lastAccessMillis = nowMillis;
                    }
                }
            }
            if (prepared == null) {
                prepared = createPreparedWaveform(
                    vibrator,
                    timingsMilliseconds,
                    amplitudes,
                    repeatIndex,
                    1L,
                    nowMillis
                );
            }
            if (prepared == null) {
                return RESULT_UNSUPPORTED;
            }
            vibrateControlled(vibrator, prepared.effect, purpose);
            state.started = true;
            state.paused = false;
            final long expectedRevision = state.controlRevision;
            state.completionRunnable = new Runnable() {
                @Override
                public void run() {
                    synchronized (CONTROLLED_WAVEFORM_LOCK) {
                        if (controlledWaveform != state
                            || state.paused
                            || state.controlRevision != expectedRevision) {
                            return;
                        }
                        finishControlledWaveformLocked(
                            state,
                            CONTROLLED_EVENT_COMPLETED,
                            true,
                            true
                        );
                    }
                }
            };
            long completionUptimeMillis = SystemClock.uptimeMillis()
                + completionDurationMillis;
            if (!SCHEDULED_HANDLER.postAtTime(
                state.completionRunnable,
                completionUptimeMillis
            )) {
                state.completionRunnable = null;
                vibrator.cancel();
                return RESULT_FAILED;
            }
            if (emitStarted) {
                final long startedRevision = state.controlRevision;
                SCHEDULED_HANDLER.post(new Runnable() {
                    @Override
                    public void run() {
                        synchronized (CONTROLLED_WAVEFORM_LOCK) {
                            if (controlledWaveform == state
                                && state.controlRevision == startedRevision) {
                                emitControlledWaveformEvent(
                                    state,
                                    CONTROLLED_EVENT_STARTED
                                );
                            }
                        }
                    }
                });
            }
            return prepared.usedDefaultAmplitude
                ? RESULT_DEFAULT_AMPLITUDE
                : RESULT_ACCEPTED;
        } catch (SecurityException exception) {
            return RESULT_FAILED;
        } catch (Exception exception) {
            return RESULT_FAILED;
        }
    }

    static int playControlledWaveform(
        Activity activity,
        final long requestId,
        final long preparedResourceId,
        final long[] timingsMilliseconds,
        final int[] amplitudes,
        final int repeatIndex,
        final int purpose,
        final long completionDurationMillis,
        long startDelayMillis
    ) {
        recordSubmission(requestId);
        if (activity == null || requestId == 0L
            || startDelayMillis < 0L
            || startDelayMillis > MAXIMUM_SCHEDULED_DELAY_MILLIS) {
            return RESULT_UNSUPPORTED;
        }
        synchronized (CONTROLLED_WAVEFORM_LOCK) {
            if (controlledWaveform != null) {
                finishControlledWaveformLocked(
                    controlledWaveform,
                    CONTROLLED_EVENT_INTERRUPTED,
                    true,
                    true
                );
            }
            final ControlledWaveform state =
                new ControlledWaveform(activity, requestId);
            controlledWaveform = state;
            if (startDelayMillis > 0L) {
                state.startRunnable = new Runnable() {
                    @Override
                    public void run() {
                        synchronized (CONTROLLED_WAVEFORM_LOCK) {
                            if (controlledWaveform != state) {
                                return;
                            }
                            state.startRunnable = null;
                            boolean canStart = false;
                            try {
                                canStart = nativeCanStart(requestId);
                            } catch (UnsatisfiedLinkError ignored) {
                            }
                            if (!canStart) {
                                finishControlledWaveformLocked(
                                    state,
                                    CONTROLLED_EVENT_INTERRUPTED,
                                    false,
                                    true
                                );
                                return;
                            }
                            int result = startControlledOutputLocked(
                                state,
                                preparedResourceId,
                                timingsMilliseconds,
                                amplitudes,
                                repeatIndex,
                                purpose,
                                completionDurationMillis,
                                true
                            );
                            if (result != RESULT_ACCEPTED
                                && result != RESULT_DEFAULT_AMPLITUDE) {
                                finishControlledWaveformLocked(
                                    state,
                                    CONTROLLED_EVENT_FAILED,
                                    true,
                                    true
                                );
                            }
                        }
                    }
                };
                long startUptimeMillis = SystemClock.uptimeMillis()
                    + startDelayMillis;
                if (!SCHEDULED_HANDLER.postAtTime(
                    state.startRunnable,
                    startUptimeMillis
                )) {
                    controlledWaveform = null;
                    return RESULT_FAILED;
                }
                return RESULT_PENDING;
            }
            int result = startControlledOutputLocked(
                state,
                preparedResourceId,
                timingsMilliseconds,
                amplitudes,
                repeatIndex,
                purpose,
                completionDurationMillis,
                true
            );
            if (result != RESULT_ACCEPTED
                && result != RESULT_DEFAULT_AMPLITUDE) {
                finishControlledWaveformLocked(
                    state,
                    CONTROLLED_EVENT_FAILED,
                    true,
                    false
                );
                return result;
            }
            return RESULT_PENDING;
        }
    }

    static int pauseControlledWaveform(
        Activity activity,
        long requestId,
        long controlRevision
    ) {
        synchronized (CONTROLLED_WAVEFORM_LOCK) {
            ControlledWaveform state = controlledWaveform;
            if (state == null || state.requestId != requestId) {
                return RESULT_STALE;
            }
            if (state.activity.get() != activity) {
                finishControlledWaveformLocked(
                    state,
                    CONTROLLED_EVENT_INTERRUPTED,
                    true,
                    true
                );
                return RESULT_STALE;
            }
            if (!state.started || state.paused
                || state.controlRevision == Long.MAX_VALUE
                || controlRevision != state.controlRevision + 1L) {
                return RESULT_FAILED;
            }
            removeControlledCallbacks(state);
            cancelControlledOutput(state);
            state.controlRevision = controlRevision;
            state.paused = true;
            return RESULT_ACCEPTED;
        }
    }

    static int resumeControlledWaveform(
        Activity activity,
        long requestId,
        long controlRevision,
        long[] timingsMilliseconds,
        int[] amplitudes,
        int repeatIndex,
        int purpose,
        long completionDurationMillis
    ) {
        synchronized (CONTROLLED_WAVEFORM_LOCK) {
            ControlledWaveform state = controlledWaveform;
            if (state == null || state.requestId != requestId) {
                return RESULT_STALE;
            }
            if (state.activity.get() != activity) {
                finishControlledWaveformLocked(
                    state,
                    CONTROLLED_EVENT_INTERRUPTED,
                    true,
                    true
                );
                return RESULT_STALE;
            }
            if (!state.paused
                || state.controlRevision == Long.MAX_VALUE
                || controlRevision != state.controlRevision + 1L) {
                return RESULT_FAILED;
            }
            long previousRevision = state.controlRevision;
            state.controlRevision = controlRevision;
            int result = startControlledOutputLocked(
                state,
                0L,
                timingsMilliseconds,
                amplitudes,
                repeatIndex,
                purpose,
                completionDurationMillis,
                false
            );
            if (result != RESULT_ACCEPTED
                && result != RESULT_DEFAULT_AMPLITUDE) {
                state.controlRevision = previousRevision;
                state.paused = true;
                return result;
            }
            return RESULT_ACCEPTED;
        }
    }

    static int seekControlledWaveform(
        Activity activity,
        long requestId,
        long controlRevision,
        long[] timingsMilliseconds,
        int[] amplitudes,
        int repeatIndex,
        int purpose,
        long completionDurationMillis
    ) {
        synchronized (CONTROLLED_WAVEFORM_LOCK) {
            ControlledWaveform state = controlledWaveform;
            if (state == null || state.requestId != requestId) {
                return RESULT_STALE;
            }
            Activity owner = state.activity.get();
            if (owner == null || owner != activity) {
                finishControlledWaveformLocked(
                    state,
                    CONTROLLED_EVENT_INTERRUPTED,
                    true,
                    true
                );
                return RESULT_STALE;
            }
            if (!state.started
                || state.controlRevision == Long.MAX_VALUE
                || controlRevision != state.controlRevision + 1L) {
                return RESULT_FAILED;
            }
            if (state.paused) {
                try {
                    PreparedWaveform prepared = createPreparedWaveform(
                        vibrator(owner),
                        timingsMilliseconds,
                        amplitudes,
                        repeatIndex,
                        1L,
                        SystemClock.uptimeMillis()
                    );
                    if (prepared == null) {
                        return RESULT_UNSUPPORTED;
                    }
                } catch (Exception exception) {
                    return RESULT_FAILED;
                }
                state.controlRevision = controlRevision;
                return RESULT_ACCEPTED;
            }
            removeControlledCallbacks(state);
            cancelControlledOutput(state);
            long previousRevision = state.controlRevision;
            state.controlRevision = controlRevision;
            int result = startControlledOutputLocked(
                state,
                0L,
                timingsMilliseconds,
                amplitudes,
                repeatIndex,
                purpose,
                completionDurationMillis,
                false
            );
            if (result != RESULT_ACCEPTED
                && result != RESULT_DEFAULT_AMPLITUDE) {
                state.controlRevision = previousRevision;
                finishControlledWaveformLocked(
                    state,
                    CONTROLLED_EVENT_FAILED,
                    true,
                    true
                );
                return result;
            }
            return RESULT_ACCEPTED;
        }
    }

    static boolean stopControlledWaveform(long requestId) {
        synchronized (CONTROLLED_WAVEFORM_LOCK) {
            ControlledWaveform state = controlledWaveform;
            if (state == null || state.requestId != requestId) {
                return false;
            }
            finishControlledWaveformLocked(
                state,
                CONTROLLED_EVENT_COMPLETED,
                true,
                false
            );
            return true;
        }
    }

    private static void clearControlledWaveform() {
        synchronized (CONTROLLED_WAVEFORM_LOCK) {
            if (controlledWaveform != null) {
                finishControlledWaveformLocked(
                    controlledWaveform,
                    CONTROLLED_EVENT_COMPLETED,
                    true,
                    false
                );
            }
        }
    }

    private static EnvelopeApi36 envelopeApi36()
        throws ReflectiveOperationException {
        EnvelopeApi36 api = envelopeApi36;
        if (api != null) {
            return api;
        }
        synchronized (OpenMobileHapticsBridgeV1.class) {
            api = envelopeApi36;
            if (api == null) {
                api = new EnvelopeApi36();
                envelopeApi36 = api;
            }
        }
        return api;
    }

    private static void prunePreparedWaveformsLocked(long nowMillis) {
        Iterator<Map.Entry<Long, PreparedWaveform>> iterator =
            PREPARED_WAVEFORMS.entrySet().iterator();
        while (iterator.hasNext()) {
            PreparedWaveform prepared = iterator.next().getValue();
            if (nowMillis - prepared.lastAccessMillis
                >= preparedWaveformIdleMillis) {
                preparedWaveformBytes = Math.max(
                    0L,
                    preparedWaveformBytes - prepared.estimatedBytes
                );
                iterator.remove();
            }
        }
        iterator = PREPARED_WAVEFORMS.entrySet().iterator();
        while (iterator.hasNext()
            && (PREPARED_WAVEFORMS.size() > maximumPreparedWaveforms
                || preparedWaveformBytes
                    > maximumPreparedWaveformBytes)) {
            PreparedWaveform prepared = iterator.next().getValue();
            preparedWaveformBytes = Math.max(
                0L,
                preparedWaveformBytes - prepared.estimatedBytes
            );
            iterator.remove();
        }
    }

    private static PreparedWaveform createPreparedWaveform(
        Vibrator vibrator,
        long[] timingsMilliseconds,
        int[] amplitudes,
        int repeatIndex,
        long estimatedBytes,
        long nowMillis
    ) {
        if (Build.VERSION.SDK_INT < 26
            || vibrator == null
            || !vibrator.hasVibrator()
            || timingsMilliseconds == null
            || amplitudes == null
            || timingsMilliseconds.length == 0
            || timingsMilliseconds.length != amplitudes.length
            || repeatIndex < -1
            || repeatIndex >= timingsMilliseconds.length) {
            return null;
        }
        boolean hasPositiveTiming = false;
        for (int index = 0; index < timingsMilliseconds.length; ++index) {
            long timing = timingsMilliseconds[index];
            int amplitude = amplitudes[index];
            if (timing < 0L
                || (amplitude != VibrationEffect.DEFAULT_AMPLITUDE
                    && (amplitude < 0 || amplitude > 255))) {
                return null;
            }
            hasPositiveTiming |= timing > 0L;
        }
        if (!hasPositiveTiming) {
            return null;
        }

        boolean amplitudeControl = vibrator.hasAmplitudeControl();
        boolean usedDefaultAmplitude = false;
        int[] nativeAmplitudes = new int[amplitudes.length];
        for (int index = 0; index < amplitudes.length; ++index) {
            int amplitude = amplitudes[index];
            nativeAmplitudes[index] = amplitude == 0
                ? 0
                : amplitudeControl
                    ? amplitude
                    : VibrationEffect.DEFAULT_AMPLITUDE;
            usedDefaultAmplitude |= !amplitudeControl && amplitude > 0;
        }
        return new PreparedWaveform(
            VibrationEffect.createWaveform(
                timingsMilliseconds,
                nativeAmplitudes,
                repeatIndex
            ),
            usedDefaultAmplitude,
            estimatedBytes,
            nowMillis
        );
    }

    static int prepareWaveform(
        Activity activity,
        long resourceId,
        long[] timingsMilliseconds,
        int[] amplitudes,
        int repeatIndex,
        long estimatedBytes,
        int maximumCount,
        long maximumBytes,
        long idleLifetimeMillis
    ) {
        if (resourceId == 0L
            || estimatedBytes <= 0L
            || maximumCount <= 0
            || maximumBytes <= 0L
            || idleLifetimeMillis <= 0L) {
            return RESULT_UNSUPPORTED;
        }
        try {
            long nowMillis = SystemClock.elapsedRealtime();
            Vibrator vibrator = vibrator(activity);
            PreparedWaveform prepared = createPreparedWaveform(
                vibrator,
                timingsMilliseconds,
                amplitudes,
                repeatIndex,
                estimatedBytes,
                nowMillis
            );
            if (prepared == null) {
                return RESULT_UNSUPPORTED;
            }
            synchronized (PREPARED_WAVEFORMS) {
                maximumPreparedWaveforms = maximumCount;
                maximumPreparedWaveformBytes = maximumBytes;
                preparedWaveformIdleMillis = idleLifetimeMillis;
                PreparedWaveform previous = PREPARED_WAVEFORMS.remove(
                    resourceId
                );
                if (previous != null) {
                    preparedWaveformBytes = Math.max(
                        0L,
                        preparedWaveformBytes - previous.estimatedBytes
                    );
                }
                if (prepared.estimatedBytes <= maximumBytes) {
                    PREPARED_WAVEFORMS.put(resourceId, prepared);
                    preparedWaveformBytes += prepared.estimatedBytes;
                }
                prunePreparedWaveformsLocked(nowMillis);
            }
            return RESULT_ACCEPTED;
        } catch (SecurityException exception) {
            return RESULT_FAILED;
        } catch (Exception exception) {
            return RESULT_FAILED;
        }
    }

    static void releasePreparedResources() {
        synchronized (PREPARED_WAVEFORMS) {
            PREPARED_WAVEFORMS.clear();
            preparedWaveformBytes = 0L;
        }
    }

    static long[] queryCapabilities(Activity activity) {
        try {
            Vibrator vibrator = vibrator(activity);
            if (vibrator == null) {
                return new long[] {-1L};
            }
            if (!vibrator.hasVibrator()) {
                return new long[] {
                    0L, 0L, 0L, -1L, -1L, -1L, -1L, -1L, -1L
                };
            }

            long flags = 1L | 2L;
            if (Build.VERSION.SDK_INT >= 26) {
                flags |= 32L | 64L;
                if (vibrator.hasAmplitudeControl()) {
                    flags |= 8L;
                }
            }
            long presetSupport = 0L;
            long primitiveSupport = 0L;
            long maxControlPoints = -1L;
            long maxDurationMillis = -1L;
            long minTimingMillis = -1L;
            long maxControlPointDurationMillis = -1L;
            long minFrequencyMilliHertz = -1L;
            long maxFrequencyMilliHertz = -1L;
            if (Build.VERSION.SDK_INT >= 30) {
                int[] effects = vibrator.areEffectsSupported(
                    VibrationEffect.EFFECT_TICK,
                    VibrationEffect.EFFECT_CLICK,
                    VibrationEffect.EFFECT_HEAVY_CLICK,
                    VibrationEffect.EFFECT_DOUBLE_CLICK
                );
                for (int index = 0; index < effects.length; ++index) {
                    presetSupport |= ((long)effects[index] & 3L) << (index * 2);
                }
                flags |= 1024L;
                boolean[] primitives = vibrator.arePrimitivesSupported(
                    VibrationEffect.Composition.PRIMITIVE_CLICK,
                    VibrationEffect.Composition.PRIMITIVE_THUD,
                    VibrationEffect.Composition.PRIMITIVE_SPIN,
                    VibrationEffect.Composition.PRIMITIVE_QUICK_RISE,
                    VibrationEffect.Composition.PRIMITIVE_SLOW_RISE,
                    VibrationEffect.Composition.PRIMITIVE_QUICK_FALL,
                    VibrationEffect.Composition.PRIMITIVE_TICK,
                    VibrationEffect.Composition.PRIMITIVE_LOW_TICK
                );
                for (int index = 0; index < primitives.length; ++index) {
                    if (primitives[index]) {
                        primitiveSupport |= 1L << index;
                    }
                }
                flags |= 2048L;
                if (primitiveSupport != 0L) {
                    flags |= 4L | 128L;
                }
            }
            if (Build.VERSION.SDK_INT >= 36) {
                try {
                    Method supportMethod = Vibrator.class.getMethod(
                        "areEnvelopeEffectsSupported"
                    );
                    boolean supported = (Boolean)supportMethod.invoke(vibrator);
                    flags |= 4096L | 8192L;
                    if (supported) {
                        flags |= 256L;
                        Object info = Vibrator.class.getMethod(
                            "getEnvelopeEffectInfo"
                        ).invoke(vibrator);
                        if (info != null) {
                            maxControlPoints = ((Number)info.getClass()
                                .getMethod("getMaxSize").invoke(info)).longValue();
                            maxDurationMillis = ((Number)info.getClass()
                                .getMethod("getMaxDurationMillis")
                                .invoke(info)).longValue();
                            minTimingMillis = ((Number)info.getClass()
                                .getMethod("getMinControlPointDurationMillis")
                                .invoke(info)).longValue();
                            maxControlPointDurationMillis = ((Number)info
                                .getClass()
                                .getMethod("getMaxControlPointDurationMillis")
                                .invoke(info)).longValue();
                        }
                        Object frequencyProfile = Vibrator.class.getMethod(
                            "getFrequencyProfile"
                        ).invoke(vibrator);
                        if (frequencyProfile != null) {
                            flags |= 512L;
                            minFrequencyMilliHertz = Math.round(
                                ((Number)frequencyProfile.getClass()
                                    .getMethod("getMinFrequencyHz")
                                    .invoke(frequencyProfile)).doubleValue()
                                    * 1000.0
                            );
                            maxFrequencyMilliHertz = Math.round(
                                ((Number)frequencyProfile.getClass()
                                    .getMethod("getMaxFrequencyHz")
                                    .invoke(frequencyProfile)).doubleValue()
                                    * 1000.0
                            );
                        }
                    }
                } catch (ReflectiveOperationException ignored) {
                }
            }
            return new long[] {
                flags,
                presetSupport,
                primitiveSupport,
                maxControlPoints,
                maxDurationMillis,
                minTimingMillis,
                maxControlPointDurationMillis,
                minFrequencyMilliHertz,
                maxFrequencyMilliHertz
            };
        } catch (Exception exception) {
            return new long[] {-1L};
        }
    }

    static int playSemantic(
        Activity activity,
        long requestId,
        int behavior,
        float intensity,
        int path,
        int purpose,
        long startDelayMillis
    ) {
        recordSubmission(requestId);
        if (startDelayMillis > 0L) {
            final int scheduledBehavior = behavior;
            final float scheduledIntensity = intensity;
            final int scheduledPath = path;
            final int scheduledPurpose = purpose;
            return schedulePlayback(
                activity,
                requestId,
                startDelayMillis,
                new ScheduledPlayback() {
                    @Override
                    public int play(Activity current) {
                        return playSemantic(
                            current,
                            0L,
                            scheduledBehavior,
                            scheduledIntensity,
                            scheduledPath,
                            scheduledPurpose,
                            0L
                        );
                    }
                }
            );
        }
        if (path != 1) {
            return playVibration(activity, behavior, intensity, path, purpose);
        }
        if (!isUsable(activity)) {
            return RESULT_SUPPRESSED;
        }
        if (Looper.myLooper() == Looper.getMainLooper()) {
            return performSemantic(activity, behavior);
        }

        final WeakReference<Activity> weakActivity =
            new WeakReference<Activity>(activity);
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Activity current = weakActivity.get();
                int result = current != null
                    ? performSemantic(current, behavior)
                    : RESULT_SUPPRESSED;
                incrementBounded(callbackCount);
                try {
                    nativeOnBridgeResult(requestId, result);
                } catch (UnsatisfiedLinkError ignored) {
                }
            }
        });
        return RESULT_PENDING;
    }

    static int playOneShot(
        Activity activity,
        long requestId,
        long durationMillis,
        float intensity,
        int path,
        int purpose,
        long startDelayMillis
    ) {
        recordSubmission(requestId);
        if (startDelayMillis > 0L) {
            final long scheduledDurationMillis = durationMillis;
            final float scheduledIntensity = intensity;
            final int scheduledPath = path;
            final int scheduledPurpose = purpose;
            return schedulePlayback(
                activity,
                requestId,
                startDelayMillis,
                new ScheduledPlayback() {
                    @Override
                    public int play(Activity current) {
                        return playOneShot(
                            current,
                            0L,
                            scheduledDurationMillis,
                            scheduledIntensity,
                            scheduledPath,
                            scheduledPurpose,
                            0L
                        );
                    }
                }
            );
        }
        try {
            if (path == 1) {
                if (!isUsable(activity)) {
                    return RESULT_SUPPRESSED;
                }
                return performSemantic(activity, 1);
            }

            Vibrator vibrator = vibrator(activity);
            if (vibrator == null || !vibrator.hasVibrator()) {
                return RESULT_UNSUPPORTED;
            }
            Context context = applicationContext(activity);
            if (!systemHapticsEnabled(context)) {
                return RESULT_SUPPRESSED;
            }
            VibrationEffect effect = null;
            boolean usedDefaultAmplitude = false;
            if (path == 2) {
                if (Build.VERSION.SDK_INT < 30) {
                    return RESULT_UNSUPPORTED;
                }
                int[] support = vibrator.areEffectsSupported(
                    VibrationEffect.EFFECT_CLICK
                );
                if (support == null
                    || support.length != 1
                    || support[0] != Vibrator.VIBRATION_EFFECT_SUPPORT_YES) {
                    return RESULT_UNSUPPORTED;
                }
                effect = VibrationEffect.createPredefined(
                    VibrationEffect.EFFECT_CLICK
                );
            } else if (path != 3) {
                return RESULT_UNSUPPORTED;
            }
            if (effect == null && Build.VERSION.SDK_INT >= 26) {
                boolean amplitudeControl = vibrator.hasAmplitudeControl();
                int amplitude = amplitudeControl
                    ? Math.max(1, Math.min(255, Math.round(intensity * 255.0f)))
                    : VibrationEffect.DEFAULT_AMPLITUDE;
                usedDefaultAmplitude = !amplitudeControl && intensity < 1.0f;
                effect = VibrationEffect.createOneShot(durationMillis, amplitude);
            }
            vibrate(vibrator, effect, durationMillis, purpose);
            return usedDefaultAmplitude
                ? RESULT_DEFAULT_AMPLITUDE
                : RESULT_ACCEPTED;
        } catch (SecurityException exception) {
            return RESULT_FAILED;
        } catch (Exception exception) {
            return RESULT_FAILED;
        }
    }

    static int playWaveform(
        Activity activity,
        long requestId,
        long preparedResourceId,
        long[] timingsMilliseconds,
        int[] amplitudes,
        int repeatIndex,
        int purpose,
        long startDelayMillis
    ) {
        recordSubmission(requestId);
        if (startDelayMillis > 0L) {
            final long[] scheduledTimings = timingsMilliseconds;
            final int[] scheduledAmplitudes = amplitudes;
            final int scheduledRepeatIndex = repeatIndex;
            final int scheduledPurpose = purpose;
            return schedulePlayback(
                activity,
                requestId,
                startDelayMillis,
                new ScheduledPlayback() {
                    @Override
                    public int play(Activity current) {
                        return playWaveform(
                            current,
                            0L,
                            preparedResourceId,
                            scheduledTimings,
                            scheduledAmplitudes,
                            scheduledRepeatIndex,
                            scheduledPurpose,
                            0L
                        );
                    }
                }
            );
        }
        if (Build.VERSION.SDK_INT < 26) {
            return RESULT_UNSUPPORTED;
        }
        try {
            Vibrator vibrator = vibrator(activity);
            Context context = applicationContext(activity);
            if (!systemHapticsEnabled(context)) {
                return RESULT_SUPPRESSED;
            }

            long nowMillis = SystemClock.elapsedRealtime();
            PreparedWaveform prepared = null;
            if (preparedResourceId != 0L) {
                synchronized (PREPARED_WAVEFORMS) {
                    prunePreparedWaveformsLocked(nowMillis);
                    prepared = PREPARED_WAVEFORMS.get(preparedResourceId);
                    if (prepared != null) {
                        prepared.lastAccessMillis = nowMillis;
                    }
                }
            }
            if (prepared == null) {
                prepared = createPreparedWaveform(
                    vibrator,
                    timingsMilliseconds,
                    amplitudes,
                    repeatIndex,
                    1L,
                    nowMillis
                );
            }
            if (prepared == null) {
                return RESULT_UNSUPPORTED;
            }
            vibrate(vibrator, prepared.effect, 0L, purpose);
            return prepared.usedDefaultAmplitude
                ? RESULT_DEFAULT_AMPLITUDE
                : RESULT_ACCEPTED;
        } catch (SecurityException exception) {
            return RESULT_FAILED;
        } catch (Exception exception) {
            return RESULT_FAILED;
        }
    }

    static int playPredefined(
        Activity activity,
        long requestId,
        int effect,
        int purpose,
        long startDelayMillis
    ) {
        recordSubmission(requestId);
        if (startDelayMillis > 0L) {
            final int scheduledEffect = effect;
            final int scheduledPurpose = purpose;
            return schedulePlayback(
                activity,
                requestId,
                startDelayMillis,
                new ScheduledPlayback() {
                    @Override
                    public int play(Activity current) {
                        return playPredefined(
                            current,
                            0L,
                            scheduledEffect,
                            scheduledPurpose,
                            0L
                        );
                    }
                }
            );
        }
        if (Build.VERSION.SDK_INT < 29) {
            return RESULT_UNSUPPORTED;
        }
        try {
            int nativeEffect = predefinedEffectFromIntent(effect);
            if (nativeEffect < 0 || Build.VERSION.SDK_INT < 30) {
                return RESULT_UNSUPPORTED;
            }
            Vibrator vibrator = vibrator(activity);
            if (vibrator == null || !vibrator.hasVibrator()) {
                return RESULT_UNSUPPORTED;
            }
            int[] support = vibrator.areEffectsSupported(nativeEffect);
            if (support == null
                || support.length != 1
                || support[0] != Vibrator.VIBRATION_EFFECT_SUPPORT_YES) {
                return RESULT_UNSUPPORTED;
            }
            Context context = applicationContext(activity);
            if (!systemHapticsEnabled(context)) {
                return RESULT_SUPPRESSED;
            }
            VibrationEffect effectValue =
                VibrationEffect.createPredefined(nativeEffect);
            vibrate(vibrator, effectValue, 0L, purpose);
            return RESULT_ACCEPTED;
        } catch (SecurityException exception) {
            return RESULT_FAILED;
        } catch (Exception exception) {
            return RESULT_FAILED;
        }
    }

    static int playPrimitives(
        Activity activity,
        long requestId,
        int[] primitives,
        float[] scales,
        int[] delaysMilliseconds,
        int purpose,
        long startDelayMillis
    ) {
        recordSubmission(requestId);
        if (startDelayMillis > 0L) {
            final int[] scheduledPrimitives = primitives;
            final float[] scheduledScales = scales;
            final int[] scheduledDelays = delaysMilliseconds;
            final int scheduledPurpose = purpose;
            return schedulePlayback(
                activity,
                requestId,
                startDelayMillis,
                new ScheduledPlayback() {
                    @Override
                    public int play(Activity current) {
                        return playPrimitives(
                            current,
                            0L,
                            scheduledPrimitives,
                            scheduledScales,
                            scheduledDelays,
                            scheduledPurpose,
                            0L
                        );
                    }
                }
            );
        }
        if (Build.VERSION.SDK_INT < 30
            || primitives == null
            || scales == null
            || delaysMilliseconds == null
            || primitives.length == 0
            || scales.length != primitives.length
            || delaysMilliseconds.length != primitives.length) {
            return RESULT_UNSUPPORTED;
        }
        try {
            int[] nativePrimitives = new int[primitives.length];
            long totalDelayMilliseconds = 0L;
            for (int index = 0; index < primitives.length; ++index) {
                int nativePrimitive = primitiveId(primitives[index]);
                float scale = scales[index];
                int delayMilliseconds = delaysMilliseconds[index];
                if (nativePrimitive < 0
                    || Float.isNaN(scale)
                    || Float.isInfinite(scale)
                    || scale < 0.0f
                    || scale > 1.0f
                    || delayMilliseconds < 0
                    || delayMilliseconds > 10000) {
                    return RESULT_UNSUPPORTED;
                }
                totalDelayMilliseconds += delayMilliseconds;
                if (totalDelayMilliseconds > 30000L) {
                    return RESULT_UNSUPPORTED;
                }
                nativePrimitives[index] = nativePrimitive;
            }

            Vibrator vibrator = vibrator(activity);
            if (vibrator == null || !vibrator.hasVibrator()) {
                return RESULT_UNSUPPORTED;
            }
            Context context = applicationContext(activity);
            if (!systemHapticsEnabled(context)) {
                return RESULT_SUPPRESSED;
            }
            boolean[] supported = vibrator.arePrimitivesSupported(nativePrimitives);
            if (supported == null
                || supported.length != nativePrimitives.length) {
                return RESULT_UNSUPPORTED;
            }
            for (boolean primitiveSupported : supported) {
                if (!primitiveSupported) {
                    return RESULT_UNSUPPORTED;
                }
            }

            VibrationEffect.Composition composition =
                VibrationEffect.startComposition();
            for (int index = 0; index < nativePrimitives.length; ++index) {
                composition.addPrimitive(
                    nativePrimitives[index],
                    scales[index],
                    delaysMilliseconds[index]
                );
            }
            vibrate(vibrator, composition.compose(), 0L, purpose);
            return RESULT_ACCEPTED;
        } catch (SecurityException exception) {
            return RESULT_FAILED;
        } catch (Exception exception) {
            return RESULT_FAILED;
        }
    }

    static int playEnvelope(
        Activity activity,
        long requestId,
        int format,
        float[] amplitudes,
        float[] controlValues,
        long[] durationsMilliseconds,
        int purpose,
        long startDelayMillis
    ) {
        recordSubmission(requestId);
        if (startDelayMillis > 0L) {
            final int scheduledFormat = format;
            final float[] scheduledAmplitudes = amplitudes;
            final float[] scheduledControlValues = controlValues;
            final long[] scheduledDurations = durationsMilliseconds;
            final int scheduledPurpose = purpose;
            return schedulePlayback(
                activity,
                requestId,
                startDelayMillis,
                new ScheduledPlayback() {
                    @Override
                    public int play(Activity current) {
                        return playEnvelope(
                            current,
                            0L,
                            scheduledFormat,
                            scheduledAmplitudes,
                            scheduledControlValues,
                            scheduledDurations,
                            scheduledPurpose,
                            0L
                        );
                    }
                }
            );
        }
        if (Build.VERSION.SDK_INT < 36
            || (format != 2 && format != 3)
            || amplitudes == null
            || controlValues == null
            || durationsMilliseconds == null
            || amplitudes.length == 0
            || controlValues.length != amplitudes.length
            || durationsMilliseconds.length != amplitudes.length) {
            return RESULT_UNSUPPORTED;
        }
        try {
            return playEnvelopeApi36(
                activity,
                format,
                amplitudes,
                controlValues,
                durationsMilliseconds,
                purpose
            );
        } catch (SecurityException exception) {
            return RESULT_FAILED;
        } catch (Exception exception) {
            return RESULT_FAILED;
        }
    }

    private static int playEnvelopeApi36(
        Activity activity,
        int format,
        float[] amplitudes,
        float[] controlValues,
        long[] durationsMilliseconds,
        int purpose
    ) throws ReflectiveOperationException {
        Vibrator vibrator = vibrator(activity);
        if (vibrator == null || !vibrator.hasVibrator()) {
            return RESULT_UNSUPPORTED;
        }
        EnvelopeApi36 api = envelopeApi36();
        boolean envelopeSupported =
            (Boolean)api.areEnvelopeEffectsSupported.invoke(vibrator);
        if (!envelopeSupported) {
            return RESULT_UNSUPPORTED;
        }
        Object info = api.getEnvelopeEffectInfo.invoke(vibrator);
        if (info == null) {
            return RESULT_UNSUPPORTED;
        }
        int maximumSize = ((Number)api.getMaxSize.invoke(info)).intValue();
        long minimumDuration = ((Number)api.getMinControlPointDurationMillis
            .invoke(info)).longValue();
        long maximumDuration = ((Number)api.getMaxControlPointDurationMillis
            .invoke(info)).longValue();
        long maximumTotalDuration = ((Number)api.getMaxDurationMillis
            .invoke(info)).longValue();
        if (maximumSize <= 0
            || minimumDuration <= 0L
            || maximumDuration < minimumDuration
            || maximumTotalDuration < minimumDuration
            || amplitudes.length > maximumSize) {
            return RESULT_UNSUPPORTED;
        }
        long totalDuration = 0L;
        for (int index = 0; index < amplitudes.length; ++index) {
            float amplitude = amplitudes[index];
            float control = controlValues[index];
            long duration = durationsMilliseconds[index];
            if (Float.isNaN(amplitude)
                || Float.isInfinite(amplitude)
                || amplitude < 0.0f
                || amplitude > 1.0f
                || Float.isNaN(control)
                || Float.isInfinite(control)
                || duration < minimumDuration
                || duration > maximumDuration
                || totalDuration > maximumTotalDuration - duration) {
                return RESULT_UNSUPPORTED;
            }
            totalDuration += duration;
        }

        Context context = applicationContext(activity);
        if (!systemHapticsEnabled(context)) {
            return RESULT_SUPPRESSED;
        }

        VibrationEffect effect;
        if (format == 2) {
            if (amplitudes[amplitudes.length - 1] != 0.0f) {
                return RESULT_UNSUPPORTED;
            }
            for (float sharpness : controlValues) {
                if (sharpness < 0.0f || sharpness > 1.0f) {
                    return RESULT_UNSUPPORTED;
                }
            }
            Object builder = api.basicConstructor.newInstance();
            api.basicSetInitialSharpness.invoke(builder, controlValues[0]);
            for (int index = 0; index < amplitudes.length; ++index) {
                api.basicAddControlPoint.invoke(
                    builder,
                    amplitudes[index],
                    controlValues[index],
                    durationsMilliseconds[index]
                );
            }
            effect = (VibrationEffect)api.basicBuild.invoke(builder);
        } else {
            Object profile = api.getFrequencyProfile.invoke(vibrator);
            if (profile == null) {
                return RESULT_UNSUPPORTED;
            }
            float minimumFrequency = ((Number)api.getMinFrequencyHz
                .invoke(profile)).floatValue();
            float maximumFrequency = ((Number)api.getMaxFrequencyHz
                .invoke(profile)).floatValue();
            if (Float.isNaN(minimumFrequency)
                || Float.isInfinite(minimumFrequency)
                || Float.isNaN(maximumFrequency)
                || Float.isInfinite(maximumFrequency)
                || minimumFrequency <= 0.0f
                || maximumFrequency < minimumFrequency) {
                return RESULT_UNSUPPORTED;
            }
            for (float frequency : controlValues) {
                if (frequency < minimumFrequency
                    || frequency > maximumFrequency) {
                    return RESULT_UNSUPPORTED;
                }
            }
            Object builder = api.waveformConstructor.newInstance();
            api.waveformSetInitialFrequencyHz.invoke(
                builder,
                controlValues[0]
            );
            for (int index = 0; index < amplitudes.length; ++index) {
                api.waveformAddControlPoint.invoke(
                    builder,
                    amplitudes[index],
                    controlValues[index],
                    durationsMilliseconds[index]
                );
            }
            effect = (VibrationEffect)api.waveformBuild.invoke(builder);
        }
        vibrate(vibrator, effect, 0L, purpose);
        return RESULT_ACCEPTED;
    }

    static boolean stopAll(Activity activity) {
        cancelScheduledRequests();
        clearControlledWaveform();
        try {
            Vibrator vibrator = vibrator(activity);
            if (vibrator == null) {
                return true;
            }
            vibrator.cancel();
            return true;
        } catch (SecurityException exception) {
            return false;
        } catch (Exception exception) {
            return false;
        }
    }

    static long[] snapshotInstrumentation() {
        return new long[] {
            BRIDGE_VERSION,
            submissionCount.get(),
            callbackCount.get(),
            lastRequestId.get()
        };
    }

    private static int playVibration(
        Activity activity,
        int behavior,
        float intensity,
        int path,
        int purpose
    ) {
        try {
            Vibrator vibrator = vibrator(activity);
            if (vibrator == null || !vibrator.hasVibrator()) {
                return RESULT_UNSUPPORTED;
            }
            Context context = applicationContext(activity);
            if (!systemHapticsEnabled(context)) {
                return RESULT_SUPPRESSED;
            }
            VibrationEffect effect = null;
            if (path == 2) {
                if (Build.VERSION.SDK_INT < 30) {
                    return RESULT_UNSUPPORTED;
                }
                int nativeEffect = semanticPredefinedEffect(behavior);
                int[] support = vibrator.areEffectsSupported(nativeEffect);
                if (support == null
                    || support.length != 1
                    || support[0] != Vibrator.VIBRATION_EFFECT_SUPPORT_YES) {
                    return RESULT_UNSUPPORTED;
                }
                effect = VibrationEffect.createPredefined(
                    nativeEffect
                );
            } else if (path != 3) {
                return RESULT_UNSUPPORTED;
            }
            long durationMillis = durationMillis(behavior);
            if (effect == null && Build.VERSION.SDK_INT >= 26) {
                int amplitude = vibrator.hasAmplitudeControl()
                    ? Math.max(1, Math.min(255, Math.round(intensity * 255.0f)))
                    : VibrationEffect.DEFAULT_AMPLITUDE;
                effect = VibrationEffect.createOneShot(durationMillis, amplitude);
            }
            vibrate(vibrator, effect, durationMillis, purpose);
            return RESULT_FALLBACK;
        } catch (SecurityException exception) {
            return RESULT_FAILED;
        } catch (Exception exception) {
            return RESULT_FAILED;
        }
    }

    private static int performSemantic(Activity activity, int behavior) {
        if (!isUsable(activity)) {
            return RESULT_SUPPRESSED;
        }
        View view = activity.getWindow() != null
            ? activity.getWindow().getDecorView()
            : null;
        if (view == null || !view.isShown() || !view.hasWindowFocus()) {
            return RESULT_SUPPRESSED;
        }
        interruptControlledWaveform();
        return view.performHapticFeedback(feedbackConstant(behavior))
            ? RESULT_ACCEPTED
            : RESULT_SUPPRESSED;
    }

    private static boolean isUsable(Activity activity) {
        return activity != null
            && !activity.isFinishing()
            && (Build.VERSION.SDK_INT < 17 || !activity.isDestroyed());
    }

    private static Context applicationContext(Activity activity) {
        if (activity == null) {
            return null;
        }
        Context context = activity.getApplicationContext();
        return context != null ? context : activity;
    }

    private static Vibrator vibrator(Activity activity) {
        Context context = applicationContext(activity);
        if (context == null) {
            return null;
        }
        if (Build.VERSION.SDK_INT >= 31) {
            VibratorManager manager =
                (VibratorManager)context.getSystemService(
                    Context.VIBRATOR_MANAGER_SERVICE
                );
            return manager != null ? manager.getDefaultVibrator() : null;
        }
        return (Vibrator)context.getSystemService(Context.VIBRATOR_SERVICE);
    }

    private static boolean systemHapticsEnabled(Context context) {
        return context != null && Settings.System.getInt(
            context.getContentResolver(),
            Settings.System.HAPTIC_FEEDBACK_ENABLED,
            1
        ) != 0;
    }

    private static void vibrate(
        Vibrator vibrator,
        VibrationEffect effect,
        long durationMillis,
        int purpose
    ) {
        interruptControlledWaveform();
        vibrateNative(vibrator, effect, durationMillis, purpose);
    }

    private static void vibrateControlled(
        Vibrator vibrator,
        VibrationEffect effect,
        int purpose
    ) {
        vibrateNative(vibrator, effect, 0L, purpose);
    }

    private static void vibrateNative(
        Vibrator vibrator,
        VibrationEffect effect,
        long durationMillis,
        int purpose
    ) {
        if (Build.VERSION.SDK_INT >= 33) {
            vibrator.vibrate(
                effect,
                VibrationAttributes.createForUsage(vibrationUsage(purpose))
            );
        } else if (Build.VERSION.SDK_INT >= 26) {
            vibrator.vibrate(effect, audioAttributes(purpose));
        } else {
            vibrator.vibrate(durationMillis, audioAttributes(purpose));
        }
    }

    private static int vibrationUsage(int purpose) {
        if (purpose == 1) {
            return VibrationAttributes.USAGE_MEDIA;
        }
        if (purpose == 2) {
            return VibrationAttributes.USAGE_NOTIFICATION;
        }
        return VibrationAttributes.USAGE_TOUCH;
    }

    private static AudioAttributes audioAttributes(int purpose) {
        int usage = AudioAttributes.USAGE_ASSISTANCE_SONIFICATION;
        if (purpose == 1) {
            usage = AudioAttributes.USAGE_GAME;
        } else if (purpose == 2) {
            usage = AudioAttributes.USAGE_NOTIFICATION_EVENT;
        }
        return new AudioAttributes.Builder()
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .setUsage(usage)
            .build();
    }

    private static int feedbackConstant(int behavior) {
        switch (behavior) {
            case 0:
                return Build.VERSION.SDK_INT >= 34
                    ? HapticFeedbackConstants.SEGMENT_FREQUENT_TICK
                    : HapticFeedbackConstants.CLOCK_TICK;
            case 1:
                return HapticFeedbackConstants.VIRTUAL_KEY;
            case 2:
                return Build.VERSION.SDK_INT >= 23
                    ? HapticFeedbackConstants.CONTEXT_CLICK
                    : HapticFeedbackConstants.LONG_PRESS;
            case 3:
                return HapticFeedbackConstants.LONG_PRESS;
            case 4:
                return HapticFeedbackConstants.CLOCK_TICK;
            case 5:
                return Build.VERSION.SDK_INT >= 23
                    ? HapticFeedbackConstants.CONTEXT_CLICK
                    : HapticFeedbackConstants.LONG_PRESS;
            case 6:
                return Build.VERSION.SDK_INT >= 30
                    ? HapticFeedbackConstants.CONFIRM
                    : HapticFeedbackConstants.VIRTUAL_KEY;
            case 7:
                return Build.VERSION.SDK_INT >= 30
                    ? HapticFeedbackConstants.GESTURE_END
                    : HapticFeedbackConstants.LONG_PRESS;
            default:
                return Build.VERSION.SDK_INT >= 30
                    ? HapticFeedbackConstants.REJECT
                    : HapticFeedbackConstants.LONG_PRESS;
        }
    }

    private static int semanticPredefinedEffect(int behavior) {
        switch (behavior) {
            case 0:
            case 4:
                return VibrationEffect.EFFECT_TICK;
            case 1:
            case 5:
                return VibrationEffect.EFFECT_CLICK;
            case 6:
                return VibrationEffect.EFFECT_DOUBLE_CLICK;
            default:
                return VibrationEffect.EFFECT_HEAVY_CLICK;
        }
    }

    private static int predefinedEffectFromIntent(int effect) {
        switch (effect) {
            case 0:
                return VibrationEffect.EFFECT_TICK;
            case 1:
                return VibrationEffect.EFFECT_CLICK;
            case 2:
                return VibrationEffect.EFFECT_HEAVY_CLICK;
            case 3:
                return VibrationEffect.EFFECT_DOUBLE_CLICK;
            default:
                return -1;
        }
    }

    private static int primitiveId(int primitive) {
        switch (primitive) {
            case 0:
                return VibrationEffect.Composition.PRIMITIVE_TICK;
            case 1:
                return VibrationEffect.Composition.PRIMITIVE_LOW_TICK;
            case 2:
                return VibrationEffect.Composition.PRIMITIVE_CLICK;
            case 3:
                return VibrationEffect.Composition.PRIMITIVE_THUD;
            case 4:
                return VibrationEffect.Composition.PRIMITIVE_SPIN;
            case 5:
                return VibrationEffect.Composition.PRIMITIVE_QUICK_RISE;
            case 6:
                return VibrationEffect.Composition.PRIMITIVE_SLOW_RISE;
            case 7:
                return VibrationEffect.Composition.PRIMITIVE_QUICK_FALL;
            default:
                return -1;
        }
    }

    private static long durationMillis(int behavior) {
        switch (behavior) {
            case 0:
            case 1:
            case 4:
                return 15L;
            case 2:
            case 5:
                return 25L;
            case 3:
            case 7:
            case 8:
                return 40L;
            default:
                return 30L;
        }
    }

    private static void recordSubmission(long requestId) {
        if (requestId == 0L) {
            return;
        }
        lastRequestId.set(requestId);
        incrementBounded(submissionCount);
    }

    private static void incrementBounded(AtomicLong value) {
        long current;
        do {
            current = value.get();
            if (current == Long.MAX_VALUE) {
                return;
            }
        } while (!value.compareAndSet(current, current + 1));
    }

    private static native boolean nativeCanStart(long requestId);
    private static native void nativeOnBridgeResult(long requestId, int result);
    private static native void nativeOnControlledWaveformEvent(
        long requestId,
        long controlRevision,
        int event
    );
}
