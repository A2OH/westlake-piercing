// android.net.ConnectivityManager — mainline stub.
//
// WESTLAKE §418 (2026-07-27): this was an EMPTY shell, so noice's connectivity pre-check
// (`connectivityManager.getActiveNetwork()`) threw
//   NoSuchMethodError: No InvokeType(2) method getActiveNetwork()Landroid/net/Network;
// on a kotlinx-coroutines thread.  noice reports that as "Oops! We couldn't fetch the sound
// library. The network is unreachable." and NEVER OPENS A SOCKET — verified: 0 entries for the
// app uid in /proc/net/tcp, while a probe running as that same uid resolves api.trynoice.com and
// connects to it fine.  So the block was purely this missing API surface, not the network.
//
// The board has one always-on WiFi link and no concept of network selection here, so report a
// single validated, unmetered WiFi network and drive callbacks as "available" immediately.
package android.net;

import java.util.ArrayList;
import java.util.List;

public class ConnectivityManager {
    public static final int TYPE_MOBILE = 0;
    public static final int TYPE_WIFI = 1;
    public static final int TYPE_ETHERNET = 9;

    /** The single network this device exposes. */
    private static final Network DEFAULT_NETWORK = new Network(100);

    private final List<NetworkCallback> mCallbacks = new ArrayList<>();

    private static void log(String m) {
        System.err.println("[WESTLAKE-418] ConnectivityManager." + m);
        System.err.flush();
    }

    public ConnectivityManager() { log("<init>"); }

    public Network getActiveNetwork() { log("getActiveNetwork -> " + DEFAULT_NETWORK); return DEFAULT_NETWORK; }

    public Network[] getAllNetworks() { return new Network[] { DEFAULT_NETWORK }; }

    public NetworkInfo getActiveNetworkInfo() { log("getActiveNetworkInfo -> connected"); return new NetworkInfo(TYPE_WIFI, true); }

    public NetworkInfo getNetworkInfo(Network network) { return getActiveNetworkInfo(); }

    public NetworkCapabilities getNetworkCapabilities(Network network) {
        log("getNetworkCapabilities(" + network + ")");
        return new NetworkCapabilities();
    }

    public LinkProperties getLinkProperties(Network network) { return new LinkProperties(); }

    public boolean isDefaultNetworkActive() { return true; }

    public boolean isActiveNetworkMetered() { return false; }

    public int getRestrictBackgroundStatus() { return 1; /* RESTRICT_BACKGROUND_STATUS_DISABLED */ }

    // --- callbacks: report "available" straight away so cold flows emit online ---
    private void notifyAvailable(final NetworkCallback cb) {
        if (cb == null) return;
        // The real framework posts these from its own thread AFTER registration returns; doing it
        // synchronously inside register*() can be dropped by a collector that is not attached yet.
        // A NAMED nested class, not an anonymous one: d8 rejects javac-21 `-source 8` anonymous
        // inner classes ("NullPointerException: Cannot invoke String.length()").
        new Thread(new NotifyTask(this, cb), "wl-net-cb").start();
    }

    private static class NotifyTask implements Runnable {
        private final ConnectivityManager mCm;
        private final NetworkCallback mCb;
        NotifyTask(ConnectivityManager cm, NetworkCallback cb) { mCm = cm; mCb = cb; }
        public void run() { mCm.deliver(mCb); }
    }

    private void deliver(NetworkCallback cb) {
        try {
            Thread.sleep(50);
        } catch (InterruptedException ignored) { }
        try {
            log("delivering onAvailable/onCapabilitiesChanged to " + cb.getClass().getName());
            cb.onAvailable(DEFAULT_NETWORK);
            cb.onCapabilitiesChanged(DEFAULT_NETWORK, new NetworkCapabilities());
            cb.onLinkPropertiesChanged(DEFAULT_NETWORK, new LinkProperties());
            cb.onBlockedStatusChanged(DEFAULT_NETWORK, false);
        } catch (Throwable t) {
            // a listener that throws must not take the process down, but say so
            log("callback THREW: " + t);
        }
    }

    public void registerDefaultNetworkCallback(NetworkCallback cb) {
        log("registerDefaultNetworkCallback");
        synchronized (mCallbacks) { mCallbacks.add(cb); }
        notifyAvailable(cb);
    }

    public void registerDefaultNetworkCallback(NetworkCallback cb, android.os.Handler h) {
        registerDefaultNetworkCallback(cb);
    }

    public void registerNetworkCallback(NetworkRequest req, NetworkCallback cb) {
        log("registerNetworkCallback");
        synchronized (mCallbacks) { mCallbacks.add(cb); }
        notifyAvailable(cb);
    }

    public void registerNetworkCallback(NetworkRequest req, NetworkCallback cb, android.os.Handler h) {
        registerNetworkCallback(req, cb);
    }

    public void requestNetwork(NetworkRequest req, NetworkCallback cb) {
        registerNetworkCallback(req, cb);
    }

    public void unregisterNetworkCallback(NetworkCallback cb) {
        synchronized (mCallbacks) { mCallbacks.remove(cb); }
    }

    public boolean bindProcessToNetwork(Network network) { return true; }

    public Network getBoundNetworkForProcess() { return null; }

    public static class NetworkCallback {
        public NetworkCallback() {}
        public void onAvailable(Network network) {}
        public void onLost(Network network) {}
        public void onUnavailable() {}
        public void onLosing(Network network, int maxMsToLive) {}
        public void onCapabilitiesChanged(Network network, NetworkCapabilities caps) {}
        public void onLinkPropertiesChanged(Network network, LinkProperties props) {}
        public void onBlockedStatusChanged(Network network, boolean blocked) {}
    }
}
