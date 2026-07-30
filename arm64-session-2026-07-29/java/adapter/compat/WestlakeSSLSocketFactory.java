package adapter.compat;

import java.io.IOException;
import java.net.InetAddress;
import java.net.Socket;
import javax.net.ssl.SSLSocketFactory;

/** WESTLAKE §441 — now returns a real TLS socket instead of the plain one. */
public final class WestlakeSSLSocketFactory extends SSLSocketFactory {

    @Override public String[] getDefaultCipherSuites() {
        return new String[] { "TLS_AES_128_GCM_SHA256", "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256" };
    }

    @Override public String[] getSupportedCipherSuites() { return getDefaultCipherSuites(); }

    /** The path OkHttp actually uses: wrap an already-connected socket. */
    @Override
    public Socket createSocket(Socket s, String host, int port, boolean autoClose) throws IOException {
        return new WestlakeSSLSocket(s, host, port, autoClose);
    }

    @Override public Socket createSocket(String host, int port) throws IOException {
        Socket s = new Socket(host, port);
        return new WestlakeSSLSocket(s, host, port, true);
    }

    @Override
    public Socket createSocket(String host, int port, InetAddress localHost, int localPort)
            throws IOException {
        Socket s = new Socket(host, port, localHost, localPort);
        return new WestlakeSSLSocket(s, host, port, true);
    }

    @Override public Socket createSocket(InetAddress host, int port) throws IOException {
        Socket s = new Socket(host, port);
        return new WestlakeSSLSocket(s, host.getHostName(), port, true);
    }

    @Override
    public Socket createSocket(InetAddress address, int port, InetAddress localAddress, int localPort)
            throws IOException {
        Socket s = new Socket(address, port, localAddress, localPort);
        return new WestlakeSSLSocket(s, address.getHostName(), port, true);
    }
}
