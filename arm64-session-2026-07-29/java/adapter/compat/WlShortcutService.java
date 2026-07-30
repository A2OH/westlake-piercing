package adapter.compat;

import android.os.Binder;
import android.os.IBinder;
import android.os.IInterface;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * WESTLAKE §454 — a working "shortcut" system service.
 *
 * §416 seeded ServiceManager.sCache with a bare {@code new Binder()}. That is enough for
 * {@code IShortcutService.Stub.asInterface()} to succeed, but every call then goes through
 * {@code transact()} on a Binder that implements nothing, so the reply parcel is empty and the
 * generated Proxy hands back a null {@code ParceledListSlice}. noice's PresetsFragment paid for it:
 * <pre>
 *   NPE at android.content.pm.ShortcutManager.getDynamicShortcuts(ShortcutManager.java:171)
 *      &lt;- ShortcutManagerCompat &lt;- PresetsViewModel &lt;- PresetsFragment.onViewCreated:55
 * </pre>
 * onViewCreated died before it ever queried the preset table, so the page showed "No Presets"
 * even though the 4 default presets were sitting in the database.
 *
 * Fix: {@code Stub.asInterface()} consults {@code queryLocalInterface()} FIRST and, when that
 * returns something implementing the interface, uses it directly and never calls transact(). So we
 * register a Binder whose queryLocalInterface returns a dynamic Proxy for IShortcutService that
 * answers with empty results instead of null.
 */
public final class WlShortcutService implements InvocationHandler {

    /** Binder whose queryLocalInterface hands back our in-process implementation. */
    public static final class LocalBinder extends Binder {
        private IInterface iface;
        void setIface(IInterface i) { this.iface = i; }
        @Override public IInterface queryLocalInterface(String descriptor) { return iface; }
    }

    private IBinder self;
    private void setSelf(IBinder b) { this.self = b; }

    @Override
    public Object invoke(Object proxy, Method method, Object[] args) throws Throwable {
        final String name = method.getName();
        if (name.equals("asBinder")) return self;
        if (name.equals("toString")) return "WlShortcutService";
        if (name.equals("hashCode")) return Integer.valueOf(System.identityHashCode(proxy));
        if (name.equals("equals")) return Boolean.valueOf(args != null && args.length > 0 && proxy == args[0]);
        return defaultFor(method.getReturnType());
    }

    /** Empty-but-non-null answers: null is what broke ShortcutManager in the first place. */
    private static Object defaultFor(Class<?> r) {
        if (r == void.class || r == Void.class) return null;
        if (r == boolean.class) return Boolean.FALSE;
        if (r == int.class) return Integer.valueOf(0);
        if (r == long.class) return Long.valueOf(0L);
        if (r == float.class) return Float.valueOf(0f);
        if (r == double.class) return Double.valueOf(0d);
        if (r == byte.class) return Byte.valueOf((byte) 0);
        if (r == short.class) return Short.valueOf((short) 0);
        if (r == char.class) return Character.valueOf('\0');
        if (r.getName().equals("android.content.pm.ParceledListSlice")) return emptySlice();
        if (List.class.isAssignableFrom(r)) return new ArrayList<Object>();
        return null;
    }

    /** {@code new ParceledListSlice(emptyList())} — the class is @hide, so build it reflectively. */
    private static Object emptySlice() {
        try {
            Class<?> c = Class.forName("android.content.pm.ParceledListSlice");
            Constructor<?> ctor = c.getConstructor(List.class);
            ctor.setAccessible(true);
            return ctor.newInstance(new ArrayList<Object>());
        } catch (Throwable t) {
            try {   // some builds expose ParceledListSlice.emptyList()
                Class<?> c = Class.forName("android.content.pm.ParceledListSlice");
                Method m = c.getMethod("emptyList");
                return m.invoke(null);
            } catch (Throwable t2) {
                return null;
            }
        }
    }

    /** Returns a short status string for the bridge to log. */
    public static String install() {
        try {
            ClassLoader cl = WlShortcutService.class.getClassLoader();
            Class<?> iface = Class.forName("android.content.pm.IShortcutService", false, cl);

            WlShortcutService handler = new WlShortcutService();
            Object proxy = Proxy.newProxyInstance(cl, new Class<?>[] { iface }, handler);

            LocalBinder binder = new LocalBinder();
            binder.setIface((IInterface) proxy);
            handler.setSelf(binder);

            // Replace whatever §416 put in ServiceManager.sCache for "shortcut".
            Class<?> sm = Class.forName("android.os.ServiceManager", false, cl);
            java.lang.reflect.Field f = sm.getDeclaredField("sCache");
            f.setAccessible(true);
            Object cache = f.get(null);
            if (!(cache instanceof java.util.Map)) return "sCache is not a Map";
            @SuppressWarnings("unchecked")
            java.util.Map<String, IBinder> map = (java.util.Map<String, IBinder>) cache;
            map.put("shortcut", binder);

            // Sanity-check the path ShortcutManager actually takes.
            IInterface back = binder.queryLocalInterface("android.content.pm.IShortcutService");
            boolean ok = (back != null) && iface.isInstance(back);
            return "installed, queryLocalInterface=" + (ok ? "OK" : "WRONG");
        } catch (Throwable t) {
            return "FAILED: " + t.getClass().getName() + ": " + t.getMessage();
        }
    }

    private WlShortcutService() { }
}
