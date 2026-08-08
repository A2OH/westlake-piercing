package com.android.internal.os;

// §578b: LEAF version — run() is fully self-contained, ZERO method calls, ZERO recursion.
// If the JIT-compiled leaf still throws StackOverflowError, it is 100% a broken compiled
// stack-overflow check (a leaf frame uses a few words), not real stack usage.
public final class WlJitBench {
    public static long run(int iters) {
        long acc = 1;
        for (int i = 0; i < iters; i++) {
            long x = acc + i;
            x ^= (x << 13);
            x ^= (x >>> 7);
            x ^= (x << 17);
            acc = x + ((long) i * 2654435761L);
        }
        return acc;
    }
    public static long bench(int rounds, int itersPerRound) {
        long total = 0;
        for (int r = 0; r < rounds; r++) total ^= run(itersPerRound);
        return total;
    }
}
