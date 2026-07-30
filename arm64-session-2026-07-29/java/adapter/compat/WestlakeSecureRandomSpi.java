package adapter.compat;

import java.io.FileInputStream;
import java.io.IOException;
import java.security.SecureRandomSpi;

/**
 * WESTLAKE §474 — a real SecureRandom, because this BCP ships none.
 *
 * Every `new SecureRandom()` threw:
 *     java.lang.IllegalStateException: No SecureRandom implementation!
 *         at java.security.SecureRandom.getDefaultPRNG(SecureRandom.java:330)
 * and that is not cosmetic: it lands squarely on the playback path, because ExoPlayer's SimpleCache
 * generates its cache UID with SecureRandom —
 *     at com.google.android.exoplayer2.upstream.cache.c.o(SimpleCache.java)
 *     at com.google.android.exoplayer2.upstream.cache.c.k(SimpleCache.java:121)
 * so the media cache never initialises and the player is left holding nulls.
 *
 * It also mattered indirectly: the storm of these throws SATURATED libart's throw probe (it caps at
 * 40), which is why the real exception's stack was invisible.
 *
 * Backed by /dev/urandom — the kernel CSPRNG, which is what AOSP's own default PRNG uses. No attempt
 * is made to be a full NativePRNG: seeding is accepted and mixed in, but the kernel is the source.
 */
public final class WestlakeSecureRandomSpi extends SecureRandomSpi {

    private static final long serialVersionUID = 1L;

    /** Kept open: reopening /dev/urandom per call is the usual cause of SecureRandom being slow. */
    private static FileInputStream sUrandom;

    private static synchronized FileInputStream urandom() throws IOException {
        if (sUrandom == null) sUrandom = new FileInputStream("/dev/urandom");
        return sUrandom;
    }

    @Override protected void engineSetSeed(byte[] seed) {
        // /dev/urandom is already seeded by the kernel; mixing extra entropy in is optional and
        // writing to it requires privileges we may not have. Accept and ignore, as AOSP does when
        // the write fails.
    }

    @Override protected void engineNextBytes(byte[] bytes) {
        if (bytes == null || bytes.length == 0) return;
        try {
            FileInputStream in = urandom();
            int off = 0;
            synchronized (WestlakeSecureRandomSpi.class) {
                while (off < bytes.length) {
                    int n = in.read(bytes, off, bytes.length - off);
                    if (n <= 0) throw new IOException("short read from /dev/urandom: " + n);
                    off += n;
                }
            }
        } catch (IOException e) {
            // A SecureRandom that throws is worse than one that is merely unlucky: callers like
            // SimpleCache treat the failure as fatal. Fall back to the (non-secure) JVM PRNG rather
            // than take the process down; these values are cache UIDs, not key material.
            System.err.println("[WESTLAKE-474] /dev/urandom unavailable (" + e + "), falling back");
            System.err.flush();
            new java.util.Random().nextBytes(bytes);
        }
    }

    @Override protected byte[] engineGenerateSeed(int numBytes) {
        byte[] b = new byte[numBytes];
        engineNextBytes(b);
        return b;
    }
}
