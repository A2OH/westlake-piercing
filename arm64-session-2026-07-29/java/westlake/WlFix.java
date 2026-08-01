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

    /**
     * §484 — probe the RoomDatabase's OWN executors, which is what CoroutinesRoom.execute uses.
     * Generic pools were proven to work (§483), so the question is whether THIS database's executor
     * drains. Called with the RoomDatabase instance taken straight from the DAO, so no name lookup is
     * needed (the app is R8-minified and androidx class names are gone).
     */
    private static boolean sRoomProbed;

    public static synchronized void probeRoom(Object db) {
        if (sRoomProbed || db == null) return;
        sRoomProbed = true;
        try {
            log("db = " + db.getClass().getName());
            Class<?> c = db.getClass();
            int found = 0;
            while (c != null && c != Object.class) {
                for (java.lang.reflect.Field f : c.getDeclaredFields()) {
                    if (!java.util.concurrent.Executor.class.isAssignableFrom(f.getType())) continue;
                    f.setAccessible(true);
                    Object ex = f.get(db);
                    if (ex == null) { log("  executor field " + f.getName() + " = null"); found++; continue; }
                    final java.util.concurrent.CountDownLatch l = new java.util.concurrent.CountDownLatch(1);
                    try {
                        ((java.util.concurrent.Executor) ex).execute(new Runnable() {
                            @Override public void run() { l.countDown(); }
                        });
                    } catch (Throwable t) {
                        log("  executor " + f.getName() + " (" + ex.getClass().getName() + ") REJECTED: " + t);
                        found++; continue;
                    }
                    boolean ran = l.await(3, java.util.concurrent.TimeUnit.SECONDS);
                    log("  executor " + f.getName() + " (" + ex.getClass().getName() + ") ran=" + ran);
                    found++;
                }
                c = c.getSuperclass();
            }
            if (found == 0) log("  no Executor-typed fields found on the database");
            dumpTransactionExecutor(db);
        } catch (Throwable t) {
            log("probeRoom FAILED: " + t);
        }
    }

    /**
     * §484b — Room's TransactionExecutor serialises tasks: it keeps one "active" runnable and only
     * schedules the next when that one finishes. If a task never completes, everything queued behind
     * it waits forever — which is exactly what a dead executor with a growing queue looks like.
     * Dump its internals so we can tell "wedged on an active task" from "never started".
     */
    private static void dumpTransactionExecutor(Object db) {
        try {
            Class<?> c = db.getClass();
            while (c != null && c != Object.class) {
                for (java.lang.reflect.Field f : c.getDeclaredFields()) {
                    if (!java.util.concurrent.Executor.class.isAssignableFrom(f.getType())) continue;
                    f.setAccessible(true);
                    Object ex = f.get(db);
                    if (ex == null) continue;
                    String cn = ex.getClass().getName();
                    if (cn.startsWith("java.util.concurrent")) continue;   // the raw pool, not the wrapper
                    StringBuilder sb = new StringBuilder("  " + f.getName() + " " + cn + " {");
                    for (java.lang.reflect.Field g : ex.getClass().getDeclaredFields()) {
                        g.setAccessible(true);
                        Object v = g.get(ex);
                        String desc;
                        if (v instanceof java.util.Collection) desc = "size=" + ((java.util.Collection<?>) v).size();
                        else if (v instanceof java.util.Queue) desc = "queue=" + ((java.util.Queue<?>) v).size();
                        else desc = String.valueOf(v);
                        if (desc != null && desc.length() > 80) desc = desc.substring(0, 80) + "...";
                        sb.append(' ').append(g.getName()).append('=').append(desc);
                        // If this field holds the ACTIVE runnable, reach inside it: R8 merges lambdas
                        // into one class, so the class name says nothing — the captured fields do.
                        if (v instanceof Runnable) sb.append(describeRunnable((Runnable) v));
                    }
                    log(sb.append(" }").toString());
                }
                c = c.getSuperclass();
            }
        } catch (Throwable t) {
            log("dumpTransactionExecutor failed: " + t);
        }
    }

    /** Unwrap a merged-lambda Runnable far enough to name what is actually stuck. */
    private static String describeRunnable(Runnable r) {
        StringBuilder sb = new StringBuilder("[");
        try {
            Object cur = r;
            for (int depth = 0; depth < 3 && cur != null; depth++) {
                sb.append(depth > 0 ? " -> " : "").append(cur.getClass().getName()).append('(');
                Object next = null;
                for (java.lang.reflect.Field f : cur.getClass().getDeclaredFields()) {
                    f.setAccessible(true);
                    Object v = f.get(cur);
                    String d = (v == null) ? "null" : v.getClass().getName();
                    if (v instanceof Integer || v instanceof Boolean || v instanceof String) d = String.valueOf(v);
                    sb.append(f.getName()).append('=').append(d).append(' ');
                    // follow the wrapped command / receiver one level down
                    if (next == null && v != null && (v instanceof Runnable
                            || v.getClass().getName().startsWith("com.github"))) next = v;
                }
                sb.append(')');
                cur = next;
            }
        } catch (Throwable t) {
            sb.append("unwrap failed: ").append(t);
        }
        return sb.append(']').toString();
    }

    /**
     * §490 — OBSERVE ONLY. The earlier probeRoom() submitted a task to the transaction executor and
     * then dumped that executor's queue, so the queue length it reported was probably its OWN
     * runnable. A probe must observe before it acts. This one never executes anything.
     */
    private static boolean sObserved;

    public static synchronized void observeRoom(Object db) {
        if (sObserved || db == null) return;
        sObserved = true;
        try {
            log("observe: db=" + db.getClass().getSimpleName()
                    + " thread=" + Thread.currentThread().getName());
            Class<?> c = db.getClass();
            while (c != null && c != Object.class) {
                for (java.lang.reflect.Field f : c.getDeclaredFields()) {
                    if (!java.util.concurrent.Executor.class.isAssignableFrom(f.getType())) continue;
                    f.setAccessible(true);
                    Object ex = f.get(db);
                    if (ex == null) continue;
                    StringBuilder sb = new StringBuilder("  " + f.getName() + " " + ex.getClass().getName() + " {");
                    for (java.lang.reflect.Field g : ex.getClass().getDeclaredFields()) {
                        g.setAccessible(true);
                        Object v = g.get(ex);
                        String d;
                        if (v instanceof java.util.Collection) d = "size=" + ((java.util.Collection<?>) v).size();
                        else if (v == null) d = "null";
                        else d = v.getClass().getSimpleName() + "@" + Integer.toHexString(System.identityHashCode(v));
                        sb.append(' ').append(g.getName()).append('=').append(d);
                    }
                    log(sb.append(" }").toString());
                    for (java.lang.reflect.Field g : ex.getClass().getDeclaredFields()) {
                        g.setAccessible(true);
                        Object v = g.get(ex);
                        if (v instanceof Runnable) namesInside(v, "  active-task");
                    }
                }
                c = c.getSuperclass();
            }
        } catch (Throwable t) { log("observeRoom failed: " + t); }
    }

    /**
     * §491 — name the stuck transaction. Walk the active runnable's captured fields breadth-first,
     * reporting anything belonging to the app (com.github...), which is what identifies WHICH
     * withTransaction block is parked. R8 merges lambdas, so only the captured graph identifies it.
     */
    public static void namesInside(Object root, String label) {
        try {
            java.util.ArrayDeque<Object> q = new java.util.ArrayDeque<Object>();
            java.util.Set<Object> seen = java.util.Collections.newSetFromMap(
                    new java.util.IdentityHashMap<Object, Boolean>());
            java.util.Set<String> hits = new java.util.LinkedHashSet<String>();
            q.add(root); seen.add(root);
            int visited = 0;
            while (!q.isEmpty() && visited < 400) {
                Object cur = q.poll(); visited++;
                Class<?> k = cur.getClass();
                String n = k.getName();
                if (n.startsWith("com.github") || n.contains("Repository") || n.contains("Dao")) hits.add(n);
                if (n.startsWith("java.") || n.startsWith("kotlin.jvm.internal")) continue;
                for (java.lang.reflect.Field f : k.getDeclaredFields()) {
                    try {
                        f.setAccessible(true);
                        Object v = f.get(cur);
                        if (v == null || v.getClass().isPrimitive()) continue;
                        if (seen.add(v)) q.add(v);
                    } catch (Throwable ignored) { }
                }
            }
            log(label + " app objects reachable from the stuck task: " + hits);
        } catch (Throwable t) { log("namesInside failed: " + t); }
    }

    /**
     * §490b — the decisive question codex posed: at the moment the DAO runs, does the coroutine
     * context still carry Room's TransactionElement, and which dispatcher is selected?
     * CoroutineContext.toString() lists its elements, so one line answers both.
     */
    private static boolean sCtxProbed;
    private static boolean sCtxProbed2;

    /** Same probe, used INSIDE the transaction block, so the two can be told apart. */
    public static synchronized void probeContextInTxn(Object continuation) {
        if (sCtxProbed2) return;
        sCtxProbed2 = true;
        log("--- inside withTransaction ---");
        sCtxProbed = false;
        probeContext(continuation);
        sCtxProbed = true;
    }

    public static synchronized void probeContext(Object continuation) {
        if (sCtxProbed) return;
        sCtxProbed = true;
        try {
            log("dao thread = " + Thread.currentThread().getName());
            if (continuation == null) { log("continuation is null"); return; }
            // ★Do NOT look for "getContext": kotlin.coroutines.Continuation is minified here (it is
            // n7.c, and getContext() became c()). The RETURN TYPE survives, because it belongs to the
            // Kotlin runtime rather than the app, so match on that instead.
            java.lang.reflect.Method m = null;
            for (java.lang.reflect.Method cand : continuation.getClass().getMethods()) {
                if (cand.getParameterTypes().length == 0
                        && "kotlin.coroutines.CoroutineContext".equals(cand.getReturnType().getName())) {
                    m = cand; break;
                }
            }
            if (m == null) {
                log("no CoroutineContext getter on " + continuation.getClass().getName());
                return;
            }
            m.setAccessible(true);
            Object ctx = m.invoke(continuation);
            log("dao coroutineContext = " + ctx);
        } catch (Throwable t) { log("probeContext failed: " + t); }
    }

    /**
     * §500 — log the cause ExoPlayer wraps in an ExoPlaybackException. The app's throw probe is
     * saturated by unrelated MediaRouter noise, so playback failures are otherwise invisible; this
     * catches the reason at construction time instead of relying on the probe.
     */
    private static int sCauses;

    public static synchronized void logCause(Object cause) {
        if (sCauses++ > 8) return;
        if (cause == null) { log("ExoPlaybackException cause = null"); return; }
        StringBuilder sb = new StringBuilder("ExoPlaybackException cause = ");
        Throwable t = (cause instanceof Throwable) ? (Throwable) cause : null;
        if (t == null) { sb.append(cause.getClass().getName()); log(sb.toString()); return; }
        for (Throwable c = t; c != null && sb.length() < 1200; c = c.getCause()) {
            sb.append(c.getClass().getName());
            if (c.getMessage() != null) sb.append(": ").append(c.getMessage());
            StackTraceElement[] fr = c.getStackTrace();
            if (fr != null && fr.length > 0) {
                for (int i = 0; i < fr.length && i < 6; i++) sb.append("\n\tat ").append(fr[i]);
            }
            if (c.getCause() == c) break;
            if (c.getCause() != null) sb.append("\n  caused by ");
        }
        log(sb.toString());
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
