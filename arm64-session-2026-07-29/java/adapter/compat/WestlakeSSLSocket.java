package adapter.compat;

import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.InetAddress;
import java.net.Socket;
import java.net.SocketAddress;
import java.security.Principal;
import java.security.cert.Certificate;
import java.security.cert.CertificateFactory;
import java.util.ArrayList;
import java.util.List;
import javax.net.ssl.HandshakeCompletedListener;
import javax.net.ssl.SSLSession;
import javax.net.ssl.SSLSessionContext;
import javax.net.ssl.SSLSocket;

/**
 * WESTLAKE §441 — a real TLS client socket.
 *
 * The previous WestlakeSSLSocketFactory handed OkHttp back the *plain* socket, so the app spoke
 * cleartext to port 443 and every request died as "EOFException: \n not found: limit=0". This wraps
 * the already-connected socket and drives OpenSSL (OHOS's own libssl_openssl.z.so) through the
 * bridge. Certificates are verified against /etc/ssl/certs/cacert.pem and the hostname is checked by
 * OpenSSL; OkHttp re-checks it from getSession().getPeerCertificates().
 */
public final class WestlakeSSLSocket extends SSLSocket {

    private static native long nativeHandshake(int fd, String host, int timeoutMs) throws IOException;
    private static native int  nativeRead(long ssl, int fd, byte[] b, int off, int len, int timeoutMs);
    private static native int  nativeWrite(long ssl, int fd, byte[] b, int off, int len, int timeoutMs);
    private static native byte[] nativePeerCert(long ssl);
    private static native String nativeInfo(long ssl, int which);
    private static native void nativeClose(long ssl);

    private final Socket under;
    private final String host;
    private final int port;
    private final boolean autoClose;

    private long ssl;
    private int fd = -1;
    private boolean handshaked;
    private boolean closed;
    private Session session;
    private In in;
    private Out out;

    public WestlakeSSLSocket(Socket under, String host, int port, boolean autoClose) {
        this.under = under;
        this.host = host;
        this.port = port;
        this.autoClose = autoClose;
    }

    /** The underlying socket's raw fd — libcore hides it behind getFileDescriptor$(). */
    private static int fdOf(Socket s) {
        try {
            Method m = Socket.class.getDeclaredMethod("getFileDescriptor$");
            m.setAccessible(true);
            Object f = m.invoke(s);
            if (f instanceof FileDescriptor) return intOf((FileDescriptor) f);
        } catch (Throwable ignored) { }
        try {
            Field implF = Socket.class.getDeclaredField("impl");
            implF.setAccessible(true);
            Object impl = implF.get(s);
            Method gfd = null;
            Class<?> c = impl.getClass();
            while (c != null && gfd == null) {
                try { gfd = c.getDeclaredMethod("getFileDescriptor"); } catch (Throwable t) { c = c.getSuperclass(); }
            }
            if (gfd != null) {
                gfd.setAccessible(true);
                Object f = gfd.invoke(impl);
                if (f instanceof FileDescriptor) return intOf((FileDescriptor) f);
            }
        } catch (Throwable ignored) { }
        return -1;
    }

    private static int intOf(FileDescriptor f) throws Exception {
        Field d = FileDescriptor.class.getDeclaredField("descriptor");
        d.setAccessible(true);
        return d.getInt(f);
    }

    @Override
    public synchronized void startHandshake() throws IOException {
        if (handshaked) return;
        fd = fdOf(under);
        if (fd < 0) throw new IOException("WestlakeTLS: could not obtain fd of underlying socket");
        int timeout = 0;
        try { timeout = under.getSoTimeout(); } catch (Throwable ignored) { }
        ssl = nativeHandshake(fd, host, timeout > 0 ? timeout : 30000);
        if (ssl == 0) throw new IOException("WestlakeTLS: handshake failed for " + host);
        handshaked = true;
    }

    private void ensureHandshake() throws IOException {
        if (!handshaked) startHandshake();
    }

    @Override public synchronized InputStream getInputStream() throws IOException {
        ensureHandshake();
        if (in == null) in = new In(this);
        return in;
    }

    @Override public synchronized OutputStream getOutputStream() throws IOException {
        ensureHandshake();
        if (out == null) out = new Out(this);
        return out;
    }

