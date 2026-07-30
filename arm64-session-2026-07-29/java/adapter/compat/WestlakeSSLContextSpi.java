package adapter.compat;

import java.security.KeyManagementException;
import java.security.SecureRandom;
import javax.net.ssl.KeyManager;
import javax.net.ssl.SSLEngine;
import javax.net.ssl.SSLServerSocketFactory;
import javax.net.ssl.SSLSessionContext;
import javax.net.ssl.SSLSocketFactory;
import javax.net.ssl.SSLContextSpi;
import javax.net.ssl.TrustManager;

/** WESTLAKE §441 — hands out the real socket factory. */
public final class WestlakeSSLContextSpi extends SSLContextSpi {
    @Override protected void engineInit(KeyManager[] km, TrustManager[] tm, SecureRandom sr)
            throws KeyManagementException { }
    @Override protected SSLSocketFactory engineGetSocketFactory() {
        return new WestlakeSSLSocketFactory();
    }
    @Override protected SSLServerSocketFactory engineGetServerSocketFactory() { return null; }
    @Override protected SSLEngine engineCreateSSLEngine() { return null; }
    @Override protected SSLEngine engineCreateSSLEngine(String host, int port) { return null; }
    @Override protected SSLSessionContext engineGetClientSessionContext() { return null; }
    @Override protected SSLSessionContext engineGetServerSessionContext() { return null; }
}
