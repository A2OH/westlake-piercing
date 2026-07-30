package adapter.compat;

/**
 * WESTLAKE §442 — is an interface call on a d8-DESUGARED LAMBDA inside a BCP dex wrong?
 *
 * java.util.regex matches literals but every CharProperty node (which holds a CharPredicate lambda)
 * always returns false. App-dex lambdas demonstrably work (the whole UI runs on them), so the
 * suspicion is specifically BCP-dex desugared lambdas. This class lives in oh-adapter-framework.jar
 * (a BCP jar) and compares four call shapes against the SAME logic.
 */
public final class WlProbe {
    private WlProbe() {}

    public interface P { boolean test(int c); }

    /** §450: native sink — Android routes System.err to the log framework, not to our fd 2. */
    private static native void nativeLog(String s);

    /** Public entry point for injected app-dex tracing (westlake.WlTrace). */
    public static void log(String s) {
        try { nativeLog(s); } catch (Throwable ignored) { }
    }

    /**
     * Called from injected bytecode in noice's catch handlers so the real Throwable is visible.
     *
     * ★§477: this used to delegate to t.printStackTrace(PrintWriter), which produces a HEADER AND NO
     * FRAMES here -- every printStackTrace overload in this runtime is stubbed out
     * ("[RT] Throwable.printStackTrace (fork-safe noop)"). That silently cost a debugging session:
     * the exception was reported with an empty stack, and libart's own throw probe caps at 40 and had
     * been saturated by unrelated throws, so there was no stack from either source. Walk
     * getStackTrace() by hand instead — it works — and follow the cause chain.
     */
    public static void logThrowable(Throwable t) {
        try {
            nativeLog(render(t));
        } catch (Throwable ignored) {
            try { nativeLog("logThrowable failed for " + t.getClass().getName()); }
            catch (Throwable ignored2) { }
        }
    }

    private static String render(Throwable t) {
        StringBuilder sb = new StringBuilder();
        String prefix = "";
        for (Throwable c = t; c != null && sb.length() < 6000; c = c.getCause()) {
            sb.append(prefix).append(c.getClass().getName());
            String m = c.getMessage();
            if (m != null) sb.append(": ").append(m);
            sb.append('\n');
            StackTraceElement[] fr = c.getStackTrace();
            if (fr == null || fr.length == 0) {
                sb.append("\tat <no frames — getStackTrace() empty>\n");
            } else {
                for (int i = 0; i < fr.length && i < 40; i++) sb.append("\tat ").append(fr[i]).append('\n');
                if (fr.length > 40) sb.append("\t... ").append(fr.length - 40).append(" more\n");
            }
            prefix = "Caused by: ";
            if (c.getCause() == c) break;
        }
        return sb.toString();
    }

    /** Ordinary named class implementing the interface. */
    static final class Named implements P {
        @Override public boolean test(int c) { return c >= 'a' && c <= 'z'; }
    }

    static boolean direct(Named n, int c) { return n.test(c); }

    /** The exact shape Pattern uses: a static factory returning a capturing lambda. */
    /** Model-shaped: no no-arg constructor, so Gson must use Unsafe.allocateInstance. */
    public static final class Model {
        public String name; public int count;
        public Model(String name, int count) { this.name = name; this.count = count; }
    }

    static P single(int c) { return ch -> ch == c; }
    static P range(int lo, int hi) { return ch -> lo <= ch && ch <= hi; }   // invoke-virtual on a real class

