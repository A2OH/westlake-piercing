package android.net.ssl;

import javax.net.ssl.SSLSocket;

/**
 * WESTLAKE §441 — this class is missing from our BCP (it normally ships with Conscrypt), and
 * OkHttp's Android10Platform.configureTlsExtensions() calls it while setting up TLS. Its absence
 * threw NoClassDefFoundError on the coroutine thread, which noice surfaced as the generic
 * "The network is unreachable."
 *
 * Session tickets are an optimisation, so reporting "unsupported" and doing nothing is correct
 * behaviour rather than a stub that hides a failure.
 */
public final class SSLSockets {
    private SSLSockets() {}

    public static boolean isSupportedSocket(SSLSocket socket) { return false; }

    public static void setUseSessionTickets(SSLSocket socket, boolean useSessionTickets) { }
}
