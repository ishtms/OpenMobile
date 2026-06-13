package com.openmobile.haptics;

import android.app.Activity;
import android.content.Context;
import android.media.AudioAttributes;
import android.os.Build;
import android.os.Looper;
import android.os.VibrationAttributes;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.provider.Settings;
import android.view.HapticFeedbackConstants;
import android.view.View;
import java.lang.ref.WeakReference;
import java.lang.reflect.Method;
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

    private static final AtomicLong submissionCount = new AtomicLong();
    private static final AtomicLong callbackCount = new AtomicLong();
    private static final AtomicLong lastRequestId = new AtomicLong();

    private OpenMobileHapticsBridgeV1() {
    }

    static long[] queryCapabilities(Activity activity) {
        try {
            Vibrator vibrator = vibrator(activity);
            if (vibrator == null) {
                return new long[] {-1L};
            }
            if (!vibrator.hasVibrator()) {
                return new long[] {0L, 0L, 0L, -1L, -1L, -1L};
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
                        }
                        Object frequencyProfile = Vibrator.class.getMethod(
                            "getFrequencyProfile"
                        ).invoke(vibrator);
                        if (frequencyProfile != null) {
                            flags |= 512L;
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
                minTimingMillis
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
        int purpose
    ) {
        recordSubmission(requestId);
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
        int purpose
    ) {
        recordSubmission(requestId);
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
            boolean usedFallback = false;
            boolean usedDefaultAmplitude = false;
            if (path == 2) {
                if (Build.VERSION.SDK_INT >= 29) {
                    effect = VibrationEffect.createPredefined(
                        VibrationEffect.EFFECT_CLICK
                    );
                } else {
                    usedFallback = true;
                }
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
                : usedFallback ? RESULT_FALLBACK : RESULT_ACCEPTED;
        } catch (SecurityException exception) {
            return RESULT_FAILED;
        } catch (Exception exception) {
            return RESULT_FAILED;
        }
    }

    static boolean stopAll(Activity activity) {
        try {
            Vibrator vibrator = vibrator(activity);
            if (vibrator == null) {
                return false;
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
            if (path == 2 && Build.VERSION.SDK_INT >= 29) {
                effect = VibrationEffect.createPredefined(
                    predefinedEffect(behavior)
                );
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

    private static int predefinedEffect(int behavior) {
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

    private static native void nativeOnBridgeResult(long requestId, int result);
}
