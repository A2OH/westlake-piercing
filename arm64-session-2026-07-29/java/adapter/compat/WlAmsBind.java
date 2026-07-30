package adapter.compat;

import android.content.ComponentName;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * WESTLAKE §458/§465 — make bindService AND startService actually work (the audio gates).
 *
 * AppSpawnXInit installs an IActivityManager dynamic-Proxy stub that returns a type default for
 * every method. Two consequences killed audio:
 *   §458  bindServiceInstance() -> 0, so SoundPlaybackService was never created or connected.
 *   §465  startService() -> null, so even once created it never received onStartCommand — which is
 *         how a media service is actually told to play. That is why tapping play produced no
 *         player, no CDN segment request and no codec activity, with no exception anywhere.
 *
 * We wrap the stub and do in-process what ActivityThread would: instantiate the Service, attach it,
 * onCreate, then either onBind + IServiceConnection.connected (bind) or onStartCommand (start).
 *
 * ⚠️Never delegate with method.invoke(delegate, ...): the delegate is itself a Proxy and a
 * reflective invoke on a proxy SEGVs this runtime (§438) — A/B proven. The stub only returns type
 * defaults, so they are replicated inline below.
 */
public final class WlAmsBind {

    private WlAmsBind() {}

    // ---- the wrapper -------------------------------------------------------------------------
    static final class Handler2 implements InvocationHandler {
        Handler2(Object delegateUnused) { }

        @Override
        public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
            final String n = method.getName();
            if (n.startsWith("bindService") || n.equals("bindIsolatedService")) {
                Object r = route(method, args, false);
                if (r != null) return r;
            }
            if (n.startsWith("startService") || n.startsWith("startForegroundService")) {
                Object r = route(method, args, true);
                if (r != null) return r;
            }
            if (n.startsWith("getIntentSender")) {
                Object s = intentSender();
                if (s != null) return s;
            }
            if ("asBinder".equals(n))  return proxy;
            if ("toString".equals(n))  return "WlAmsBind";
            if ("hashCode".equals(n))  return Integer.valueOf(System.identityHashCode(proxy));
            if ("equals".equals(n))    return Boolean.valueOf(args != null && args.length == 1
                                                              && args[0] == proxy);
            return defaultFor(method.getReturnType());
        }

        /** Shared bind/start routing. Returns null to fall through to the default. */
        private Object route(Method method, Object[] args, boolean start) {
            try {
                if (args == null) return null;
                Intent intent = null;
                Object conn = null;
                Class<?> connIface = Class.forName("android.app.IServiceConnection");
                for (Object a : args) {
                    if (a instanceof Intent && intent == null) intent = (Intent) a;
                    else if (a != null && conn == null && connIface.isInstance(a)) conn = a;
                }
                if (intent == null) return null;
                if (!start && conn == null) return null;
                ComponentName comp = intent.getComponent();
                if (comp == null) return null;

                Object app = call("android.app.ActivityThread", "currentApplication");
                if (app == null) return null;
                String pkg = (String) app.getClass().getMethod("getPackageName").invoke(app);
                if (pkg == null || !pkg.equals(comp.getPackageName())) return null;   // not our app

                Runnable task = start ? (Runnable) new StartTask(intent, comp)
                                      : (Runnable) new BindTask(intent, comp, conn);
                new Handler(Looper.getMainLooper()).post(task);
                System.err.println("[WESTLAKE-" + (start ? "465" : "458") + "] routed "
                        + method.getName() + " -> in-process " + comp.getClassName());
                System.err.flush();

                Class<?> rt = method.getReturnType();
                if (rt.getName().equals("android.content.ComponentName")) return comp;
                if (rt == boolean.class) return Boolean.TRUE;
                if (rt == int.class) return Integer.valueOf(start ? 0 : 1);
                return null;
            } catch (Throwable t) {
                System.err.println("[WESTLAKE-458] routing failed: " + t);
                System.err.flush();
                return null;
            }
        }

