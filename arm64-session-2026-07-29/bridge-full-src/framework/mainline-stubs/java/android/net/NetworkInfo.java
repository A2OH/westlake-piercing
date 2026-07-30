// android.net.NetworkInfo — mainline stub (WESTLAKE §418): the legacy "am I online?" API.
// State/DetailedState are plain classes with static instances rather than enums: they stay
// ==-comparable for callers, and d8 fails on javac-21 `-source 8` enum inner classes.
package android.net;

public class NetworkInfo {
    private final int mType;
    private final boolean mConnected;
    public NetworkInfo() { this(1 /* TYPE_WIFI */, true); }
    public NetworkInfo(int type, boolean connected) { mType = type; mConnected = connected; }

    public boolean isConnected() { return mConnected; }
    public boolean isAvailable() { return true; }
    public boolean isConnectedOrConnecting() { return mConnected; }
    public boolean isRoaming() { return false; }
    public int getType() { return mType; }
    public String getTypeName() { return mType == 1 ? "WIFI" : "MOBILE"; }
    public String getSubtypeName() { return ""; }
    public int getSubtype() { return 0; }
    public State getState() { return mConnected ? State.CONNECTED : State.DISCONNECTED; }
    public DetailedState getDetailedState() { return mConnected ? DetailedState.CONNECTED : DetailedState.DISCONNECTED; }
    public String getExtraInfo() { return null; }
    public String getReason() { return null; }
    @Override public String toString() { return "NetworkInfo[type=" + getTypeName() + ",connected=" + mConnected + "]"; }

    public static class State {
        private final String mName;
        private State(String n) { mName = n; }
        public String name() { return mName; }
        @Override public String toString() { return mName; }
        public static final State CONNECTING = new State("CONNECTING");
        public static final State CONNECTED = new State("CONNECTED");
        public static final State SUSPENDED = new State("SUSPENDED");
        public static final State DISCONNECTING = new State("DISCONNECTING");
        public static final State DISCONNECTED = new State("DISCONNECTED");
        public static final State UNKNOWN = new State("UNKNOWN");
    }

    public static class DetailedState {
        private final String mName;
        private DetailedState(String n) { mName = n; }
        public String name() { return mName; }
        @Override public String toString() { return mName; }
        public static final DetailedState IDLE = new DetailedState("IDLE");
        public static final DetailedState SCANNING = new DetailedState("SCANNING");
        public static final DetailedState CONNECTING = new DetailedState("CONNECTING");
        public static final DetailedState AUTHENTICATING = new DetailedState("AUTHENTICATING");
        public static final DetailedState OBTAINING_IPADDR = new DetailedState("OBTAINING_IPADDR");
        public static final DetailedState CONNECTED = new DetailedState("CONNECTED");
        public static final DetailedState SUSPENDED = new DetailedState("SUSPENDED");
        public static final DetailedState DISCONNECTING = new DetailedState("DISCONNECTING");
        public static final DetailedState DISCONNECTED = new DetailedState("DISCONNECTED");
        public static final DetailedState FAILED = new DetailedState("FAILED");
        public static final DetailedState BLOCKED = new DetailedState("BLOCKED");
        public static final DetailedState VERIFYING_POOR_LINK = new DetailedState("VERIFYING_POOR_LINK");
        public static final DetailedState CAPTIVE_PORTAL_CHECK = new DetailedState("CAPTIVE_PORTAL_CHECK");
    }
}
