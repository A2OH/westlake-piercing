// Inject Throwable.printStackTrace() at the catch handler that builds the Failure resource, so the
// exception reaches stderr. libart's SCTHROW probe caps at 40 and is saturated by benign throws.
import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.iface.instruction.*;
import com.android.tools.smali.dexlib2.builder.*;
import com.android.tools.smali.dexlib2.builder.instruction.*;
import com.android.tools.smali.dexlib2.immutable.*;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableMethodReference;
import com.android.tools.smali.dexlib2.writer.io.FileDataStore;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;
import java.io.File;
import java.util.*;

public class PatchLog {
    // §465: instrument every catch handler in noice's playback-related classes so a silently
    // swallowed exception in the play path becomes visible.
    static boolean wanted(String type) {
        if (!type.startsWith("Lcom/github/ashutoshgngwr/noice/")) return false;
        String t = type.toLowerCase();
        return t.contains("service") || t.contains("player") || t.contains("sound")
            || t.contains("fetchnetworkboundresource");
    }
    static int injected = 0;

    static MethodImplementation patch(MethodImplementation impl) {
        MutableMethodImplementation mut = new MutableMethodImplementation(impl);
        List<BuilderInstruction> list = mut.getInstructions();
        // walk backwards so earlier indices stay valid
        for (int i = list.size() - 1; i >= 0; i--) {
            if (list.get(i).getOpcode() != Opcode.MOVE_EXCEPTION) continue;
            int reg = ((OneRegisterInstruction) list.get(i)).getRegisterA();
            if (reg > 15) continue;                       // 35c needs a 4-bit register
            mut.addInstruction(i + 1, new BuilderInstruction35c(
                    Opcode.INVOKE_STATIC, 1, reg, 0, 0, 0, 0,
                    new ImmutableMethodReference("Ladapter/compat/WlProbe;", "logThrowable",
                            Collections.singletonList("Ljava/lang/Throwable;"), "V")));
            injected++;
        }
        return mut;
    }

    public static void main(String[] a) throws Exception {
        DexFile in = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
        DexPool pool = new DexPool(Opcodes.forApi(34));
        for (ClassDef c : in.getClasses()) {
            if (!wanted(c.getType())) { pool.internClass(c); continue; }
            List<Method> direct = new ArrayList<>(), virt = new ArrayList<>();
            for (int pass = 0; pass < 2; pass++) {
                for (Method m : (pass == 0 ? c.getDirectMethods() : c.getVirtualMethods())) {
                    MethodImplementation impl = m.getImplementation();
                    MethodImplementation ni = impl;
                    if (impl != null) ni = patch(impl);
                    Method nm = new ImmutableMethod(m.getDefiningClass(), m.getName(),
                            m.getParameters(), m.getReturnType(), m.getAccessFlags(),
                            m.getAnnotations(), m.getHiddenApiRestrictions(), ni);
                    (pass == 0 ? direct : virt).add(nm);
                }
            }
            pool.internClass(new ImmutableClassDef(c.getType(), c.getAccessFlags(),
                    c.getSuperclass(), c.getInterfaces(), c.getSourceFile(), c.getAnnotations(),
                    c.getStaticFields(), c.getInstanceFields(), direct, virt));
        }
        pool.writeTo(new FileDataStore(new File(a[1])));
        System.out.println("injected printStackTrace at " + injected + " catch handler(s)");
        if (injected == 0) System.exit(2);
    }
}
