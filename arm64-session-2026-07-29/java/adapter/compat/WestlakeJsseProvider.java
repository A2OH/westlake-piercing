package adapter.compat;

import java.security.Provider;

/** WESTLAKE §441 — registers the TLS implementation (Conscrypt is absent from this BCP). */
public final class WestlakeJsseProvider extends Provider {
    @SuppressWarnings("deprecation")
    public WestlakeJsseProvider() {
        super("WestlakeJSSE", 1.0, "Westlake TLS over OHOS OpenSSL");
        String spi = "adapter.compat.WestlakeSSLContextSpi";
        put("SSLContext.TLS", spi);
        put("SSLContext.TLSv1", spi);
        put("SSLContext.TLSv1.1", spi);
        put("SSLContext.TLSv1.2", spi);
        put("SSLContext.TLSv1.3", spi);
        put("SSLContext.SSL", spi);
        put("SSLContext.Default", spi);
        // §445: this BCP has no KeyStore/TrustManagerFactory at all, and OkHttp's Platform
        // bootstrap needs both to build its default X509TrustManager.
        put("KeyStore.JKS", "adapter.compat.WestlakeKeyStoreSpi");
        put("KeyStore.jks", "adapter.compat.WestlakeKeyStoreSpi");
        put("KeyStore.BKS", "adapter.compat.WestlakeKeyStoreSpi");
        put("KeyStore.AndroidCAStore", "adapter.compat.WestlakeKeyStoreSpi");
        String tmf = "adapter.compat.WestlakeTrustManagerFactorySpi";
        put("TrustManagerFactory.PKIX", tmf);
        put("TrustManagerFactory.X509", tmf);
        put("TrustManagerFactory.SunX509", tmf);
        put("Alg.Alias.TrustManagerFactory.SunPKIX", "PKIX");
        // §474: this BCP has no SecureRandom either. `new SecureRandom()` threw
        // "No SecureRandom implementation!", which broke ExoPlayer's SimpleCache (it generates its
        // cache UID with one) and so broke playback.
        String srng = "adapter.compat.WestlakeSecureRandomSpi";
        put("SecureRandom.SHA1PRNG", srng);
        put("SecureRandom.NativePRNG", srng);
        put("SecureRandom.DRBG", srng);
        put("Alg.Alias.SecureRandom.NativePRNGNonBlocking", "NativePRNG");
    }
}
