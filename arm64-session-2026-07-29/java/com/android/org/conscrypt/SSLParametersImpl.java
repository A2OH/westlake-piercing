package com.android.org.conscrypt;

/**
 * WESTLAKE §676 (2026-08-18) — presence-only stub for com.android.org.conscrypt.SSLParametersImpl.
 *
 * Why this class exists at all: okhttp's platform detection is a pure Class.forName probe.
 *   Platform.findPlatform()  -> isAndroid() (java.vm.name == "Dalvik") -> findAndroidPlatform()
 *   findAndroidPlatform()    -> Android10Platform.buildIfSupported() ?: AndroidPlatform.buildIfSupported()
 *                               ?: throw NullPointerException("No platform found on Android")
 *   Android10Platform.buildIfSupported() -> getSdkInt() >= 29 && findClass("com.android.org.conscrypt.SSLParametersImpl")
 *   AndroidPlatform.buildIfSupported()   -> findClass(conscrypt…) or findClass(org.apache.harmony…jsse.SSLParametersImpl)
 * Verified 2026-08-18: NEITHER name exists in ANY of the ten boot-classpath jars, so both builders
 * swallow ClassNotFoundException, return null, and Platform.<clinit> dies with that NPE. Measured
 * consequence on Toutiao: requests are issued (doPost=18, doGet=25) but NOT ONE callback fires
 * (onResponse=0 AND onFailure=0) — the HTTP layer is dead, so the feed and category names stay empty.
 *
 * This port has no Conscrypt: TLS is real JSSE over libssl_openssl.z.so (§441). okhttp only needs
 * the TYPE to be resolvable — it uses it solely as a reflection target:
 *   AndroidPlatform.trustManager() does readFieldOrNull(sslSocketFactory, SSLParametersImpl.class,
 *   "sslParameters"), and readFieldOrNull returns null when the field is absent, which okhttp
 *   already handles. So a presence-only class is enough to let AndroidPlatform build, and it
 *   deliberately implements NO conscrypt behaviour — nothing must ever route TLS through here.
 *
 * ⚠️If some caller ever tries to USE this as a real SSLParametersImpl it will not work; the intent
 * is strictly "let Class.forName succeed". Delete this the day the port ships a real Conscrypt.
 */
public class SSLParametersImpl {
    private SSLParametersImpl() {
        throw new UnsupportedOperationException(
                "westlake §676: presence-only stub for okhttp platform detection; not a real "
                + "Conscrypt SSLParametersImpl. TLS goes through JSSE (§441).");
    }
}
