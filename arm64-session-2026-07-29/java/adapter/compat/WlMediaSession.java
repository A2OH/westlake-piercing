package adapter.compat;

import android.os.Binder;
import android.os.IBinder;
import android.os.IInterface;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * WESTLAKE §467 — provide a "media_session" system service.
 *
 * noice's SoundPlaybackService builds a MediaSession, and this runtime has no session service, so
 * onStartCommand died with:
 *   NullPointerException: Attempt to invoke InvokeType(2) method
 *   'ISession MediaSessionManager.createSession(MediaSession$CallbackStub, String, Bundle)'
 *   on a null object reference
 * (i.e. MediaSessionManager's ISessionManager was null).
 *
 * Uses the §454 recipe: seed ServiceManager.sCache with a Binder whose queryLocalInterface() returns
 * a local ISessionManager implementation, so ISessionManager.Stub.asInterface() uses it directly and
 * never transacts. createSession() hands back an inert ISession; anything that must be non-null is
 * answered with another inert proxy rather than null, since null is what crashes callers.
 */
public final class WlMediaSession {

    private WlMediaSession() {}

    public static final class LocalBinder extends Binder {
        private IInterface iface;
        void setIface(IInterface i) { this.iface = i; }
        @Override public IInterface queryLocalInterface(String descriptor) { return iface; }
    }

    /** Generic inert handler: proxies for ISession / ISessionController / callbacks. */
    static final class Inert implements InvocationHandler {
        private final String label;
        private final ClassLoader cl;
        Inert(String label, ClassLoader cl) { this.label = label; this.cl = cl; }

        // §472: once MediaSessionManager could really be built, the child died with a repeating
        // DoCall/INVOKE_INTERFACE_RANGE frame pattern -- runaway recursion, not a null deref. Handing
        // back a brand-new proxy for every interface-typed return lets a caller walk an unbounded
        // chain of them. Bound the depth, and report which interface+method is doing the walking.
        private static final ThreadLocal<int[]> DEPTH = new ThreadLocal<int[]>() {
            @Override protected int[] initialValue() { return new int[1]; }
        };
        private static final int MAX_DEPTH = 24;
        private static final java.util.Set<String> SEEN =
                java.util.Collections.synchronizedSet(new java.util.HashSet<String>());

        @Override public Object invoke(Object proxy, Method method, Object[] args) {
            final String n = method.getName();
            // Log each distinct interface.method ONCE -- a call-count cap would saturate on chatter
            // and hide the one that actually recurses.
            String key = method.getDeclaringClass().getName() + "." + n;
            if (SEEN.add(key)) {
                System.err.println("[WESTLAKE-472] inert " + label + " -> " + key);
                System.err.flush();
            }
            if ("asBinder".equals(n)) return proxy;
            if ("toString".equals(n)) return label;
            if ("hashCode".equals(n)) return Integer.valueOf(System.identityHashCode(proxy));
            if ("equals".equals(n))   return Boolean.valueOf(args != null && args.length == 1
                                                             && args[0] == proxy);
            Class<?> rt = method.getReturnType();
            if (rt == void.class)    return null;
            if (rt == boolean.class) return Boolean.FALSE;
            if (rt == int.class)     return Integer.valueOf(0);
            if (rt == long.class)    return Long.valueOf(0L);
            if (rt == float.class)   return Float.valueOf(0f);
            if (rt == double.class)  return Double.valueOf(0d);
            // Never hand back null for an interface: callers dereference these immediately.
            // ...but stop if we are already deep in a chain of inert proxies, or a caller that walks
            // interface-typed getters will recurse until the stack dies (§472).
            if (rt.isInterface()) {
                int[] d = DEPTH.get();
                if (d[0] >= MAX_DEPTH) {
                    System.err.println("[WESTLAKE-472] depth cap at " + key + " -> null");
                    System.err.flush();
                    return null;
                }
                d[0]++;
                try {
                    Object o = make(cl, rt.getName(), "Wl" + rt.getSimpleName());
                    if (o != null) return o;
                } finally { d[0]--; }
            }
            return null;
        }
    }

    /** Add a SystemServiceRegistry fetcher for MEDIA_SESSION_SERVICE. */
    @SuppressWarnings("unchecked")
    static String registerFetcher(ClassLoader cl) {
        try {
            Class<?> ssr = Class.forName("android.app.SystemServiceRegistry", false, cl);
            java.lang.reflect.Field f = null;
            for (java.lang.reflect.Field cand : ssr.getDeclaredFields()) {
                if (cand.getName().equals("SYSTEM_SERVICE_FETCHERS")) { f = cand; break; }
            }
            if (f == null) return "no SYSTEM_SERVICE_FETCHERS";
            f.setAccessible(true);
            Object m = f.get(null);
            if (!(m instanceof java.util.Map)) return "fetchers not a Map";
            java.util.Map<String, Object> map = (java.util.Map<String, Object>) m;
            if (map.get("media_session") != null) return "already registered";

            Class<?> fetcherIface = null;
            for (Class<?> c : ssr.getDeclaredClasses()) {
                if (c.getSimpleName().equals("ServiceFetcher")) { fetcherIface = c; break; }
            }
            if (fetcherIface == null) return "no ServiceFetcher interface";
            Object fetcher = Proxy.newProxyInstance(cl, new Class<?>[] { fetcherIface },
                    new FetcherHandler(cl));
            map.put("media_session", fetcher);
            return "registered (" + map.size() + " fetchers)";
        } catch (Throwable t) {
            return "FAILED " + t;
        }
    }

    /** Builds a MediaSessionManager on demand; its ctor uses the sCache binder seeded above. */
    static final class FetcherHandler implements InvocationHandler {
        private final ClassLoader cl;
        private Object cached;
        FetcherHandler(ClassLoader cl) { this.cl = cl; }
        @Override public synchronized Object invoke(Object proxy, Method method, Object[] args) {
            final String n = method.getName();
            if ("toString".equals(n)) return "WlMediaSessionFetcher";
            if ("hashCode".equals(n)) return Integer.valueOf(System.identityHashCode(proxy));
            if ("equals".equals(n))   return Boolean.valueOf(args != null && args.length == 1
                                                             && args[0] == proxy);
            if (cached != null) return cached;
            try {
                Object ctx = (args != null && args.length > 0) ? args[0] : null;
                Class<?> msm = Class.forName("android.media.session.MediaSessionManager", false, cl);
                Class<?> ctxCls = Class.forName("android.content.Context", false, cl);
                java.lang.reflect.Constructor<?> c = msm.getDeclaredConstructor(ctxCls);
                c.setAccessible(true);
                cached = c.newInstance(ctx);
                System.err.println("[WESTLAKE-469] built MediaSessionManager: " + cached);
                System.err.flush();
                return cached;
            } catch (Throwable t) {
                Throwable e = (t instanceof java.lang.reflect.InvocationTargetException && t.getCause() != null)
                        ? t.getCause() : t;
                System.err.println("[WESTLAKE-469] MediaSessionManager build failed: " + e);
                System.err.flush();
                return null;
            }
        }
    }

    static Object make(ClassLoader cl, String ifaceName, String label) {
        try {
            Class<?> c = Class.forName(ifaceName, false, cl);
            Class<?> ib = Class.forName("android.os.IBinder", false, cl);
            return Proxy.newProxyInstance(cl, new Class<?>[] { c, ib }, new Inert(label, cl));
        } catch (Throwable t) {
            return null;
        }
    }

    /**
     * §473: prefer a REAL generated class over a dynamic Proxy.
     *
     * §436 means any java.lang.reflect.Proxy blows up the first time framework code invokes an
     * interface method on it -- here MediaSession.<init> -> MediaSessionManager.createSession ->
     * ISessionManager.createSession threw "NoSuchMethodError: No InvokeType(4) method createSession"
     * and then took the stack out. tools/MakeIfaceImpl.java emits westlake.impl.*Impl, ordinary
     * classes implementing the same interfaces, which dispatch through the normal path.
     * Falls back to the Proxy if the generated classes are not on the BCP, so this stays working on
     * a deployment that has not had the impl dex merged in yet.
     */
    static Object realOrProxy(ClassLoader cl, String iface, String label) {
        String simple = iface.substring(iface.lastIndexOf('.') + 1);
        try {
            Class<?> impl = Class.forName("westlake.impl." + simple + "Impl", true, cl);
            Object o = impl.getDeclaredConstructor().newInstance();
            System.err.println("[WESTLAKE-473] using generated " + impl.getName() + " (not a Proxy)");
            System.err.flush();
            return o;
        } catch (Throwable t) {
            System.err.println("[WESTLAKE-473] no generated impl for " + iface + " (" + t
                    + ") -- falling back to Proxy, expect §436 on first interface call");
            System.err.flush();
            return make(cl, iface, label);
        }
    }

    public static String install() {
        try {
            ClassLoader cl = WlMediaSession.class.getClassLoader();
            Class<?> ism = Class.forName("android.media.session.ISessionManager", false, cl);
            Object mgr = realOrProxy(cl, "android.media.session.ISessionManager", "WlSessionManager");
            if (mgr == null) return "could not build ISessionManager impl";

            LocalBinder binder = new LocalBinder();
            binder.setIface((IInterface) mgr);

            Class<?> sm = Class.forName("android.os.ServiceManager", false, cl);
            java.lang.reflect.Field f = sm.getDeclaredField("sCache");
            f.setAccessible(true);
            Object cache = f.get(null);
            if (!(cache instanceof java.util.Map)) return "sCache is not a Map";
            @SuppressWarnings("unchecked")
            java.util.Map<String, IBinder> map = (java.util.Map<String, IBinder>) cache;
            map.put("media_session", binder);

            IInterface back = binder.queryLocalInterface("android.media.session.ISessionManager");
            boolean ok = (back != null) && ism.isInstance(back);

            // §469 (a2hlab gap 2): seeding sCache is NOT enough — getSystemService(MEDIA_SESSION_SERVICE)
            // still returned null, so MediaSession.<init> NPE'd on a null MediaSessionManager.
            // The gap is one layer up: SystemServiceRegistry has no fetcher registered for it. Add
            // one that builds a real MediaSessionManager (its ctor resolves ISessionManager through
            // the binder we just seeded, so it now succeeds).
            String reg = registerFetcher(cl);
            return "installed media_session, queryLocalInterface=" + (ok ? "OK" : "WRONG")
                    + "; fetcher: " + reg;
        } catch (Throwable t) {
            return "FAILED: " + t.getClass().getName() + ": " + t.getMessage();
        }
    }
}
