package adapter.compat;

import java.io.InputStream;
import java.io.OutputStream;
import java.security.Key;
import java.security.KeyStoreSpi;
import java.security.cert.Certificate;
import java.util.Collections;
import java.util.Date;
import java.util.Enumeration;

/**
 * WESTLAKE §445 — an empty in-memory "JKS" KeyStore.
 *
 * This BCP has no KeyStore provider at all (Conscrypt is absent), so OkHttp's
 * Platform.platformTrustManager() died with "KeyStoreException: jks not found" while building its
 * default X509TrustManager. TrustManagerFactory.init(null) only needs a loadable, empty store.
 */
public final class WestlakeKeyStoreSpi extends KeyStoreSpi {
    @Override public Key engineGetKey(String alias, char[] password) { return null; }
    @Override public Certificate[] engineGetCertificateChain(String alias) { return null; }
    @Override public Certificate engineGetCertificate(String alias) { return null; }
    @Override public Date engineGetCreationDate(String alias) { return null; }
    @Override public void engineSetKeyEntry(String a, Key k, char[] p, Certificate[] c) { }
    @Override public void engineSetKeyEntry(String a, byte[] k, Certificate[] c) { }
    @Override public void engineSetCertificateEntry(String a, Certificate c) { }
    @Override public void engineDeleteEntry(String alias) { }
    @Override public Enumeration<String> engineAliases() {
        return Collections.enumeration(Collections.<String>emptyList());
    }
    @Override public boolean engineContainsAlias(String alias) { return false; }
    @Override public int engineSize() { return 0; }
    @Override public boolean engineIsKeyEntry(String alias) { return false; }
    @Override public boolean engineIsCertificateEntry(String alias) { return false; }
    @Override public String engineGetCertificateAlias(Certificate cert) { return null; }
    @Override public void engineStore(OutputStream stream, char[] password) { }
    @Override public void engineLoad(InputStream stream, char[] password) { }
}
