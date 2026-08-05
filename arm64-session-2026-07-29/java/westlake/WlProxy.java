package westlake;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

/**
 * WESTLAKE §440 — bypass for the invoke-interface-on-Proxy defect (§436).
 *
 * Our libart patch in FindMethodToCall re-validates the dispatched method with
 * ArtMethod::GetNameView()/GetSignature(), which AOSP forbids on proxy methods
 * (DCHECK(!IsProxyMethod()), compiled out by -DNDEBUG). For a Proxy receiver it therefore reads the
 * wrong dex, fabricates a signature mismatch and throws NoSuchMethodError naming the interface.
 * Only PROXY receivers are affected.
 *
 * These helpers reach the same target without ever executing an invoke-interface whose receiver is
 * a proxy: they fetch the InvocationHandler (a normal class) and call invoke() on that instead.
 * Call sites are rewritten invoke-interface -> invoke-static, same register list.
 */
public final class WlProxy {
    private WlProxy() {}

    private static Object viaHandler(Object proxy, String name, int argc, Object[] args)
            throws Throwable {
        InvocationHandler h = Proxy.getInvocationHandler(proxy);
        Method target = null;
        Class<?>[] ifaces = proxy.getClass().getInterfaces();
        for (int i = 0; i < ifaces.length && target == null; i++) {
            Method[] ms = ifaces[i].getDeclaredMethods();
            for (int k = 0; k < ms.length; k++) {
                if (ms[k].getName().equals(name) && ms[k].getParameterTypes().length == argc) {
                    target = ms[k];
                    break;
                }
            }
        }
        if (target == null) {
            throw new NoSuchMethodError("WlProxy: " + name + " not found on " + proxy.getClass());
        }
        return h.invoke(proxy, target, args);
    }

    /** Replaces {@code invoke-interface Lq6/b;->b(Ln7/c;)Ljava/lang/Object;} */
    public static Object svcB(Object service, Object continuation) throws Throwable {
        return viaHandler(service, "b", 1, new Object[] { continuation });
    }

    /** Replaces {@code invoke-interface Lq6/b;->a(Ln7/c;)Ljava/lang/Object;} */
    public static Object svcA(Object service, Object continuation) throws Throwable {
        return viaHandler(service, "a", 1, new Object[] { continuation });
    }


    // ── §524 ────────────────────────────────────────────────────────────────────────────────────
    // §440 covered only three sites. A static scan of the app dex found SIXTEEN invoke-interface
    // sites on the four Retrofit service interfaces that NoiceApiClient creates through
    // Proxy.newProxyInstance (q6/a account, q6/b cdn, q6/c internal-account, q6/d subscription).
    // Every one of the thirteen unpatched sites is a latent §436 crash, and the app was dying
    // intermittently with a SIGSEGV inside art::interpreter::DoCall from INVOKE_INTERFACE.
    //
    // ★q6/b.c is the CDN `resource(path)` call — CdnSoundDataSource.open() makes it for every audio
    // segment, so it is by far the most executed Proxy call during playback.
    //
    // Helpers are per-arity because the rewrite must preserve the register list: invoke-interface and
    // invoke-static are both format 35c, so a site with N registers needs a static taking N args.

    /** Replaces {@code invoke-interface Lq6/b;->c(Ljava/lang/String;)Le9/b;} — CDN resource(path). */
    public static Object svcC(Object service, Object path) throws Throwable {
        return viaHandler(service, "c", 1, new Object[] { path });
    }

    /** One-arg service methods (receiver + 1): q6/a.a, q6/c.a, q6/c.b. */
    public static Object svc1(Object service, String name, Object a0) throws Throwable {
        return viaHandler(service, name, 1, new Object[] { a0 });
    }

    /** Two-arg service methods (receiver + 2): q6/a.b/c/e, q6/d.a/b/e/f. */
    public static Object svc2(Object service, String name, Object a0, Object a1) throws Throwable {
        return viaHandler(service, name, 2, new Object[] { a0, a1 });
    }

    /** Replaces {@code invoke-interface Lg9/f;->value()Ljava/lang/String;} (annotation proxy) */
    public static String annValue(Object annotation) throws Throwable {
        return (String) viaHandler(annotation, "value", 0, null);
    }
}
