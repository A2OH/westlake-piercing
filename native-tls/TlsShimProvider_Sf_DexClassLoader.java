package gen;
import java.net.Socket;
import java.io.IOException;
import dalvik.system.DexClassLoader;
public class SfGen {
    public Socket createSocket(Socket socket, String host, int port, boolean autoClose) throws IOException {
        try {
            DexClassLoader dcl = new DexClassLoader("/system/android/framework/tlsjni-extra.dex", "/data/local/tmp", null, Object.class.getClassLoader());
            Class<?> c = dcl.loadClass("com.android.internal.os.TlsJniSocket");
            java.lang.reflect.Constructor<?> ctor = c.getConstructor(Socket.class, String.class, Integer.TYPE);
            return (Socket) ctor.newInstance(socket, host, Integer.valueOf(port));
        } catch (Throwable t) {
            throw new IOException("TlsJni load: " + t);
        }
    }
}
