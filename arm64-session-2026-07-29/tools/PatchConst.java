// PatchConst <in.dex> <out.dex> <Lclass;>:<method>:<index>:<reg>:<value> [...]
//
// Insert `const/4 vREG, VALUE` before an instruction. Deliberately const/4 and an EXISTING register,
// so the method's register count is untouched — bumping it would shift parameter registers, which is
// how bytecode edits usually break.
//
// §485: noice's sound-metadata DAO calls
//     CoroutinesRoom.execute(db, inTransaction=TRUE, signal, callable, continuation)
// so the read is dispatched to Room's TransactionExecutor. That executor is WEDGED on this runtime —
// probed live: active task non-null, queue size 1, delegate healthy — because an earlier
// withTransaction block never completed. The query therefore never runs, sound metadata never loads,
// and playback silently parks in BUFFERING.
// The read does not need a transaction, so set the flag to false and let it run on the (working)
// query executor. The register holding it is dead after the bind calls, so overwriting it is safe.
import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.builder.MutableMethodImplementation;
import com.android.tools.smali.dexlib2.builder.instruction.BuilderInstruction11n;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.immutable.*;
import com.android.tools.smali.dexlib2.writer.io.FileDataStore;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;
import java.io.File;
import java.util.*;

public class PatchConst {
    public static void main(String[] a) throws Exception {
        if (a.length < 3) { System.out.println("usage: PatchConst <in> <out> <Lcls;>:<m>:<idx>:<reg>:<val>"); System.exit(1); }
        List<String[]> sites = new ArrayList<>();
        for (int i = 2; i < a.length; i++) {
            String[] p = a[i].split(":");
            if (p.length != 5) { System.out.println("bad site: " + a[i]); System.exit(1); }
            sites.add(p);
        }
        DexFile in = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
        DexPool pool = new DexPool(Opcodes.forApi(34));
        int patched = 0;
        for (ClassDef c : in.getClasses()) {
            boolean touched = false;
            List<Method> direct = new ArrayList<>(), virt = new ArrayList<>();
            for (int pass = 0; pass < 2; pass++) {
                for (Method m : (pass == 0 ? c.getDirectMethods() : c.getVirtualMethods())) {
                    Method out = m;
                    for (String[] s : sites) {
                        if (!c.getType().equals(s[0]) || !m.getName().equals(s[1])) continue;
                        if (m.getImplementation() == null) continue;
                        MutableMethodImplementation mi = new MutableMethodImplementation(m.getImplementation());
                        int at = Integer.parseInt(s[2]), reg = Integer.parseInt(s[3]), val = Integer.parseInt(s[4]);
                        mi.addInstruction(at, new BuilderInstruction11n(Opcode.CONST_4, reg, val));
                        out = new ImmutableMethod(m.getDefiningClass(), m.getName(), m.getParameters(),
                                m.getReturnType(), m.getAccessFlags(), m.getAnnotations(),
                                m.getHiddenApiRestrictions(), mi);
                        patched++; touched = true;
                        System.out.println("  const/4 v" + reg + ", " + val + " -> " + s[0] + "." + s[1] + " @" + at);
                    }
                    (pass == 0 ? direct : virt).add(out);
                }
            }
            if (touched) pool.internClass(new ImmutableClassDef(c.getType(), c.getAccessFlags(), c.getSuperclass(),
                    c.getInterfaces(), c.getSourceFile(), c.getAnnotations(), c.getStaticFields(),
                    c.getInstanceFields(), direct, virt));
            else pool.internClass(c);
        }
        System.out.println("patched=" + patched + " of " + sites.size());
        if (patched < sites.size()) { System.out.println("FAIL"); System.exit(2); }
        pool.writeTo(new FileDataStore(new File(a[1])));
    }
}
