// android.net.NetworkRequest — mainline stub (WESTLAKE §418): builder must chain, not crash.
package android.net;

public class NetworkRequest {
    public NetworkRequest() {}
    public boolean canBeSatisfiedBy(NetworkCapabilities caps) { return true; }
    public boolean hasCapability(int capability) { return new NetworkCapabilities().hasCapability(capability); }
    public boolean hasTransport(int transportType) { return transportType == NetworkCapabilities.TRANSPORT_WIFI; }

    public static class Builder {
        public Builder() {}
        public Builder addCapability(int capability) { return this; }
        public Builder removeCapability(int capability) { return this; }
        public Builder addTransportType(int transportType) { return this; }
        public Builder removeTransportType(int transportType) { return this; }
        public Builder setIncludeOtherUidNetworks(boolean include) { return this; }
        public NetworkRequest build() { return new NetworkRequest(); }
    }
}
