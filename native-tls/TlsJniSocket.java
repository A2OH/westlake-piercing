package com.android.internal.os;
import java.io.*;
import java.net.*;
import java.security.cert.Certificate;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.security.Principal;
import javax.net.ssl.*;

/** SSLSocket backed by native OpenSSL (TlsJni) over an already-connected plain socket. */
public final class TlsJniSocket extends SSLSocket {
    final Socket under;
    final String host;
    long ssl;
    boolean shook;
    X509Certificate[] peer;
    private InputStream in;
    private OutputStream out;
    private final TlsSession session = new TlsSession(this);

    public TlsJniSocket(Socket under, String host, int port) { this.under = under; this.host = host; TlsJni.lg("TlsJniSocket: ctor host="+host); }

    int fd() throws IOException {
        try {
            java.lang.reflect.Field implF = Socket.class.getDeclaredField("impl"); implF.setAccessible(true);
            Object impl = implF.get(under);
            java.io.FileDescriptor fdesc = null;
            try { java.lang.reflect.Field fdF = SocketImpl.class.getDeclaredField("fd"); fdF.setAccessible(true); fdesc = (java.io.FileDescriptor) fdF.get(impl); } catch (Throwable t) {}
            if (fdesc == null) { java.lang.reflect.Method m = impl.getClass().getMethod("getFileDescriptor"); m.setAccessible(true); fdesc = (java.io.FileDescriptor) m.invoke(impl); }
            try { java.lang.reflect.Field d = java.io.FileDescriptor.class.getDeclaredField("descriptor"); d.setAccessible(true); return d.getInt(fdesc); }
            catch (Throwable t) { java.lang.reflect.Method gi = java.io.FileDescriptor.class.getMethod("getInt$"); return ((Integer) gi.invoke(fdesc)).intValue(); }
        } catch (Throwable t) { throw new IOException("TlsJniSocket: cannot get fd: " + t); }
    }

    public void startHandshake() throws IOException {
        if (shook) return;
        TlsJni.lg("TlsJniSocket: startHandshake begin");
        int f = fd();
        TlsJni.lg("TlsJniSocket: fd="+f);
        ssl = TlsJni.sslConnect(f, host);
        TlsJni.lg("TlsJniSocket: sslConnect ret="+ssl);
        if (ssl == 0) throw new SSLHandshakeException("TlsJni: SSL_connect failed for " + host);
        try {
            byte[] der = TlsJni.sslPeerCertDer(ssl);
            if (der != null) {
                CertificateFactory cf = CertificateFactory.getInstance("X.509");
                X509Certificate xc = (X509Certificate) cf.generateCertificate(new ByteArrayInputStream(der));
                peer = new X509Certificate[]{ xc };
            }
        } catch (Throwable t) {}
        shook = true;
    }

    public InputStream getInputStream() throws IOException { if (!shook) startHandshake(); if (in == null) in = new TlsIn(this); return in; }
    public OutputStream getOutputStream() throws IOException { if (!shook) startHandshake(); if (out == null) out = new TlsOut(this); return out; }
    public synchronized void close() throws IOException { try { if (ssl != 0) TlsJni.sslClose(ssl); } catch (Throwable t) {} ssl = 0; try { under.close(); } catch (Throwable t) {} }
    public SSLSession getSession() { try { if (!shook) startHandshake(); } catch (IOException e) {} return session; }

    public String[] getSupportedCipherSuites() { return new String[]{ "TLS_AES_128_GCM_SHA256" }; }
    public String[] getEnabledCipherSuites() { return getSupportedCipherSuites(); }
    public void setEnabledCipherSuites(String[] s) {}
    public String[] getSupportedProtocols() { return new String[]{ "TLSv1.2", "TLSv1.3" }; }
    public String[] getEnabledProtocols() { return getSupportedProtocols(); }
    public void setEnabledProtocols(String[] p) {}
    public void addHandshakeCompletedListener(HandshakeCompletedListener l) {}
    public void removeHandshakeCompletedListener(HandshakeCompletedListener l) {}
    public void setUseClientMode(boolean b) {}
    public boolean getUseClientMode() { return true; }
    public void setNeedClientAuth(boolean b) {}
    public boolean getNeedClientAuth() { return false; }
    public void setWantClientAuth(boolean b) {}
    public boolean getWantClientAuth() { return false; }
    public void setEnableSessionCreation(boolean b) {}
    public boolean getEnableSessionCreation() { return true; }