    int tlsRead(byte[] b, int off, int len) throws IOException {
        if (closed) return -1;
        int t = 0;
        try { t = under.getSoTimeout(); } catch (Throwable ignored) { }
        return nativeRead(ssl, fd, b, off, len, t);
    }

    void tlsWrite(byte[] b, int off, int len) throws IOException {
        if (closed) throw new IOException("socket closed");
        int t = 0;
        try { t = under.getSoTimeout(); } catch (Throwable ignored) { }
        if (nativeWrite(ssl, fd, b, off, len, t) < 0) throw new IOException("WestlakeTLS: write failed");
    }

    @Override public synchronized void close() throws IOException {
        if (closed) return;
        closed = true;
        if (ssl != 0) { nativeClose(ssl); ssl = 0; }
        if (autoClose) under.close();
    }

    @Override public synchronized SSLSession getSession() {
        try { ensureHandshake(); } catch (IOException e) { /* fall through to an invalid session */ }
        if (session == null) session = new Session(this);
        return session;
    }

    // ---- SSLSocket surface OkHttp touches; the rest are benign no-ops -------------------------
    private static final String[] PROTOS = { "TLSv1.2", "TLSv1.3" };
    private static final String[] SUITES = { "TLS_AES_128_GCM_SHA256", "TLS_AES_256_GCM_SHA384",
            "TLS_CHACHA20_POLY1305_SHA256", "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256",
            "TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384" };

    @Override public String[] getSupportedCipherSuites() { return SUITES.clone(); }
    @Override public String[] getEnabledCipherSuites()   { return SUITES.clone(); }
    @Override public void setEnabledCipherSuites(String[] s) { }
    @Override public String[] getSupportedProtocols()    { return PROTOS.clone(); }
    @Override public String[] getEnabledProtocols()      { return PROTOS.clone(); }
    @Override public void setEnabledProtocols(String[] p) { }
    @Override public void addHandshakeCompletedListener(HandshakeCompletedListener l) { }
    @Override public void removeHandshakeCompletedListener(HandshakeCompletedListener l) { }
    @Override public void setUseClientMode(boolean m) { }
    @Override public boolean getUseClientMode() { return true; }
    @Override public void setNeedClientAuth(boolean n) { }
    @Override public boolean getNeedClientAuth() { return false; }
    @Override public void setWantClientAuth(boolean w) { }
    @Override public boolean getWantClientAuth() { return false; }
    @Override public void setEnableSessionCreation(boolean f) { }
    // OkHttp asks for the ALPN result; the JDK default throws UnsupportedOperationException.
    // We negotiate no ALPN protocol, so "" is the honest answer and OkHttp falls back to HTTP/1.1.
    @Override public String getApplicationProtocol() { return ""; }
    @Override public String getHandshakeApplicationProtocol() { return ""; }
    @Override public boolean getEnableSessionCreation() { return true; }

    // ---- delegate the plain-Socket surface to the socket we wrap ------------------------------
    @Override public InetAddress getInetAddress() { return under.getInetAddress(); }
    @Override public InetAddress getLocalAddress() { return under.getLocalAddress(); }
    @Override public int getPort() { return under.getPort(); }
    @Override public int getLocalPort() { return under.getLocalPort(); }
    @Override public SocketAddress getRemoteSocketAddress() { return under.getRemoteSocketAddress(); }
    @Override public SocketAddress getLocalSocketAddress() { return under.getLocalSocketAddress(); }
    @Override public boolean isConnected() { return under.isConnected(); }
    @Override public boolean isBound() { return under.isBound(); }
    @Override public boolean isClosed() { return closed || under.isClosed(); }
    @Override public void setSoTimeout(int t) throws java.net.SocketException { under.setSoTimeout(t); }
    @Override public int getSoTimeout() throws java.net.SocketException { return under.getSoTimeout(); }
    @Override public void setTcpNoDelay(boolean on) throws java.net.SocketException { under.setTcpNoDelay(on); }
    @Override public boolean getTcpNoDelay() throws java.net.SocketException { return under.getTcpNoDelay(); }
    @Override public void setKeepAlive(boolean on) throws java.net.SocketException { under.setKeepAlive(on); }
    @Override public boolean getKeepAlive() throws java.net.SocketException { return under.getKeepAlive(); }
    @Override public void setSoLinger(boolean on, int l) throws java.net.SocketException { under.setSoLinger(on, l); }
    @Override public int getSoLinger() throws java.net.SocketException { return under.getSoLinger(); }
    @Override public void setSendBufferSize(int s) throws java.net.SocketException { under.setSendBufferSize(s); }
    @Override public int getSendBufferSize() throws java.net.SocketException { return under.getSendBufferSize(); }
    @Override public void setReceiveBufferSize(int s) throws java.net.SocketException { under.setReceiveBufferSize(s); }
    @Override public int getReceiveBufferSize() throws java.net.SocketException { return under.getReceiveBufferSize(); }
    @Override public void shutdownInput() throws IOException { under.shutdownInput(); }
    @Override public void shutdownOutput() throws IOException { under.shutdownOutput(); }
    @Override public boolean isInputShutdown() { return under.isInputShutdown(); }
    @Override public boolean isOutputShutdown() { return under.isOutputShutdown(); }