    public static String run() {
        StringBuilder sb = new StringBuilder();

        // 1. desugared lambda, called through the interface
        P lambda = c -> c >= 'a' && c <= 'z';
        sb.append("lambda_iface(m)=").append(lambda.test('m'))
          .append(" (want true) lambda_iface(M)=").append(lambda.test('M')).append(" (want false)");

        // 2. named class, called through the interface  -> isolates "desugared" from "interface"
        P named = new Named();
        sb.append(" | named_iface(m)=").append(named.test('m'))
          .append(" (want true) named_iface(M)=").append(named.test('M')).append(" (want false)");

        // 3. named class, called directly (invoke-virtual) -> isolates the interface itself
        Named n = new Named();
        sb.append(" | named_virtual(m)=").append(direct(n, 'm')).append(" (want true)");

        // 4. the same comparison inline, no dispatch at all -> proves the logic itself
        int c = 'm';
        sb.append(" | inline(m)=").append(c >= 'a' && c <= 'z').append(" (want true)");

        // CharProperty.match() does: ch = Character.codePointAt(seq, i); predicate.is(ch) ...
        // Slice (literal) nodes instead use seq.charAt(). That is the only structural difference
        // between the regexes that work and the ones that fail.
        // CAPTURING lambdas — exactly how Pattern builds its predicates:
        //   static CharPredicate single(int c) { return ch -> ch == c; }
        // A non-capturing lambda desugars to a singleton; a capturing one gets instance fields.
        int k = 'a';
        P captured = c2 -> c2 == k;
        sb.append(" || captured(a)=").append(captured.test('a')).append(" (want true)");
        sb.append(" captured(b)=").append(captured.test('b')).append(" (want false)");
        P fromFactory = single('a');
        sb.append(" factory(a)=").append(fromFactory.test('a')).append(" (want true)");
        P rangeP = range('a', 'z');
        sb.append(" range(m)=").append(rangeP.test('m')).append(" (want true)");

        CharSequence cs = "abc";
        sb.append(" || codePointAt(cs,0)=").append(Character.codePointAt(cs, 0)).append(" (want 97)");
        sb.append(" charAt=").append((int) cs.charAt(0)).append(" (want 97)");
        sb.append(" charCount(97)=").append(Character.charCount(97)).append(" (want 1)");
        sb.append(" isLetter(m)=").append(Character.isLetter('m')).append(" (want true)");
        sb.append(" strCodePointAt=").append("abc".codePointAt(0)).append(" (want 97)");
        // §449: Gson cannot use a no-arg ctor for noice's api models (28 NoSuchMethodException in
        // the log), so it falls back to sun.misc.Unsafe.allocateInstance. If that path is dead the
        // manifest parses to nothing, nothing is inserted, and the UI says "unknown error" with no
        // exception anywhere — exactly what we observe.
        sb.append(" || UNSAFE:");
        try {
            Class<?> u = Class.forName("sun.misc.Unsafe");
            java.lang.reflect.Field f = u.getDeclaredField("theUnsafe");
            f.setAccessible(true);
            Object unsafe = f.get(null);
            sb.append(" theUnsafe=").append(unsafe != null);
            java.lang.reflect.Method m = u.getMethod("allocateInstance", Class.class);
            Object o = m.invoke(unsafe, Model.class);
            sb.append(" allocateInstance=").append(o != null ? o.getClass().getSimpleName() : "null");
            if (o != null) {
                java.lang.reflect.Field nf = Model.class.getDeclaredField("name");
                nf.setAccessible(true);
                nf.set(o, "hello");
                sb.append(" fieldSet=").append(((Model) o).name);
            }
        } catch (Throwable e) {
            sb.append(" EX(").append(e.getClass().getSimpleName()).append(":")
              .append(String.valueOf(e.getMessage())).append(")");
        }

        // §448: OkHttp sends "Accept-Encoding: gzip" and transparently gunzips the response.
        // If zlib/Inflater is stubbed in this runtime, the body decompresses to NOTHING — Gson then
        // builds an empty manifest, nothing is inserted, and the UI reports "unknown error" with no
        // exception anywhere. Round-trip both Deflater/Inflater and GZIP streams to find out.
        sb.append(" || ZIP:");
        try {
            String src = "the quick brown fox jumps over the lazy dog 0123456789 the quick brown fox";
            byte[] raw = src.getBytes("UTF-8");
            java.util.zip.Deflater def = new java.util.zip.Deflater();
            def.setInput(raw); def.finish();
            byte[] comp = new byte[512];
            int clen = def.deflate(comp); def.end();
            sb.append(" deflate=").append(clen).append("B");
            java.util.zip.Inflater inf = new java.util.zip.Inflater();
            inf.setInput(comp, 0, clen);
            byte[] out = new byte[512];
            int olen = inf.inflate(out); inf.end();
            String back = new String(out, 0, olen, "UTF-8");
            sb.append(" inflate=").append(olen).append("B ok=").append(src.equals(back));
        } catch (Throwable e) {
            sb.append(" deflate/inflate EX(").append(e.getClass().getSimpleName())
              .append(":").append(e.getMessage()).append(")");
        }
        try {
            String src = "{\"groups\":[{\"id\":\"nature\",\"name\":\"Nature\"}],\"sounds\":[]}";
            java.io.ByteArrayOutputStream bos = new java.io.ByteArrayOutputStream();
            java.util.zip.GZIPOutputStream gz = new java.util.zip.GZIPOutputStream(bos);
            gz.write(src.getBytes("UTF-8")); gz.close();
            byte[] gzipped = bos.toByteArray();
            java.util.zip.GZIPInputStream gin = new java.util.zip.GZIPInputStream(
                    new java.io.ByteArrayInputStream(gzipped));
            java.io.ByteArrayOutputStream rec = new java.io.ByteArrayOutputStream();
            byte[] buf = new byte[256]; int zn;
            while ((zn = gin.read(buf)) > 0) rec.write(buf, 0, zn);
            gin.close();
            String back = new String(rec.toByteArray(), "UTF-8");
            sb.append(" | gzip=").append(gzipped.length).append("B gunzip=")
              .append(back.length()).append("B ok=").append(src.equals(back));
        } catch (Throwable e) {
            sb.append(" | gzip EX(").append(e.getClass().getSimpleName())
              .append(":").append(e.getMessage()).append(")");
        }

        // §442c: characterise the ICU regex engine. Literals already match, so ICU is alive and has
        // the input; the question is which constructs need Unicode property/set data.
        String[][] rx = {
            {"abc",        "abc", "1"},   // literal control
            {"a|b",        "a",   "1"},   // alternation, no character class
            {"(a)",        "a",   "1"},   // group, no character class
            {"[abc]",      "a",   "1"},   // explicit set, no range
            {"[a-z]",      "a",   "1"},   // range
            {"\\d",        "1",   "1"},   // predefined class
            {"\\p{Alpha}", "a",   "1"},   // explicit Unicode property
            {".",          "a",   "1"},   // any
            {"a?",         "a",   "1"},   // optional quantifier on a literal
            {"aa*",        "aa",  "1"},   // star on a literal
        };
        sb.append(" || ICU:");
        for (String[] t : rx) {
            boolean got;
            try { got = java.util.regex.Pattern.matches(t[0], t[1]); }
            catch (Throwable e) { sb.append(" /").append(t[0]).append("/=EX(")
                                    .append(e.getClass().getSimpleName()).append(")"); continue; }
            boolean want = t[2].equals("1");
            sb.append(" /").append(t[0]).append("/=").append(got ? 1 : 0).append(got == want ? "" : "!!");
        }
        // §442d: is ICU compiling in LITERAL mode? Then metacharacters match themselves.
        sb.append(" || LITERALMODE:");
        String[][] lit = {
            {"[abc]", "[abc]"}, {"(a)", "(a)"}, {"a|b", "a|b"}, {".", "."}, {"\\d", "\\d"},
        };
        for (String[] t : lit) {
            boolean got;
            try { got = java.util.regex.Pattern.matches(t[0], t[1]); }
            catch (Throwable e) { sb.append(" /").append(t[0]).append("/=EX"); continue; }
            sb.append(" /").append(t[0]).append("/~self=").append(got ? 1 : 0);
        }
        // group count tells us whether the compiler parsed structure at all
        try {
            java.util.regex.Matcher m2 = java.util.regex.Pattern.compile("(a)(b)").matcher("ab");
            sb.append(" groupCount(\"(a)(b)\")=").append(m2.groupCount()).append(" (want 2)");
        } catch (Throwable e) { sb.append(" groupCount=EX(").append(e.getClass().getSimpleName()).append(")"); }
        // §466: noice's engine rejects a null audioAttributes when onStartCommand builds the
        // player ("Parameter specified as non-null is null: ... parameter audioAttributes").
        // Check whether the framework can build one at all in this runtime.
        sb.append(" || AUDIOATTR:");
        try {
            Class<?> aa = Class.forName("android.media.AudioAttributes");
            Class<?> b  = Class.forName("android.media.AudioAttributes$Builder");
            Object bld = b.getConstructor().newInstance();
            b.getMethod("setUsage", int.class).invoke(bld, Integer.valueOf(1));        // USAGE_MEDIA
            b.getMethod("setContentType", int.class).invoke(bld, Integer.valueOf(2));  // MUSIC
            Object built = b.getMethod("build").invoke(bld);
            sb.append(" build=").append(built == null ? "NULL" : built.getClass().getSimpleName());
            if (built != null) {
                Object u = aa.getMethod("getUsage").invoke(built);
                sb.append(" usage=").append(u);
            }
        } catch (Throwable e) {
            sb.append(" EX(").append(e.getClass().getSimpleName()).append(":")
              .append(String.valueOf(e.getMessage())).append(")");
        }
        return sb.toString();
    }
}
