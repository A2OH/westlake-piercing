// SetConst <in.dex> <out.dex> <Lclass;>:<method>:<index>:<reg>:<newValue> [...]
//
// REPLACE the literal of an existing const instruction, in place. Sibling of PatchConst, which
// INSERTS a `const/4`. Two reasons that one could not be used here:
//   * const/4 is format 11n — its register field is 4 bits, so it cannot name a register above v15.
//     The site this was written for is v21.
//   * The target instruction already exists and already writes the right register; replacing its
//     literal keeps the instruction count identical, so no branch target or try/catch range moves.
//
// §545: WindowSessionAdapter.relayout reverse-pushes IWindow.resized(..., forceLayout=TRUE, ...) on
// EVERY relayout. ViewRootImpl.handleResized treats forceLayout as "relayout even though nothing
// changed", so it calls requestLayout() -> traversal -> relayout -> reverse-push -> forever. That
// loop is why the idle main thread sat at ~88% CPU running ConstraintLayout measure/layout with a
// completely static screen. Setting the flag to 0 lets handleResized take its early-exit path when
// nothing actually changed; at bootstrap the frame genuinely changes (0x0 -> 1200x1920), so the
// first traversal still happens and the 0x0 death spiral this push exists to prevent stays fixed.
//
// The source tree already has the intended fix (a `mReversePushed` guard that pushes once), but the
// deployed oh-adapter-framework.jar predates it: classes.dex is built by build_aosp_fw.sh, while the
// recipe that gets run regularly (build-ohaf-jar.sh) only ever swaps classes2.dex. Hence surgery.
import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.builder.MutableMethodImplementation;
import com.android.tools.smali.dexlib2.builder.instruction.BuilderInstruction21s;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.iface.instruction.*;
import com.android.tools.smali.dexlib2.immutable.*;
import com.android.tools.smali.dexlib2.writer.io.FileDataStore;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;
import java.io.File;
import java.util.*;

public class SetConst {
    public static void main(String[] a) throws Exception {
        if (a.length < 3) {
            System.out.println("usage: SetConst <in> <out> <Lcls;>:<m>:<idx>:<reg>:<newVal>");
            System.exit(1);
        }
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
                        int at = Integer.parseInt(s[2]);
                        int reg = Integer.parseInt(s[3]);
                        int val = Integer.parseInt(s[4]);
                        MutableMethodImplementation mi =
                                new MutableMethodImplementation(m.getImplementation());
                        // ★Verify before writing. A bare index is meaningless if the dex shifted, and
                        // silently patching the wrong instruction is exactly the failure this whole
                        // approach cannot afford.
                        Instruction cur = mi.getInstructions().get(at);
                        String op = cur.getOpcode().name;
                        if (!op.startsWith("const")) {
                            System.out.println("REFUSE: " + s[0] + "." + s[1] + " @" + at
                                    + " is " + op + ", not a const");
                            System.exit(3);
                        }
                        int curReg = ((OneRegisterInstruction) cur).getRegisterA();
                        if (curReg != reg) {
                            System.out.println("REFUSE: " + s[0] + "." + s[1] + " @" + at
                                    + " writes v" + curReg + ", expected v" + reg);
                            System.exit(3);
                        }
                        long oldVal = ((WideLiteralInstruction) cur).getWideLiteral();
                        mi.replaceInstruction(at, new BuilderInstruction21s(Opcode.CONST_16, reg, val));
                        out = new ImmutableMethod(m.getDefiningClass(), m.getName(), m.getParameters(),
                                m.getReturnType(), m.getAccessFlags(), m.getAnnotations(),
                                m.getHiddenApiRestrictions(), mi);
                        patched++; touched = true;
                        System.out.println("  " + s[0] + "." + s[1] + " @" + at
                                + ": " + op + " v" + reg + ", " + oldVal + "  ->  const/16 v" + reg + ", " + val);
                    }
                    (pass == 0 ? direct : virt).add(out);
                }
            }
            if (touched) pool.internClass(new ImmutableClassDef(c.getType(), c.getAccessFlags(),
                    c.getSuperclass(), c.getInterfaces(), c.getSourceFile(), c.getAnnotations(),
                    c.getStaticFields(), c.getInstanceFields(), direct, virt));
            else pool.internClass(c);
        }
        System.out.println("patched=" + patched + " of " + sites.size());
        if (patched < sites.size()) { System.out.println("FAIL"); System.exit(2); }
        pool.writeTo(new FileDataStore(new File(a[1])));
    }
}
