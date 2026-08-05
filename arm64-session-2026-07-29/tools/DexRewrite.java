// DexRewrite <appdex> <helperdex> <outdex>
// WESTLAKE §440: rewrite invoke-interface-on-Proxy sites to invoke-static into westlake/WlProxy.
import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.iface.instruction.*;
import com.android.tools.smali.dexlib2.iface.instruction.formats.*;
import com.android.tools.smali.dexlib2.iface.reference.MethodReference;
import com.android.tools.smali.dexlib2.immutable.*;
import com.android.tools.smali.dexlib2.immutable.instruction.*;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableMethodReference;
import com.android.tools.smali.dexlib2.writer.io.FileDataStore;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;
import java.io.File;
import java.util.*;

public class DexRewrite {
    // §524: was 3 (§440's q6/b.a, q6/b.b, g9/f.value). Now 4, adding q6/b.c — cdnApi.resource(path),
    // which CdnSoundDataSource.open() invokes for every audio segment and is therefore the
    // most-executed Proxy call during playback.
    // ⚠️A static scan finds NINETEEN invoke-interface sites on the four Retrofit service interfaces
    // NoiceApiClient proxies (q6/a account, q6/b cdn, q6/c internal-account, q6/d subscription).
    // The 15 still unpatched are latent §436 crashes on the sign-in / subscription / token-refresh
    // paths. They need per-arity helpers (sites use 2..5 registers) — add them WITH evidence, one at
    // a time, rather than in a batch: this assertion exists so the count can never drift silently.
    static final int EXPECTED_SITES = 4;
    static final String H = "Lwestlake/WlProxy;";
    static final String OBJ = "Ljava/lang/Object;";
    static int patched = 0;

    // target defining class + name  ->  helper method name
    static String helperFor(MethodReference mr) {
        String c = mr.getDefiningClass(), n = mr.getName();
        if (c.equals("Lq6/b;") && n.equals("b")) return "svcB";
        if (c.equals("Lq6/b;") && n.equals("a")) return "svcA";
        if (c.equals("Lg9/f;") && n.equals("value")) return "annValue";
        // §524: q6/b.c is cdnApi.resource(path) — the Proxy call CdnSoundDataSource.open() makes for
        // every audio segment, and the one most likely to be executing when the §436 SEGV fires.
        if (c.equals("Lq6/b;") && n.equals("c")) return "svcC";
        return null;
    }

    static ImmutableMethodReference helperRef(String name) {
        if (name.equals("annValue"))
            return new ImmutableMethodReference(H, name, Arrays.asList(OBJ), "Ljava/lang/String;");
        return new ImmutableMethodReference(H, name, Arrays.asList(OBJ, OBJ), OBJ);
    }

    static MethodImplementation rewriteImpl(MethodImplementation impl, String owner, String mname) {
        List<Instruction> out = new ArrayList<>();
        boolean changed = false;
        for (Instruction ins : impl.getInstructions()) {
            String helper = null;
            if (ins.getOpcode() == Opcode.INVOKE_INTERFACE) {
                helper = helperFor((MethodReference) ((ReferenceInstruction) ins).getReference());
            }
            if (helper != null) {
                Instruction35c o = (Instruction35c) ins;
                out.add(new ImmutableInstruction35c(Opcode.INVOKE_STATIC,
                        o.getRegisterCount(), o.getRegisterC(), o.getRegisterD(),
                        o.getRegisterE(), o.getRegisterF(), o.getRegisterG(), helperRef(helper)));
                changed = true; patched++;
                System.out.printf("  patched %s.%s -> WlProxy.%s (regs=%d)%n",
                        owner, mname, helper, o.getRegisterCount());
            } else {
                out.add(ins);
            }
        }
        if (!changed) return impl;
        return new ImmutableMethodImplementation(impl.getRegisterCount(), out,
                impl.getTryBlocks(), impl.getDebugItems());
    }

    public static void main(String[] a) throws Exception {
        DexFile app = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
        DexFile helper = DexFileFactory.loadDexFile(new File(a[1]), Opcodes.forApi(34));
        DexPool pool = new DexPool(Opcodes.forApi(34));
        int classes = 0;
        for (ClassDef c : app.getClasses()) {
            List<Method> direct = new ArrayList<>(), virt = new ArrayList<>();
            boolean touched = false;
            for (int pass = 0; pass < 2; pass++) {
                Iterable<? extends Method> src = (pass == 0) ? c.getDirectMethods() : c.getVirtualMethods();
                for (Method m : src) {
                    MethodImplementation impl = m.getImplementation();
                    MethodImplementation ni = (impl == null) ? null
                            : rewriteImpl(impl, c.getType(), m.getName());
                    if (ni != impl) touched = true;
                    Method nm = new ImmutableMethod(m.getDefiningClass(), m.getName(),
                            m.getParameters(), m.getReturnType(), m.getAccessFlags(),
                            m.getAnnotations(), m.getHiddenApiRestrictions(), ni);
                    (pass == 0 ? direct : virt).add(nm);
                }
            }
            if (touched) {
                pool.internClass(new ImmutableClassDef(c.getType(), c.getAccessFlags(),
                        c.getSuperclass(), c.getInterfaces(), c.getSourceFile(), c.getAnnotations(),
                        c.getStaticFields(), c.getInstanceFields(), direct, virt));
            } else {
                pool.internClass(c);
            }
            classes++;
        }
        for (ClassDef c : helper.getClasses()) { pool.internClass(c); classes++;
            System.out.println("  merged helper class " + c.getType()); }
        pool.writeTo(new FileDataStore(new File(a[2])));
        System.out.println("classes=" + classes + " patched_sites=" + patched);
        if (patched != EXPECTED_SITES) { System.out.println("FAIL: expected " + EXPECTED_SITES + " sites, got " + patched); System.exit(2); }
    }
}