        private static Object defaultFor(Class<?> rt) {
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

    // ---- named tasks (d8 cannot compile javac-21 anonymous inner classes) ----------------------
    static final class BindTask implements Runnable {
        private final Intent intent; private final ComponentName comp; private final Object conn;
        BindTask(Intent i, ComponentName c, Object k) { intent = i; comp = c; conn = k; }
        @Override public void run() { createAndConnect(intent, comp, conn); }
    }

    static final class StartTask implements Runnable {
        private final Intent intent; private final ComponentName comp;
        StartTask(Intent i, ComponentName c) { intent = i; comp = c; }
        @Override public void run() { startInProcess(intent, comp); }
    }

    // ---- in-process service lifecycle ---------------------------------------------------------
    private static final java.util.Map<String, Object> sServices =
            new java.util.concurrent.ConcurrentHashMap<String, Object>();
    private static final java.util.concurrent.atomic.AtomicInteger sStartId =
            new java.util.concurrent.atomic.AtomicInteger(1);

    private static Object call(String cls, String m) throws Exception {
        return Class.forName(cls).getMethod(m).invoke(null);
    }

    /** What ActivityThread.handleCreateService does. Must run on the main looper. */
    static synchronized Object ensureService(String cls) {
        Object svc = sServices.get(cls);
        if (svc != null) return svc;
        try {
            Object app = call("android.app.ActivityThread", "currentApplication");
            Object thread = call("android.app.ActivityThread", "currentActivityThread");
            ClassLoader cl = (ClassLoader) app.getClass().getMethod("getClassLoader").invoke(app);
            Class<?> svcClass = cl.loadClass(cls);
            svc = svcClass.newInstance();
            Class<?> serviceCls = Class.forName("android.app.Service");
            Method attach = null;
            for (Method m : serviceCls.getDeclaredMethods()) {
                if (m.getName().equals("attach") && m.getParameterTypes().length == 6) { attach = m; break; }
            }
            if (attach == null) throw new NoSuchMethodException("Service.attach/6");
            attach.setAccessible(true);
            attach.invoke(svc, app, thread, cls, new android.os.Binder(), app, null);
            serviceCls.getMethod("onCreate").invoke(svc);
            sServices.put(cls, svc);
            System.err.println("[WESTLAKE-458] created in-process service " + cls);
            System.err.flush();
            return svc;
        } catch (Throwable t) {
            report("service create FAILED for " + cls, t);
            return null;
        }
    }

    static void createAndConnect(Intent intent, ComponentName comp, Object conn) {
        final String cls = comp.getClassName();
        try {
            Object svc = ensureService(cls);
            if (svc == null) return;
            Object binder = Class.forName("android.app.Service")
                    .getMethod("onBind", Intent.class).invoke(svc, intent);
            for (Method m : conn.getClass().getMethods()) {
                if (m.getName().equals("connected") && m.getParameterTypes().length == 3) {
                    m.invoke(conn, comp, binder, Boolean.FALSE);
                    System.err.println("[WESTLAKE-458] connected " + cls + " binder=" + binder);
                    System.err.flush();
                    return;
                }
            }
            System.err.println("[WESTLAKE-458] no IServiceConnection.connected/3");
            System.err.flush();
        } catch (Throwable t) {
            report("in-process bind FAILED for " + cls, t);
        }
    }

    static void startInProcess(Intent intent, ComponentName comp) {
        final String cls = comp.getClassName();
        try {
            Object svc = ensureService(cls);
            if (svc == null) return;
            probeAudioAttributesCompat();
            int id = sStartId.getAndIncrement();
            Method osc = Class.forName("android.app.Service")
                    .getMethod("onStartCommand", Intent.class, int.class, int.class);
            Object r = osc.invoke(svc, intent, Integer.valueOf(0), Integer.valueOf(id));
            System.err.println("[WESTLAKE-465] onStartCommand " + cls + " startId=" + id + " -> " + r);
            System.err.flush();
        } catch (Throwable t) {
            report("onStartCommand FAILED for " + cls, t);
        }
    }

    /**
     * §466: noice's onStartCommand dies with "Parameter specified as non-null is null:
     * engine.a.c parameter audioAttributes", where the type is androidx.media.AudioAttributesCompat.
     * That is an APP class, so it must be probed through the app's ClassLoader — a BCP class cannot
     * see AndroidX types. Runs once, right before onStartCommand.
     */
    private static boolean sProbedAac;

    /**
     * §466: SoundPlayerManager.<clinit> runs PARTIALLY on this runtime — it sets its first statics
     * and then fails while constructing its two androidx.media.AudioAttributesCompat values, which
     * stay null. SoundPlayerManager.<init> then hands one to engine.a.c(AudioAttributesCompat),
     * whose Kotlin non-null check throws inside onStartCommand and kills playback before any player
     * exists. Re-running <clinit> would fail identically, so build the values ourselves and assign
     * them. AudioAttributesCompat.Builder was minified away, so use the same path the app's own
     * code uses: new AudioAttributesCompat(AudioAttributesImpl) over AudioAttributesImplApi21.
     */
    static synchronized void probeAudioAttributesCompat() {
        if (sProbedAac) return;
        sProbedAac = true;
        try {
            Object app = call("android.app.ActivityThread", "currentApplication");
            ClassLoader cl = (ClassLoader) app.getClass().getMethod("getClassLoader").invoke(app);
            Class<?> spm = cl.loadClass("com.github.ashutoshgngwr.noice.engine.SoundPlayerManager");
            Class<?> aacCls = cl.loadClass("androidx.media.AudioAttributesCompat");

            int repaired = 0, alreadySet = 0;
            java.lang.reflect.Field[] fs = spm.getDeclaredFields();
            // usage/contentType pairs: first null field gets MEDIA, any further ones get ALARM.
            int[][] want = new int[][] { { 1, 2 }, { 4, 2 } };   // USAGE_MEDIA/ALARM, CONTENT_MUSIC
            for (int i = 0; i < fs.length; i++) {
                if (!java.lang.reflect.Modifier.isStatic(fs[i].getModifiers())) continue;
                if (!aacCls.isAssignableFrom(fs[i].getType())) continue;
                fs[i].setAccessible(true);
                if (fs[i].get(null) != null) { alreadySet++; continue; }
                int[] w = want[repaired < want.length ? repaired : want.length - 1];
                Object v = buildCompat(cl, aacCls, w[0], w[1]);
                if (v == null) { System.err.println("[WESTLAKE-466] could not build AudioAttributesCompat"); break; }
                fs[i].set(null, v);
                System.err.println("[WESTLAKE-466] repaired SoundPlayerManager." + fs[i].getName()
                        + " usage=" + w[0] + " contentType=" + w[1]);
                repaired++;
            }
            System.err.println("[WESTLAKE-466] AudioAttributesCompat statics: repaired=" + repaired
                    + " alreadySet=" + alreadySet);
            System.err.flush();
        } catch (Throwable t) {
            Throwable c = (t instanceof java.lang.reflect.InvocationTargetException && t.getCause() != null)
                    ? t.getCause() : t;
            System.err.println("[WESTLAKE-466] static repair failed: " + c);
            System.err.flush();
        }
    }

    /** new AudioAttributesCompat(new AudioAttributesImplApi21(android.media.AudioAttributes)) */
    private static Object buildCompat(ClassLoader cl, Class<?> aacCls, int usage, int contentType) {
        try {
            Class<?> aaB = Class.forName("android.media.AudioAttributes$Builder");
            Object b = aaB.getConstructor().newInstance();
            aaB.getMethod("setUsage", int.class).invoke(b, Integer.valueOf(usage));
            aaB.getMethod("setContentType", int.class).invoke(b, Integer.valueOf(contentType));
            Object aa = aaB.getMethod("build").invoke(b);
            if (aa == null) return null;

            Class<?> implIface = cl.loadClass("androidx.media.AudioAttributesImpl");
            Class<?> api21 = cl.loadClass("androidx.media.AudioAttributesImplApi21");
            Object impl = null;
            java.lang.reflect.Constructor<?>[] cs = api21.getDeclaredConstructors();
            for (int i = 0; i < cs.length && impl == null; i++) {
                Class<?>[] pt = cs[i].getParameterTypes();
                cs[i].setAccessible(true);
                if (pt.length == 1 && pt[0].isInstance(aa)) impl = cs[i].newInstance(aa);
                else if (pt.length == 2 && pt[0].isInstance(aa) && pt[1] == int.class)
                    impl = cs[i].newInstance(aa, Integer.valueOf(-1));
            }
            if (impl == null) { System.err.println("[WESTLAKE-466] no usable AudioAttributesImplApi21 ctor"); return null; }

            java.lang.reflect.Constructor<?>[] acs = aacCls.getDeclaredConstructors();
            for (int i = 0; i < acs.length; i++) {
                Class<?>[] pt = acs[i].getParameterTypes();
                if (pt.length == 1 && pt[0].isAssignableFrom(implIface)) {
                    acs[i].setAccessible(true);
                    return acs[i].newInstance(impl);
                }
            }
            System.err.println("[WESTLAKE-466] no AudioAttributesCompat(AudioAttributesImpl) ctor");
            return null;
        } catch (Throwable t) {
            Throwable c = (t instanceof java.lang.reflect.InvocationTargetException && t.getCause() != null)
                    ? t.getCause() : t;
            System.err.println("[WESTLAKE-466] buildCompat failed: " + c);
            return null;
        }
    }

    private static void report(String what, Throwable t) {
        Throwable c = (t instanceof java.lang.reflect.InvocationTargetException && t.getCause() != null)
                ? t.getCause() : t;
        System.err.println("[WESTLAKE-458] " + what + ": " + c);
        c.printStackTrace();
        System.err.flush();
    }

    // ---- inert IIntentSender so PendingIntent construction succeeds (§464b) --------------------
    private static Object sIntentSender;

    private static synchronized Object intentSender() {
        if (sIntentSender != null) return sIntentSender;
        try {
            Class<?> iis = Class.forName("android.content.IIntentSender");
            Class<?> ib = Class.forName("android.os.IBinder");
            sIntentSender = Proxy.newProxyInstance(WlAmsBind.class.getClassLoader(),
                    new Class<?>[] { iis, ib }, new SenderHandler());
            System.err.println("[WESTLAKE-458] built inert IIntentSender");
            System.err.flush();
        } catch (Throwable t) {
            System.err.println("[WESTLAKE-458] could not build IIntentSender: " + t);
            System.err.flush();
        }
        return sIntentSender;
    }

    static final class SenderHandler implements InvocationHandler {
        @Override public Object invoke(Object proxy, Method method, Object[] args) {
            final String n = method.getName();
            if ("asBinder".equals(n)) return proxy;
            if ("toString".equals(n)) return "WlIntentSender";
            if ("hashCode".equals(n)) return Integer.valueOf(System.identityHashCode(proxy));
            if ("equals".equals(n))   return Boolean.valueOf(args != null && args.length == 1
                                                             && args[0] == proxy);
            Class<?> rt = method.getReturnType();
            if (rt == boolean.class) return Boolean.FALSE;
            if (rt == int.class)     return Integer.valueOf(0);
            if (rt == long.class)    return Long.valueOf(0L);
            return null;
        }
    }

    // ---- installation (the bridge runs before AppSpawnXInit, so wait for the stub) -------------
    public static String install() {
        String r = installOnce();
        if (r != null && r.startsWith("mInstance is null")) {
            Thread t = new Thread(new Waiter(), "wl-ams-bind");
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
                if (r != null && !r.startsWith("mInstance is null")) {
                    System.err.println("[WESTLAKE-458] ams bind (deferred): " + r);
                    System.err.flush();
                    return;
                }
            }
            System.err.println("[WESTLAKE-458] ams bind: gave up waiting for mInstance");
            System.err.flush();
        }
    }

