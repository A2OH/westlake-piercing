// RemoveInsn <in.dex> <out.dex> <Lclass;>:<method>:<index>:<expectedOpcodePrefix> [...]
//
// Delete a single instruction, verifying first that the instruction at that index really is the one
// meant (by opcode prefix). dexlib2's MutableMethodImplementation renumbers branch targets and
// try/catch ranges for us, so removal is safe in a way a raw byte edit would not be.
//
// §546: WindowSessionAdapter.relayout reverse-pushes IWindow.resized(...) on EVERY relayout. §545
// set its forceLayout flag to 0, which broke the loop for the MAIN window (idle 88% -> 0%), but a
// DIALOG kept looping at 75% CPU: ViewRootImpl.handleResized also proceeds when the frame or the
// configuration changed, and this push hands EVERY window a full-screen ClientWindowFrames and a
// freshly built, EMPTY MergedConfiguration. A bottom-sheet dialog is not full screen, so it can
// never agree with that frame — it re-requests its own size, the adapter overrides back to
// full-screen, and the two oscillate forever.
//
// The source's intent is a ONE-TIME bootstrap push (its `mReversePushed` guard), and this is the
// SYNCHRONOUS relayout path, which already returns the real geometry to the caller via outFrames
// ("outFrames populated" in the log) — so the push has nothing left to contribute here. The 0x0
// death spiral it was written for is in the ONEWAY relayoutAsync path, which is untouched.
import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.builder.MutableMethodImplementation;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.iface.instruction.Instruction;
import com.android.tools.smali.dexlib2.immutable.*;
import com.android.tools.smali.dexlib2.writer.io.FileDataStore;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;
import java.io.File;
import java.util.*;

public class RemoveInsn {
    public static void main(String[] a) throws Exception {
        if (a.length < 3) {
            System.out.println("usage: RemoveInsn <in> <out> <Lcls;>:<m>:<idx>:<opcodePrefix>");
            System.exit(1);
        }
        List<String[]> sites = new ArrayList<>();
        for (int i = 2; i < a.length; i++) {
            String[] p = a[i].split(":");
            if (p.length != 4) { System.out.println("bad site: " + a[i]); System.exit(1); }
            sites.add(p);
        }
        DexFile in = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
        DexPool pool = new DexPool(Opcodes.forApi(34));
        int done = 0;
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
                        MutableMethodImplementation mi =
                                new MutableMethodImplementation(m.getImplementation());
                        Instruction cur = mi.getInstructions().get(at);
                        String op = cur.getOpcode().name;
                        if (!op.toLowerCase().startsWith(s[3].toLowerCase())) {
                            System.out.println("REFUSE: " + s[0] + "." + s[1] + " @" + at
                                    + " is " + op + ", expected prefix " + s[3]);
                            System.exit(3);
                        }
                        mi.removeInstruction(at);
                        out = new ImmutableMethod(m.getDefiningClass(), m.getName(), m.getParameters(),
                                m.getReturnType(), m.getAccessFlags(), m.getAnnotations(),
                                m.getHiddenApiRestrictions(), mi);
                        done++; touched = true;
                        System.out.println("  removed " + op + " @" + at + " from " + s[0] + "." + s[1]);
                    }
                    (pass == 0 ? direct : virt).add(out);
                }
            }
            if (touched) pool.internClass(new ImmutableClassDef(c.getType(), c.getAccessFlags(),
                    c.getSuperclass(), c.getInterfaces(), c.getSourceFile(), c.getAnnotations(),
                    c.getStaticFields(), c.getInstanceFields(), direct, virt));
            else pool.internClass(c);
        }
        System.out.println("removed=" + done + " of " + sites.size());
        if (done < sites.size()) { System.out.println("FAIL"); System.exit(2); }
        pool.writeTo(new FileDataStore(new File(a[1])));
    }
}
