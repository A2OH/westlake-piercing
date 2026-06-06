package com.android.internal.os;
import java.io.FileWriter;
public final class TlsJni {
    public static void lg(String m){ try { FileWriter w=new FileWriter("/data/local/tmp/tls.log",true); w.write(m+"\n"); w.close(); } catch (Throwable t){} }
    static { lg("TlsJni: static init, loading libtlsjni"); try { System.load("/system/android/lib/libtlsjni.so"); lg("TlsJni: libtlsjni loaded OK"); } catch (Throwable t) { lg("TlsJni: System.load FAILED: "+t); } }
    public static native long sslConnect(int fd, String host);
    public static native int sslRead(long ssl, byte[] b, int off, int len);
    public static native int sslWrite(long ssl, byte[] b, int off, int len);
    public static native void sslClose(long ssl);
    public static native byte[] sslPeerCertDer(long ssl);
}
