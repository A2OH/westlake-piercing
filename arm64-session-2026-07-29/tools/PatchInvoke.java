// PatchInvoke <in.dex> <out.dex> <Lclass;>:<method>:<index>:<reg>
//
// Replace the instruction at <index> with `invoke-interface {vREG}, Callable.call()Ljava/lang/Object;`.
//
// §486: the sound-metadata DAO hands its work to CoroutinesRoom.execute, which dispatches onto Room's
// TransactionExecutor. On this runtime that executor is wedged — probed live: active task non-null,
// queue non-empty, delegate healthy — and setting inTransaction=false did NOT help, because a
// TransactionElement in the caller's coroutine context routes back to the transaction dispatcher
// regardless. The enclosing withTransaction holds the single transaction thread while the nested query
// queues behind it: a self-deadlock that Room's thread-confinement normally prevents.
//
// Bypass the dispatch and invoke the Callable inline. The query is a small indexed SELECT, so running
// it on the calling coroutine's thread is cheap. ★invoke-static and invoke-interface are both format
// 35c, so this is a size-preserving swap like §440/§464.
import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.builder.MutableMethodImplementation;
import com.android.tools.smali.dexlib2.builder.instruction.BuilderInstruction35c;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.immutable.*;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableMethodReference;
import com.android.tools.smali.dexlib2.writer.io.FileDataStore;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;
import java.io.File;
import java.util.*;

public class PatchInvoke {
    public static void main(String[] a) throws Exception {
        if (a.length < 3) { System.out.println("usage: PatchInvoke <in> <out> <Lcls;>:<m>:<idx>:<reg>"); System.exit(1); }
        List<String[]> sites = new ArrayList<>();
        for (int i = 2; i < a.length; i++) sites.add(a[i].split(":"));
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
                        int at = Integer.parseInt(s[2]), reg = Integer.parseInt(s[3]);
                        mi.replaceInstruction(at, new BuilderInstruction35c(Opcode.INVOKE_INTERFACE,
                                1, reg, 0, 0, 0, 0,
                                new ImmutableMethodReference("Ljava/util/concurrent/Callable;", "call",
                                        Collections.<CharSequence>emptyList(), "Ljava/lang/Object;")));
                        out = new ImmutableMethod(m.getDefiningClass(), m.getName(), m.getParameters(),
                                m.getReturnType(), m.getAccessFlags(), m.getAnnotations(),
                                m.getHiddenApiRestrictions(), mi);
                        patched++; touched = true;
                        System.out.println("  -> Callable.call() on v" + reg + " at " + s[0] + "." + s[1] + " @" + at);
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
