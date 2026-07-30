// FwRewrite <in.dex> <helper.dex> <out.dex>
// Rewrite invoke-interface/range on the two IActivityManager methods -> invoke-static/range into
// android.app.WlAmsBridge (same 3rc format, same register count, register list preserved).
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

public class FwRewrite {
    static final String AM = "Landroid/app/IActivityManager;";
    static final String H  = "Landroid/app/WlAmsBridge;";
    static int patched = 0;

    static ImmutableMethodReference helperFor(MethodReference r) {
        if (!r.getDefiningClass().equals(AM)) return null;
        List<String> p = new ArrayList<>();
        if (r.getName().equals("registerReceiverWithFeature")) {
            p.add(AM);
            for (CharSequence c : r.getParameterTypes()) p.add(c.toString());
            return new ImmutableMethodReference(H, "registerReceiverWithFeature", p, r.getReturnType());
        }
        if (r.getName().equals("getIntentSenderWithFeature")) {
            p.add(AM);
            for (CharSequence c : r.getParameterTypes()) p.add(c.toString());
            return new ImmutableMethodReference(H, "getIntentSenderWithFeature", p, r.getReturnType());
        }
        return null;
    }

    static MethodImplementation rewrite(MethodImplementation impl, String owner, String mname) {
        List<Instruction> out = new ArrayList<>();
        boolean changed = false;
        for (Instruction ins : impl.getInstructions()) {
            ImmutableMethodReference hr = null;
            if (ins.getOpcode() == Opcode.INVOKE_INTERFACE_RANGE) {
                hr = helperFor((MethodReference) ((ReferenceInstruction) ins).getReference());
            }
            if (hr != null) {
                Instruction3rc o = (Instruction3rc) ins;
                out.add(new ImmutableInstruction3rc(Opcode.INVOKE_STATIC_RANGE,
                        o.getStartRegister(), o.getRegisterCount(), hr));
                changed = true; patched++;
                System.out.printf("  patched %s.%s -> WlAmsBridge.%s (regs=%d start=%d)%n",
                        owner, mname, hr.getName(), o.getRegisterCount(), o.getStartRegister());
            } else {
                out.add(ins);
            }
        }
        if (!changed) return impl;
        return new ImmutableMethodImplementation(impl.getRegisterCount(), out,
                impl.getTryBlocks(), impl.getDebugItems());
    }

    public static void main(String[] a) throws Exception {
        DexFile in = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
        DexFile helper = DexFileFactory.loadDexFile(new File(a[1]), Opcodes.forApi(34));
        DexPool pool = new DexPool(Opcodes.forApi(34));
        int n = 0;
        for (ClassDef c : in.getClasses()) {
            List<Method> direct = new ArrayList<>(), virt = new ArrayList<>();
            boolean touched = false;
            for (int pass = 0; pass < 2; pass++) {
                for (Method m : (pass == 0 ? c.getDirectMethods() : c.getVirtualMethods())) {
                    MethodImplementation impl = m.getImplementation();
                    MethodImplementation ni = (impl == null) ? null : rewrite(impl, c.getType(), m.getName());
                    if (ni != impl) touched = true;
                    Method nm = new ImmutableMethod(m.getDefiningClass(), m.getName(), m.getParameters(),
                            m.getReturnType(), m.getAccessFlags(), m.getAnnotations(),
                            m.getHiddenApiRestrictions(), ni);
                    (pass == 0 ? direct : virt).add(nm);
                }
            }
            if (touched) {
                pool.internClass(new ImmutableClassDef(c.getType(), c.getAccessFlags(), c.getSuperclass(),
                        c.getInterfaces(), c.getSourceFile(), c.getAnnotations(), c.getStaticFields(),
                        c.getInstanceFields(), direct, virt));
            } else { pool.internClass(c); }
            n++;
        }
        for (ClassDef c : helper.getClasses()) { pool.internClass(c); n++;
            System.out.println("  merged " + c.getType()); }
        pool.writeTo(new FileDataStore(new File(a[2])));
        System.out.println("classes=" + n + " patched_sites=" + patched);
        if (patched == 0) { System.out.println("FAIL: nothing patched"); System.exit(2); }
    }
}
