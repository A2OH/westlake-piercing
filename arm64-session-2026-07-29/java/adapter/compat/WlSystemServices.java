package adapter.compat;

import android.os.IBinder;

/**
 * WESTLAKE §476 — seed the system services whose absence breaks playback.
 *
 * Tapping play threw, from inside SoundPlaybackService's `wakeLock` lazy:
 *     java.lang.IllegalArgumentException: Required value was null.
 * The app bytecode is unambiguous — SoundPlaybackService$wakeLock$2.e() does
 * getSystemService(PowerManager.class), and on null throws exactly that message:
 *     INVOKE_STATIC c0/a->d(Context, Class)      ; ContextCompat.getSystemService
 *     IF_EQZ  -> new IllegalArgumentException("Required value was null."); throw
 *
 * PowerManager was null because framework.jar's fetcher needs BOTH services:
 *     SystemServiceRegistry$38.createService():
 *         ServiceManager.getServiceOrThrow("power")          -> IPowerManager$Stub.asInterface
 *         ServiceManager.getServiceOrThrow("thermalservice") -> IThermalService$Stub.asInterface
 * Neither was in sCache, getServiceOrThrow threw, and the cached fetcher yielded null.
 *
 * ★The binders here are the GENERATED westlake.impl.* classes, not dynamic Proxies. A Proxy would
 * detonate on the first interface call (§436). Those classes extend Binder and override
 * queryLocalInterface() to return themselves, so Stub.asInterface() uses them directly and never
 * transacts — the §454 recipe, minus the Proxy hazard.
 */
public final class WlSystemServices {

    private WlSystemServices() {}

    /** service name -> generated impl class implementing its AIDL interface */
    private static final String[][] SERVICES = {
        { "power",          "westlake.impl.IPowerManagerImpl"  },
        { "thermalservice", "westlake.impl.IThermalServiceImpl" },
    };

    @SuppressWarnings("unchecked")
    public static String install() {
        StringBuilder out = new StringBuilder();
        try {
            ClassLoader cl = WlSystemServices.class.getClassLoader();
            Class<?> sm = Class.forName("android.os.ServiceManager", false, cl);
            java.lang.reflect.Field f = sm.getDeclaredField("sCache");
            f.setAccessible(true);
            Object cache = f.get(null);
            if (!(cache instanceof java.util.Map)) return "sCache is not a Map";
            java.util.Map<String, IBinder> map = (java.util.Map<String, IBinder>) cache;

            for (String[] entry : SERVICES) {
                final String name = entry[0], implName = entry[1];
                IBinder existing = map.get(name);
                if (existing != null) {
                    // Say WHAT is already there. "already" alone is not evidence the service works:
                    // a bare Binder satisfies the map but its queryLocalInterface() returns null, so
                    // Stub.asInterface() falls back to a transacting proxy and the manager ends up
                    // null anyway (that is exactly how PowerManager was failing).
                    Object li = null;
                    try { li = existing.queryLocalInterface("x"); } catch (Throwable ignored) { }
                    out.append(name).append("=already(").append(existing.getClass().getName())
                       .append(", queryLocalInterface->").append(li == null ? "null" : li.getClass().getName())
                       .append(") ");
                    continue;
                }
                try {
                    Class<?> impl = Class.forName(implName, true, cl);
                    Object o = impl.getDeclaredConstructor().newInstance();
                    if (!(o instanceof IBinder)) { out.append(name).append("=NOT_BINDER "); continue; }
                    map.put(name, (IBinder) o);
                    out.append(name).append("=ok ");
                } catch (Throwable t) {
                    // Say which one and why: a silent miss here reappears much later as an unrelated
                    // null from getSystemService.
                    out.append(name).append("=FAILED(").append(t).append(") ");
                }
            }
            System.err.println("[WESTLAKE-476] seeded services: " + out);
            System.err.flush();
            return out.toString();
        } catch (Throwable t) {
            String s = "FAILED: " + t;
            System.err.println("[WESTLAKE-476] " + s);
            System.err.flush();
            return s;
        }
    }
}