    // ---- streams -----------------------------------------------------------------------------
    static final class In extends InputStream {
        private final WestlakeSSLSocket s;
        In(WestlakeSSLSocket s) { this.s = s; }
        @Override public int read() throws IOException {
            byte[] one = new byte[1];
            int n = s.tlsRead(one, 0, 1);
            return (n <= 0) ? -1 : (one[0] & 0xff);
        }
        @Override public int read(byte[] b, int off, int len) throws IOException {
            return s.tlsRead(b, off, len);
        }
        @Override public void close() throws IOException { s.close(); }
    }

    static final class Out extends OutputStream {
        private final WestlakeSSLSocket s;
        Out(WestlakeSSLSocket s) { this.s = s; }
        @Override public void write(int b) throws IOException { s.tlsWrite(new byte[] { (byte) b }, 0, 1); }
        @Override public void write(byte[] b, int off, int len) throws IOException { s.tlsWrite(b, off, len); }
        @Override public void close() throws IOException { s.close(); }
    }

    // ---- session -----------------------------------------------------------------------------
    static final class Session implements SSLSession {
        private final WestlakeSSLSocket s;
        private Certificate[] peer;
        Session(WestlakeSSLSocket s) { this.s = s; }

        @Override public Certificate[] getPeerCertificates() {
            if (peer == null) {
                try {
                    byte[] der = nativePeerCert(s.ssl);
                    if (der != null) {
                        CertificateFactory cf = CertificateFactory.getInstance("X.509");
                        List<Certificate> l = new ArrayList<Certificate>();
                        l.add(cf.generateCertificate(new java.io.ByteArrayInputStream(der)));
                        peer = l.toArray(new Certificate[0]);
                    }
                } catch (Throwable ignored) { }
                if (peer == null) peer = new Certificate[0];
            }
            return peer.clone();
        }
        @Override public String getCipherSuite() {
            String c = nativeInfo(s.ssl, 1); return (c != null) ? c : "UNKNOWN";
        }
        @Override public String getProtocol() {
            String p = nativeInfo(s.ssl, 0); return (p != null) ? p : "TLSv1.2";
        }
        @Override public boolean isValid() { return s.handshaked && !s.closed; }
        @Override public String getPeerHost() { return s.host; }
        @Override public int getPeerPort() { return s.port; }
        @Override public byte[] getId() { return new byte[0]; }
        @Override public SSLSessionContext getSessionContext() { return null; }
        @Override public long getCreationTime() { return 0L; }
        @Override public long getLastAccessedTime() { return 0L; }
        @Override public void invalidate() { }
        @Override public void putValue(String n, Object v) { }
        @Override public Object getValue(String n) { return null; }
        @Override public void removeValue(String n) { }
        @Override public String[] getValueNames() { return new String[0]; }
        @Override public Certificate[] getLocalCertificates() { return null; }
        @Override public Principal getLocalPrincipal() { return null; }
        @Override public Principal getPeerPrincipal() {
            Certificate[] c = getPeerCertificates();
            if (c.length > 0 && c[0] instanceof java.security.cert.X509Certificate)
                return ((java.security.cert.X509Certificate) c[0]).getSubjectX500Principal();
            return null;
        }
        @Override public int getPacketBufferSize() { return 16709; }
        @Override public int getApplicationBufferSize() { return 16384; }
        @SuppressWarnings("deprecation")
        @Override public javax.security.cert.X509Certificate[] getPeerCertificateChain() {
            throw new UnsupportedOperationException("WestlakeTLS: use getPeerCertificates()");
        }
    }
}
