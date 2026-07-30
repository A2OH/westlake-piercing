// android.net.LinkProperties — mainline stub (WESTLAKE §418).
package android.net;

import java.util.ArrayList;
import java.util.List;
import java.net.InetAddress;

public class LinkProperties {
    public LinkProperties() {}
    public String getInterfaceName() { return "wlan0"; }
    public List<InetAddress> getDnsServers() { return new ArrayList<>(); }
    public List<LinkAddress> getLinkAddresses() { return new ArrayList<>(); }
    public List<RouteInfo> getRoutes() { return new ArrayList<>(); }
    public String getDomains() { return null; }
    public ProxyInfo getHttpProxy() { return null; }
    public int getMtu() { return 1500; }
    public boolean isPrivateDnsActive() { return false; }
    public String getPrivateDnsServerName() { return null; }
}
