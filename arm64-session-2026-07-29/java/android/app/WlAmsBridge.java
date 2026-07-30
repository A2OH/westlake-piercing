package android.app;

import android.content.IIntentReceiver;
import android.content.IIntentSender;
import android.content.Intent;
import android.content.IntentFilter;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * WESTLAKE §464 — framework-side bypass for the invoke-interface-on-Proxy defect (§436).
 *
 * The adapter's IActivityManager is a java.lang.reflect.Proxy. Our libart patch re-validates the
 * dispatched method with GetNameView()/GetSignature(), which AOSP forbids on proxy methods, so any
 * invoke-interface with a Proxy receiver throws
 *   NoSuchMethodError: No InvokeType(4) method <name> in class Landroid/app/IActivityManager;
 * That killed SoundPlaybackService.onCreate on both registerReceiver and PendingIntent.getActivity.
 *
 * These helpers reach the same target without a proxy receiver: fetch the InvocationHandler (a
 * normal class) and call invoke() on it. Call sites are rewritten invoke-interface/range ->
 * invoke-static/range, which is the same 3rc format and size, so the register list is untouched.
 */
public final class WlAmsBridge {

    private WlAmsBridge() {}

    private static Object via(Object recv, String name, int argc, Object[] args) throws Throwable {
        if (recv == null) throw new NullPointerException("WlAmsBridge: null receiver for " + name);
        InvocationHandler h = Proxy.getInvocationHandler(recv);
        Method target = null;
        Class<?>[] ifaces = recv.getClass().getInterfaces();
        for (int i = 0; i < ifaces.length && target == null; i++) {
            Method[] ms = ifaces[i].getDeclaredMethods();
            for (int k = 0; k < ms.length; k++) {
                if (ms[k].getName().equals(name) && ms[k].getParameterTypes().length == argc) {
                    target = ms[k];
                    break;
                }
            }
        }
        if (target == null) throw new NoSuchMethodError("WlAmsBridge: " + name + " on " + recv.getClass());
        return h.invoke(recv, target, args);
    }

    public static Intent registerReceiverWithFeature(IActivityManager am, IApplicationThread caller,
            String callingPackage, String callingFeatureId, String receiverId,
            IIntentReceiver receiver, IntentFilter filter, String requiredPermission,
            int userId, int flags) throws Throwable {
        return (Intent) via(am, "registerReceiverWithFeature", 9, new Object[] {
                caller, callingPackage, callingFeatureId, receiverId, receiver, filter,
                requiredPermission, Integer.valueOf(userId), Integer.valueOf(flags) });
    }

    public static IIntentSender getIntentSenderWithFeature(IActivityManager am, int type,
            String packageName, String featureId, android.os.IBinder token, String resultWho,
            int requestCode, Intent[] intents, String[] resolvedTypes, int flags,
            android.os.Bundle options, int userId) throws Throwable {
        return (IIntentSender) via(am, "getIntentSenderWithFeature", 11, new Object[] {
                Integer.valueOf(type), packageName, featureId, token, resultWho,
                Integer.valueOf(requestCode), intents, resolvedTypes, Integer.valueOf(flags),
                options, Integer.valueOf(userId) });
    }
}
