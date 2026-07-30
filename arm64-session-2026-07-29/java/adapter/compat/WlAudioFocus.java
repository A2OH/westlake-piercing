package adapter.compat;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * WESTLAKE §459 — grant audio focus (gate 2 of the audio chain, ported to arm64).
 *
 * The adapter stamps {@code AudioManager.sService} with a dynamic-Proxy stub that returns a type
 * default for everything, so {@code IAudioService.requestAudioFocus(...)} answers **0** =
 * AUDIOFOCUS_REQUEST_FAILED and the player refuses to start. The old arm32 board solved this with a
 * framework dex patch (PatchReturnOne); here we wrap the stub instead, which needs no boot-image or
 * framework.jar change.
 *
 * As in {@link WlAmsBind}, the wrapper must NOT delegate with {@code method.invoke(delegate, ...)} —
 * the delegate is itself a Proxy and a reflective invoke on a proxy SEGVs this runtime (§438).
 * The stub only returns type defaults, so we replicate that directly.
 */
public final class WlAudioFocus {

    private WlAudioFocus() {}

    static final class Handler3 implements InvocationHandler {
        @Override
        public Object invoke(Object proxy, Method method, Object[] args) {
            final String n = method.getName();
            if (n.equals("requestAudioFocus")) {
                return Integer.valueOf(1);              // AUDIOFOCUS_REQUEST_GRANTED
            }
            if (n.equals("abandonAudioFocus")) {
                return Integer.valueOf(1);              // AUDIOFOCUS_REQUEST_GRANTED
            }
            if ("asBinder".equals(n))  return proxy;
            if ("toString".equals(n))  return "WlAudioFocus";
            if ("hashCode".equals(n))  return Integer.valueOf(System.identityHashCode(proxy));
            if ("equals".equals(n))    return Boolean.valueOf(args != null && args.length == 1
                                                              && args[0] == proxy);
            Class<?> rt = method.getReturnType();
            if (rt == boolean.class) return Boolean.FALSE;
            if (rt == byte.class)    return Byte.valueOf((byte) 0);
            if (rt == short.class)   return Short.valueOf((short) 0);
            if (rt == int.class)     return Integer.valueOf(0);
            if (rt == long.class)    return Long.valueOf(0L);
            if (rt == float.class)   return Float.valueOf(0f);
            if (rt == double.class)  return Double.valueOf(0d);
            if (rt == char.class)    return Character.valueOf((char) 0);
            return null;
        }
    }

    public static String install() {
        String r = installOnce();
        if (r != null && r.startsWith("sService is null")) {
            Thread t = new Thread(new Waiter(), "wl-audio-focus");
            t.setDaemon(true);
            t.start();
            return r + " -> waiting on a daemon thread";
        }
        return r;
    }

    static final class Waiter implements Runnable {
        @Override public void run() {
            for (int i = 0; i < 600; i++) {
                try { Thread.sleep(200); } catch (InterruptedException e) { return; }
                String r = installOnce();
                if (r != null && !r.startsWith("sService is null")) {
                    System.err.println("[WESTLAKE-459] audio focus (deferred): " + r);
                    System.err.flush();
                    return;
                }
            }
            System.err.println("[WESTLAKE-459] audio focus: gave up waiting for sService");
            System.err.flush();
        }
    }

    private static String installOnce() {
        try {
            Class<?> am = Class.forName("android.media.AudioManager");
            Field f = am.getDeclaredField("sService");
            f.setAccessible(true);
            Object cur = f.get(null);
            if (cur == null) return "sService is null (not stamped yet)";
            if (Proxy.isProxyClass(cur.getClass())
                    && Proxy.getInvocationHandler(cur) instanceof Handler3) {
                return "already wrapped";
            }
            Class<?> ias = Class.forName("android.media.IAudioService");
            Class<?> iBinder = Class.forName("android.os.IBinder");
            Object wrapped = Proxy.newProxyInstance(WlAudioFocus.class.getClassLoader(),
                    new Class<?>[] { ias, iBinder }, new Handler3());
            f.set(null, wrapped);
            return "wrapped IAudioService (requestAudioFocus -> GRANTED)";
        } catch (Throwable t) {
            return "FAILED: " + t.getClass().getName() + ": " + t.getMessage();
        }
    }
}
