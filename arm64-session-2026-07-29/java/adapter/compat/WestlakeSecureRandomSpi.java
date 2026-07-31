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
            // ★FAIL CLOSED. An earlier version degraded to java.util.Random here so callers would not
            // crash. That is wrong: this SPI is registered as SHA1PRNG/NativePRNG/DRBG, so anything
            // in the process asking for secure bytes -- key material included -- would silently get
            // predictable ones. A caller that cannot get entropy must be told so.
            System.err.println("[WESTLAKE-474] /dev/urandom unavailable: " + e);
            System.err.flush();
            throw new java.lang.IllegalStateException("no entropy source available", e);
        }
    }

    @Override protected byte[] engineGenerateSeed(int numBytes) {
        byte[] b = new byte[numBytes];
        engineNextBytes(b);
        return b;
    }
}
