// android.net.NetworkCapabilities — mainline stub (WESTLAKE §418).
// Reports a validated, unmetered WiFi link: that is what the board actually has, and it is what
// an "am I online?" check needs to see.
package android.net;

public class NetworkCapabilities {
    public static final int TRANSPORT_CELLULAR = 0;
    public static final int TRANSPORT_WIFI = 1;
    public static final int TRANSPORT_BLUETOOTH = 2;
    public static final int TRANSPORT_ETHERNET = 3;
    public static final int TRANSPORT_VPN = 4;

    public static final int NET_CAPABILITY_MMS = 0;
    public static final int NET_CAPABILITY_NOT_METERED = 11;
    public static final int NET_CAPABILITY_INTERNET = 12;
    public static final int NET_CAPABILITY_NOT_RESTRICTED = 13;
    public static final int NET_CAPABILITY_TRUSTED = 14;
    public static final int NET_CAPABILITY_NOT_VPN = 15;
    public static final int NET_CAPABILITY_VALIDATED = 16;
    public static final int NET_CAPABILITY_NOT_ROAMING = 18;
    public static final int NET_CAPABILITY_FOREGROUND = 19;
    public static final int NET_CAPABILITY_NOT_CONGESTED = 20;
    public static final int NET_CAPABILITY_NOT_SUSPENDED = 21;

    public NetworkCapabilities() {}
    public NetworkCapabilities(NetworkCapabilities other) {}

    public boolean hasTransport(int transportType) { return transportType == TRANSPORT_WIFI; }

    public boolean hasCapability(int capability) {
        boolean r = hasCapabilityImpl(capability);
        System.err.println("[WESTLAKE-418] NetworkCapabilities.hasCapability(" + capability + ") -> " + r);
        System.err.flush();
        return r;
    }

    private boolean hasCapabilityImpl(int capability) {
        switch (capability) {
            case NET_CAPABILITY_INTERNET:
            case NET_CAPABILITY_VALIDATED:
            case NET_CAPABILITY_NOT_METERED:
            case NET_CAPABILITY_NOT_RESTRICTED:
            case NET_CAPABILITY_TRUSTED:
            case NET_CAPABILITY_NOT_VPN:
            case NET_CAPABILITY_NOT_ROAMING:
            case NET_CAPABILITY_NOT_CONGESTED:
            case NET_CAPABILITY_NOT_SUSPENDED:
                return true;
            default:
                return false;
        }
    }

    public int getLinkDownstreamBandwidthKbps() { return 30000; }
    public int getLinkUpstreamBandwidthKbps() { return 15000; }
    public int[] getCapabilities() {
        return new int[] { NET_CAPABILITY_INTERNET, NET_CAPABILITY_VALIDATED,
                           NET_CAPABILITY_NOT_METERED, NET_CAPABILITY_NOT_RESTRICTED };
    }
    public TransportInfo getTransportInfo() { return null; }
}
