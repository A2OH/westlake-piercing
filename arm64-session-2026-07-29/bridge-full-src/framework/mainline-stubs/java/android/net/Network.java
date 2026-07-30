// android.net.Network — mainline stub (WESTLAKE §418: given an id + the URL/socket helpers that
// callers expect, so binding a request to "the network" degrades to plain default networking).
package android.net;

import java.io.IOException;
import java.net.InetAddress;
import java.net.URL;
import java.net.URLConnection;
import javax.net.SocketFactory;

public class Network {
    private final int mNetId;
    public Network() { this(100); }
    public Network(int netId) { mNetId = netId; }
    public int getNetId() { return mNetId; }
    public long getNetworkHandle() { return ((long) mNetId) << 32; }
    public SocketFactory getSocketFactory() { return SocketFactory.getDefault(); }
    public URLConnection openConnection(URL url) throws IOException { return url.openConnection(); }
    public InetAddress[] getAllByName(String host) throws java.net.UnknownHostException {
        return InetAddress.getAllByName(host);
    }
    public InetAddress getByName(String host) throws java.net.UnknownHostException {
        return InetAddress.getByName(host);
    }
    public void bindSocket(java.net.Socket socket) {}
    public void bindSocket(java.net.DatagramSocket socket) {}
    @Override public boolean equals(Object o) { return (o instanceof Network) && ((Network) o).mNetId == mNetId; }
    @Override public int hashCode() { return mNetId; }
    @Override public String toString() { return Integer.toString(mNetId); }
}
