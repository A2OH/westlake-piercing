// PatchReturnNew <in.dex> <out.dex> <Lclass;> <method> <Lreturntype;>
//
// Replace a method body with `return new <ReturnType>()` (requires a public no-arg constructor).
//
// §513: this exists because PatchNoOp is the wrong tool for a method that must hand back an OBJECT.
// PatchNoOp emits a type-default return, i.e. null — which for
// DisplayManagerGlobal.getWifiDisplayStatus() is exactly the current broken behaviour, not a fix.
//
// Why it matters here: this board has no wifi-display support, so the display service returns null,
// and MediaRouter.updateWifiDisplayStatus() then NPEs on status.getFeatureState() on every route
// scan. That fired 23 times and libart's throw probe caps at 40 TOTAL, so the exception actually
// blocking audio never gets recorded — the diagnostic channel is consumed by noise.
//
// ⚠️This is deliberately NOT the §478 approach, which no-op'd MediaRouter's WifiDisplay methods and
// was reverted as over-reach. Returning a default-constructed WifiDisplayStatus is *more* correct
// than returning null: AOSP's no-arg constructor yields FEATURE_STATE_UNAVAILABLE /
// SCAN_STATE_NOT_SCANNING / DISPLAY_STATE_NOT_CONNECTED, which is precisely the truth on a board
// with no wifi display. MediaRouter then takes its "no wifi display" path instead of dereferencing
// null. We supply a missing platform object rather than disabling a platform feature — the same
// shape as §476 (power/thermalservice) and §479 (media_router).
import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.immutable.*;
import com.android.tools.smali.dexlib2.immutable.instruction.*;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableMethodReference;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableTypeReference;
import com.android.tools.smali.dexlib2.writer.io.FileDataStore;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;
import java.io.File;
import java.util.*;

public class PatchReturnNew {
    public static void main(String[] a) throws Exception {
        if (a.length != 5) {
            System.out.println("usage: PatchReturnNew <in.dex> <out.dex> <Lclass;> <method> <Lreturntype;>");
            System.exit(1);
        }
        String target = a[2], mName = a[3], retType = a[4];
        DexFile in = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
        DexPool pool = new DexPool(Opcodes.forApi(34));

        // Verify the return type really has a public no-arg <init> before rewriting anything —
        // emitting a call to a constructor that does not exist would swap a null-deref for a much
        // more confusing NoSuchMethodError at runtime.
        boolean ctorOk = false;
        for (ClassDef c : in.getClasses()) {
            if (!c.getType().equals(retType)) continue;
            for (Method m : c.getDirectMethods()) {
                if (m.getName().equals("<init>") && m.getParameterTypes().isEmpty()) { ctorOk = true; break; }
            }
        }
        if (!ctorOk) {
            System.out.println("FAIL: " + retType + " has no no-arg <init> in this dex");
            System.exit(2);
        }

        int patched = 0;
        for (ClassDef c : in.getClasses()) {
            if (!c.getType().equals(target)) { pool.internClass(c); continue; }
            List<Method> direct = new ArrayList<>(), virt = new ArrayList<>();
            for (int pass = 0; pass < 2; pass++) {
                for (Method m : (pass == 0 ? c.getDirectMethods() : c.getVirtualMethods())) {
                    Method out = m;
                    if (m.getName().equals(mName) && m.getReturnType().equals(retType)
                            && m.getImplementation() != null) {
                        int paramRegs = ((m.getAccessFlags() & AccessFlags.STATIC.getValue()) != 0) ? 0 : 1;
                        for (CharSequence p : m.getParameterTypes()) {
                            String t = p.toString();
                            paramRegs += (t.equals("J") || t.equals("D")) ? 2 : 1;
                        }
                        List<com.android.tools.smali.dexlib2.iface.instruction.Instruction> body = new ArrayList<>();
                        body.add(new ImmutableInstruction21c(Opcode.NEW_INSTANCE, 0,
                                new ImmutableTypeReference(retType)));
                        body.add(new ImmutableInstruction35c(Opcode.INVOKE_DIRECT, 1, 0, 0, 0, 0, 0,
                                new ImmutableMethodReference(retType, "<init>",
                                        Collections.<CharSequence>emptyList(), "V")));
                        body.add(new ImmutableInstruction11x(Opcode.RETURN_OBJECT, 0));
                        out = new ImmutableMethod(m.getDefiningClass(), m.getName(), m.getParameters(),
                                m.getReturnType(), m.getAccessFlags(), m.getAnnotations(),
                                m.getHiddenApiRestrictions(),
                                new ImmutableMethodImplementation(1 + paramRegs, body, null, null));
                        patched++;
                        System.out.println("  " + m.getName() + " -> return new " + retType);
                    }
                    (pass == 0 ? direct : virt).add(out);
                }
            }
            pool.internClass(new ImmutableClassDef(c.getType(), c.getAccessFlags(), c.getSuperclass(),
                    c.getInterfaces(), c.getSourceFile(), c.getAnnotations(), c.getStaticFields(),
                    c.getInstanceFields(), direct, virt));
        }
        System.out.println("patched=" + patched);
        if (patched == 0) { System.out.println("FAIL: nothing matched " + target + " " + mName); System.exit(2); }
        pool.writeTo(new FileDataStore(new File(a[1])));
    }
}
