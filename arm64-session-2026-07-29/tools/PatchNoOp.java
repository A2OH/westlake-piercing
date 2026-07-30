// PatchNoOp <in.dex> <out.dex> <Lclass;> <method> [<method> ...]
//
// Replace a method's body with an immediate type-default return. Used to neutralise a framework
// method that only throws on this runtime and whose effect we do not need.
//
// §478: MediaRouter.updateWifiDisplayStatus() NPEs on every route scan, because the display service
// hands back a null WifiDisplayStatus here. It is harmless in itself — wifi-display routes are
// meaningless on this board — but it fired 23 times in two minutes and libart's throw probe caps at
// 40 TOTAL, so it crowded out the exception actually stopping playback. Silencing a genuinely inert
// throw is the cheapest way to get the diagnostic channel back.
//
// ⚠️Only use this where the method's work is provably unnecessary. It is a muzzle, not a fix.
import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.immutable.*;
import com.android.tools.smali.dexlib2.immutable.instruction.*;
import com.android.tools.smali.dexlib2.writer.io.FileDataStore;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;
import java.io.File;
import java.util.*;

public class PatchNoOp {
    static int patched = 0;

    static MethodImplementation stubBody(String ret, int paramRegs) {
        List<com.android.tools.smali.dexlib2.iface.instruction.Instruction> body = new ArrayList<>();
        int locals;
        if (ret.equals("V")) {
            locals = 0;
            body.add(new ImmutableInstruction10x(Opcode.RETURN_VOID));
        } else if (ret.equals("J") || ret.equals("D")) {
            locals = 2;
            body.add(new ImmutableInstruction21s(Opcode.CONST_WIDE_16, 0, 0));
            body.add(new ImmutableInstruction11x(Opcode.RETURN_WIDE, 0));
        } else if (ret.length() == 1) {
            locals = 1;
            body.add(new ImmutableInstruction11n(Opcode.CONST_4, 0, 0));
            body.add(new ImmutableInstruction11x(Opcode.RETURN, 0));
        } else {
            locals = 1;
            body.add(new ImmutableInstruction11n(Opcode.CONST_4, 0, 0));
            body.add(new ImmutableInstruction11x(Opcode.RETURN_OBJECT, 0));
        }
        return new ImmutableMethodImplementation(locals + paramRegs, body, null, null);
    }

    static int width(String t) { return (t.equals("J") || t.equals("D")) ? 2 : 1; }

    public static void main(String[] a) throws Exception {
        if (a.length < 4) {
            System.out.println("usage: PatchNoOp <in.dex> <out.dex> <Lclass;> <method> [...]");
            System.exit(1);
        }
        String target = a[2];
        Set<String> names = new HashSet<>(Arrays.asList(a).subList(3, a.length));
        DexFile in = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
        DexPool pool = new DexPool(Opcodes.forApi(34));

        for (ClassDef c : in.getClasses()) {
            if (!c.getType().equals(target)) { pool.internClass(c); continue; }
            List<Method> direct = new ArrayList<>(), virt = new ArrayList<>();
            for (int pass = 0; pass < 2; pass++) {
                for (Method m : (pass == 0 ? c.getDirectMethods() : c.getVirtualMethods())) {
                    Method out = m;
                    if (names.contains(m.getName()) && m.getImplementation() != null) {
                        int paramRegs = ((m.getAccessFlags() & AccessFlags.STATIC.getValue()) != 0) ? 0 : 1;
                        for (CharSequence p : m.getParameterTypes()) paramRegs += width(p.toString());
                        out = new ImmutableMethod(m.getDefiningClass(), m.getName(), m.getParameters(),
                                m.getReturnType(), m.getAccessFlags(), m.getAnnotations(),
                                m.getHiddenApiRestrictions(), stubBody(m.getReturnType(), paramRegs));
                        patched++;
                        System.out.println("  no-op " + m.getName() + m.getParameterTypes() + " -> " + m.getReturnType());
                    }
                    (pass == 0 ? direct : virt).add(out);
                }
            }
            pool.internClass(new ImmutableClassDef(c.getType(), c.getAccessFlags(), c.getSuperclass(),
                    c.getInterfaces(), c.getSourceFile(), c.getAnnotations(), c.getStaticFields(),
                    c.getInstanceFields(), direct, virt));
        }
        System.out.println("patched=" + patched);
        if (patched == 0) { System.out.println("FAIL: nothing matched " + target + " " + names); System.exit(2); }
        pool.writeTo(new FileDataStore(new File(a[1])));
    }
}