    private static String installOnce() {
        try {
            Class<?> amCls = Class.forName("android.app.ActivityManager");
            Field singletonF = amCls.getDeclaredField("IActivityManagerSingleton");
            singletonF.setAccessible(true);
            Object singleton = singletonF.get(null);
            if (singleton == null) return "IActivityManagerSingleton is null";
            Field instF = null;
            for (Class<?> c = singleton.getClass(); c != null && instF == null; c = c.getSuperclass()) {
                try { instF = c.getDeclaredField("mInstance"); } catch (NoSuchFieldException ignored) { }
            }
            if (instF == null) return "no mInstance field";
            instF.setAccessible(true);
            Object orig = instF.get(singleton);
            if (orig == null) return "mInstance is null (AppSpawnXInit not run yet?)";
            if (Proxy.isProxyClass(orig.getClass())
                    && Proxy.getInvocationHandler(orig) instanceof Handler2) {
                return "already wrapped";
            }
            Class<?> iam = Class.forName("android.app.IActivityManager");
            Class<?> iBinder = Class.forName("android.os.IBinder");
            Object wrapped = Proxy.newProxyInstance(WlAmsBind.class.getClassLoader(),
                    new Class<?>[] { iam, iBinder }, new Handler2(orig));
            instF.set(singleton, wrapped);
            return "wrapped IActivityManager (bind+start -> in-process service)";
        } catch (Throwable t) {
            return "FAILED: " + t.getClass().getName() + ": " + t.getMessage();
        }
    }
}
