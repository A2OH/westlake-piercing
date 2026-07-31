package westlake;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * WESTLAKE §483 — find out WHICH executor is dead.
 *
 * The play path dies at a Room suspend DAO query: it is entered, its SELECT never reaches SQLite, and
 * it never resumes (§482). Room runs suspend queries on its query executor, which by default is
 * androidx.arch.core.executor.ArchTaskExecutor's IO executor. Room *writes* do work, and OkHttp's own
 * dispatcher works, so thread pools are not broken in general — this narrows it.
 *
 * Injected into a method known to run (LocalSoundPlayer.<init>), so it needs no app cooperation.
 */
public final class WlFix {
    private WlFix() {}

    private static boolean sProbed;

    private static void log(String s) {
        try { adapter.compat.WlProbe.log("[WESTLAKE-483] " + s); } catch (Throwable ignored) { }
    }

    /** Submit one task to each executor and report whether it actually ran. */
    public static synchronized void probeExecutors() {
        if (sProbed) return;
        sProbed = true;
        try {
            log("thread=" + Thread.currentThread().getName());

            // 1. a plain pool — the control
            ExecutorService pool = Executors.newSingleThreadExecutor();
            final CountDownLatch l1 = new CountDownLatch(1);
            pool.execute(new Runnable() { @Override public void run() { l1.countDown(); } });
            log("newSingleThreadExecutor ran=" + l1.await(3, TimeUnit.SECONDS));

            // 2. a fixed pool, the shape Room/ArchTaskExecutor uses
            ExecutorService fixed = Executors.newFixedThreadPool(4);
            final CountDownLatch l2 = new CountDownLatch(1);
            fixed.execute(new Runnable() { @Override public void run() { l2.countDown(); } });
            log("newFixedThreadPool(4) ran=" + l2.await(3, TimeUnit.SECONDS));

            // 3. ArchTaskExecutor's IO executor — the one Room actually uses
            probeArch();
        } catch (Throwable t) {
            log("probe FAILED: " + t);
        }
    }

    private static void probeArch() {
        String[] names = {
            "androidx.arch.core.executor.ArchTaskExecutor",
            "e.a",   // possible minified names; harmless if absent
        };
        for (String n : names) {
            try {
                Class<?> c = Class.forName(n, false, WlFix.class.getClassLoader());
                java.lang.reflect.Method get = c.getMethod("getInstance");
                Object inst = get.invoke(null);
                java.lang.reflect.Method io = null;
                for (java.lang.reflect.Method m : c.getMethods()) {
                    if (m.getParameterTypes().length == 0
                            && java.util.concurrent.Executor.class.isAssignableFrom(m.getReturnType())) {
                        io = m; break;
                    }
                }
                if (io == null) { log(n + ": no Executor-returning getter"); continue; }
                java.util.concurrent.Executor ex = (java.util.concurrent.Executor) io.invoke(inst);
                final CountDownLatch l = new CountDownLatch(1);
                ex.execute(new Runnable() { @Override public void run() { l.countDown(); } });
                log(n + "." + io.getName() + "() ran=" + l.await(3, TimeUnit.SECONDS));
                return;
            } catch (ClassNotFoundException e) {
                // minified away under this name; try the next
            } catch (Throwable t) {
                log(n + " probe error: " + t);
                return;
            }
        }
        log("ArchTaskExecutor not found under any known name (R8-minified)");
    }
}
