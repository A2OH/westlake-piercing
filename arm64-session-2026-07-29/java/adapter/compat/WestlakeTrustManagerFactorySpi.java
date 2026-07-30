package adapter.compat;

import java.security.KeyStore;
import java.security.cert.X509Certificate;
import javax.net.ssl.ManagerFactoryParameters;
import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactorySpi;
import javax.net.ssl.X509TrustManager;

/**
 * WESTLAKE §445 — a TrustManagerFactory so OkHttp can build its default trust manager.
 *
 * ⚠️IMPORTANT: this trust manager does NOT validate anything, and it is NOT where trust is
 * enforced. Certificate validation happens in the socket layer (§441): OpenSSL is configured with
 * SSL_VERIFY_PEER against /etc/ssl/certs/cacert.pem, the hostname is checked with SSL_set1_host,
 * and the handshake is rejected unless SSL_get_verify_result() == X509_V_OK. A connection with an
 * untrusted or mismatched certificate therefore never reaches Java at all. This class exists only
 * so OkHttp's Platform bootstrap can complete.
 */
public final class WestlakeTrustManagerFactorySpi extends TrustManagerFactorySpi {

    public static final class Tm implements X509TrustManager {
        @Override public void checkClientTrusted(X509Certificate[] chain, String authType) { }
        @Override public void checkServerTrusted(X509Certificate[] chain, String authType) { }
        @Override public X509Certificate[] getAcceptedIssuers() { return new X509Certificate[0]; }
    }

    @Override protected void engineInit(KeyStore ks) { }
    @Override protected void engineInit(ManagerFactoryParameters spec) { }
    @Override protected TrustManager[] engineGetTrustManagers() {
        return new TrustManager[] { new Tm() };
    }
}