    public InetAddress getInetAddress() { return under.getInetAddress(); }
    public int getPort() { return under.getPort(); }
    public int getLocalPort() { return under.getLocalPort(); }
    public SocketAddress getRemoteSocketAddress() { return under.getRemoteSocketAddress(); }
    public SocketAddress getLocalSocketAddress() { return under.getLocalSocketAddress(); }
    public boolean isConnected() { return under.isConnected(); }
    public boolean isClosed() { return under.isClosed(); }
    public boolean isBound() { return under.isBound(); }
    public void setSoTimeout(int t) throws SocketException { under.setSoTimeout(t); }
    public int getSoTimeout() throws SocketException { return under.getSoTimeout(); }
    public void setTcpNoDelay(boolean b) throws SocketException { under.setTcpNoDelay(b); }
    public boolean getTcpNoDelay() throws SocketException { return under.getTcpNoDelay(); }
    public void setKeepAlive(boolean b) throws SocketException { under.setKeepAlive(b); }
    public void setSoLinger(boolean a, int b) throws SocketException { under.setSoLinger(a, b); }

    static final class TlsIn extends InputStream {
        final TlsJniSocket s; TlsIn(TlsJniSocket s){ this.s = s; }
        public int read() throws IOException { byte[] b = new byte[1]; int n = TlsJni.sslRead(s.ssl, b, 0, 1); return n <= 0 ? -1 : (b[0] & 0xff); }
        public int read(byte[] b, int o, int l) throws IOException { int n = TlsJni.sslRead(s.ssl, b, o, l); return n <= 0 ? -1 : n; }
    }
    static final class TlsOut extends OutputStream {
        final TlsJniSocket s; TlsOut(TlsJniSocket s){ this.s = s; }
        public void write(int b) throws IOException { int n = TlsJni.sslWrite(s.ssl, new byte[]{ (byte) b }, 0, 1); if (n <= 0) throw new IOException("ssl write"); }
        public void write(byte[] b, int o, int l) throws IOException { int w = 0; while (w < l) { int n = TlsJni.sslWrite(s.ssl, b, o + w, l - w); if (n <= 0) throw new IOException("ssl write"); w += n; } }
    }
    static final class TlsSession implements SSLSession {
        final TlsJniSocket s; TlsSession(TlsJniSocket s){ this.s = s; }
        public byte[] getId() { return new byte[0]; }
        public SSLSessionContext getSessionContext() { return null; }
        public long getCreationTime() { return System.currentTimeMillis(); }
        public long getLastAccessedTime() { return System.currentTimeMillis(); }
        public void invalidate() {}
        public boolean isValid() { return true; }
        public void putValue(String n, Object v) {}
        public Object getValue(String n) { return null; }
        public void removeValue(String n) {}
        public String[] getValueNames() { return new String[0]; }
        public Certificate[] getPeerCertificates() throws SSLPeerUnverifiedException { if (s.peer == null) throw new SSLPeerUnverifiedException("no peer cert"); return s.peer; }
        public Certificate[] getLocalCertificates() { return null; }
        public Principal getPeerPrincipal() throws SSLPeerUnverifiedException { if (s.peer == null) throw new SSLPeerUnverifiedException("no peer"); return s.peer[0].getSubjectDN(); }
        public Principal getLocalPrincipal() { return null; }
        public String getCipherSuite() { return "TLS_AES_128_GCM_SHA256"; }
        public String getProtocol() { return "TLSv1.2"; }
        public String getPeerHost() { return s.host; }
        public int getPeerPort() { return -1; }
        public int getPacketBufferSize() { return 16709; }
        public int getApplicationBufferSize() { return 16384; }
    }
}
