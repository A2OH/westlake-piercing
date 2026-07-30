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

    /** Replaces {@code invoke-interface Lg9/f;->value()Ljava/lang/String;} (annotation proxy) */
    public static String annValue(Object annotation) throws Throwable {
        return (String) viaHandler(annotation, "value", 0, null);
    }
}
