// InjectTrace <in.dex> <out.dex> <Lclass;>:<method>:<index>:<label> [...]
//
// Inject execution-trace calls into an app dex. index -1 = method entry, otherwise the instruction
// index to insert BEFORE.
//
// §480: the play path stops silently — onStartCommand succeeds, no decoder is created, no CDN request
// is made, and no exception is reported. Runtime introspection is unusually hard here:
//   - libart's throw probe caps at 40 and is saturated by unrelated noise
//   - Throwable.getStackTrace() returns EMPTY in this runtime, and every printStackTrace overload
//     is a no-op, so app-side stack dumps are worthless
// So trace execution directly instead of trying to catch a throw.
//
// ★The trick that makes this safe: each trace site calls a distinct ZERO-ARG static
// (westlake.WlTrace.sN()), so the injected instruction needs NO free register. Injecting anything
// that needs a register would mean re-allocating registers around parameter registers, which is where
// bytecode injection usually goes wrong.
import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.builder.MutableMethodImplementation;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.immutable.*;
import com.android.tools.smali.dexlib2.immutable.instruction.*;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableMethodReference;
import com.android.tools.smali.dexlib2.writer.io.FileDataStore;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;
import java.io.File;
import java.util.*;

public class InjectTrace {
    static final String TRACE = "Lwestlake/WlTrace;";
    static int injected = 0;

    static class Site {
        final String cls, method; final int index; final String label; final int reg;
        Site(String c, String m, int i, String l, int r) { cls = c; method = m; index = i; label = l; reg = r; }
    }

    public static void main(String[] a) throws Exception {
        if (a.length < 3) {
            System.out.println("usage: InjectTrace <in.dex> <out.dex> <Lclass;>:<method>:<index>:<label> [...]");
            System.exit(1);
        }
        List<Site> sites = new ArrayList<>();
        for (int i = 2; i < a.length; i++) {
            String[] p = a[i].split(":");
            if (p.length != 4 && p.length != 5) { System.out.println("bad site: " + a[i]); System.exit(1); }
            // 5th field = register to pass to WlTrace.obj(Object), for logging a VALUE rather than
            // just "we got here". Use Disasm's register column to find it.
            int reg = (p.length == 5) ? Integer.parseInt(p[4]) : -1;
            sites.add(new Site(p[0], p[1], Integer.parseInt(p[2]), p[3], reg));
        }
        DexFile in = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
        DexPool pool = new DexPool(Opcodes.forApi(34));

        for (ClassDef c : in.getClasses()) {
            boolean touched = false;
            List<Method> direct = new ArrayList<>(), virt = new ArrayList<>();
            for (int pass = 0; pass < 2; pass++) {
                for (Method m : (pass == 0 ? c.getDirectMethods() : c.getVirtualMethods())) {
                    Method out = m;
                    // Collect every site for THIS method, then apply them in DESCENDING index order
                    // against ONE builder. Two bugs live here if you do it naively: rebuilding the
                    // builder per site keeps only the last injection, and inserting low-to-high
                    // shifts every later index by the number already inserted.
                    // A site's method field may carry a parameter signature, e.g.
                    //   <init>(ILjava/lang/Throwable;I)V
                    // Overloads do NOT share a register layout: for ExoPlaybackException, v11 holds
                    // the Throwable in the 3-arg ctor but an int in the 8-arg one, so a name-only
                    // match would inject a type-unsafe call into the wrong overload.
                    List<Site> mine = new ArrayList<>();
                    for (Site s : sites) {
                        if (!c.getType().equals(s.cls)) continue;
                        int paren = s.method.indexOf('(');
                        if (paren < 0) {
                            if (m.getName().equals(s.method)) mine.add(s);
                        } else {
                            String wantName = s.method.substring(0, paren);
                            if (!m.getName().equals(wantName)) continue;
                            StringBuilder sig = new StringBuilder("(");
                            for (CharSequence pt : m.getParameterTypes()) sig.append(pt);
                            sig.append(')').append(m.getReturnType());
                            if (sig.toString().equals(s.method.substring(paren))) mine.add(s);
                        }
                    }
                    if (!mine.isEmpty() && m.getImplementation() != null) {
                        Collections.sort(mine, new Comparator<Site>() {
                            @Override public int compare(Site x, Site y) { return Integer.compare(y.index, x.index); }
                        });
                        MutableMethodImplementation mi = new MutableMethodImplementation(m.getImplementation());
                        for (Site s : mine) {
                            int at = (s.index < 0) ? 0 : s.index;
                            if (at > mi.getInstructions().size()) {
                                System.out.println("  SKIP " + s.label + ": index " + at + " past end");
                                continue;
                            }
                            if (s.reg >= 0) {
                                // "fix.<name>" targets westlake.WlFix.<name>(Object); anything else
                                // goes to WlTrace.obj(Object), which just prints the value.
                                boolean toFix = s.label.startsWith("fix.");
                                String owner = toFix ? "Lwestlake/WlFix;" : TRACE;
                                String name  = toFix ? s.label.substring(4) : "obj";
                                mi.addInstruction(at, new com.android.tools.smali.dexlib2.builder.instruction
                                        .BuilderInstruction35c(Opcode.INVOKE_STATIC, 1, s.reg, 0, 0, 0, 0,
                                        new ImmutableMethodReference(owner, name,
                                                Collections.<CharSequence>singletonList("Ljava/lang/Object;"), "V")));
                            } else {
                                // A label of "probeExecutors" targets westlake.WlFix instead of
                                // WlTrace, so one-off diagnostics can live in their own class.
                                String owner = "probeExecutors".equals(s.label) ? "Lwestlake/WlFix;" : TRACE;
                                mi.addInstruction(at, new com.android.tools.smali.dexlib2.builder.instruction
                                        .BuilderInstruction35c(Opcode.INVOKE_STATIC, 0, 0, 0, 0, 0, 0,
                                        new ImmutableMethodReference(owner, s.label,
                                                Collections.<CharSequence>emptyList(), "V")));
                            }
                            injected++;
                            System.out.println("  injected " + s.label + " -> " + s.cls + "." + s.method
                                    + " @" + at);
                        }
                        out = new ImmutableMethod(m.getDefiningClass(), m.getName(), m.getParameters(),
                                m.getReturnType(), m.getAccessFlags(), m.getAnnotations(),
                                m.getHiddenApiRestrictions(), mi);
                        touched = true;
                    }
                    (pass == 0 ? direct : virt).add(out);
                }
            }
            if (touched) {
                pool.internClass(new ImmutableClassDef(c.getType(), c.getAccessFlags(), c.getSuperclass(),
                        c.getInterfaces(), c.getSourceFile(), c.getAnnotations(), c.getStaticFields(),
                        c.getInstanceFields(), direct, virt));
            } else {
                pool.internClass(c);
            }
        }
        System.out.println("injected=" + injected + " of " + sites.size() + " site(s)");
        if (injected < sites.size()) { System.out.println("FAIL: some sites did not match"); System.exit(2); }
        pool.writeTo(new FileDataStore(new File(a[1])));
    }
}
