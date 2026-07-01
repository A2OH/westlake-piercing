import com.android.tools.smali.dexlib2.Opcode;
import com.android.tools.smali.dexlib2.Opcodes;
import com.android.tools.smali.dexlib2.DexFileFactory;
import com.android.tools.smali.dexlib2.dexbacked.DexBackedDexFile;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.iface.instruction.*;
import com.android.tools.smali.dexlib2.iface.reference.MethodReference;
import com.android.tools.smali.dexlib2.immutable.ImmutableMethod;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableMethodReference;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableTypeReference;
import com.android.tools.smali.dexlib2.builder.*;
import com.android.tools.smali.dexlib2.builder.instruction.*;
import com.android.tools.smali.dexlib2.rewriter.*;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;

import java.util.*;

// For each target "Lclass;|method|LStub;|Liface;": find the invoke-static to <Stub>.asInterface
// inside class.method, and after its move-result, if the result is null, rebuild it as
// <Stub>.asInterface(new Binder()) so the service ref is never null.
public class PatchProxy {
    static int patched = 0;
    static List<String[]> targets = new ArrayList<>();

    public static void main(String[] a) throws Exception {
        String in = a[0], out = a[1];
        for (int i = 2; i < a.length; i++) targets.add(a[i].split("\\|"));
        DexBackedDexFile dex = DexFileFactory.loadDexFile(in, Opcodes.forApi(34));
        DexRewriter rw = new DexRewriter(new RewriterModule() {
            @Override public Rewriter<Method> getMethodRewriter(Rewriters rs) {
                return new MethodRewriter(rs) {
                    @Override public Method rewrite(Method m) {
                        for (String[] t : targets) {
                            if (m.getDefiningClass().equals(t[0]) && m.getName().equals(t[1])
                                    && m.getImplementation() != null) {
                                MutableMethodImplementation impl = patch(m.getImplementation(), t[2], t[3]);
                                if (impl != null)
                                    return new ImmutableMethod(m.getDefiningClass(), m.getName(),
                                            m.getParameters(), m.getReturnType(), m.getAccessFlags(),
                                            m.getAnnotations(), m.getHiddenApiRestrictions(), impl);
                            }
                        }
                        return super.rewrite(m);
                    }
                };
            }
        });
        DexFile pd = rw.getDexFileRewriter().rewrite(dex);
        DexPool.writeTo(out, pd);
        System.out.println("[proxy] wrote " + out + " patched=" + patched);
        if (patched != targets.size()) throw new RuntimeException("expected " + targets.size() + " got " + patched);
    }

    static MutableMethodImplementation patch(MethodImplementation orig, String stub, String iface) {
        MutableMethodImplementation impl = new MutableMethodImplementation(orig);
        int ai = -1;
        for (int i = 0; i < impl.getInstructions().size(); i++) {
            Instruction ins = impl.getInstructions().get(i);
            if (ins instanceof ReferenceInstruction) {
                Object r = ((ReferenceInstruction) ins).getReference();
                if (r instanceof MethodReference) {
                    MethodReference mr = (MethodReference) r;
                    if (mr.getName().equals("asInterface") && mr.getDefiningClass().equals(stub)) { ai = i; break; }
                }
            }
        }
        if (ai < 0 || ai + 1 >= impl.getInstructions().size()) return null;
        Instruction mr = impl.getInstructions().get(ai + 1);
        if (mr.getOpcode() != Opcode.MOVE_RESULT_OBJECT) return null;
        int reg = ((OneRegisterInstruction) mr).getRegisterA();
        if (reg > 15) return null; // need 4-bit reg for 35c
        int at = ai + 2;
        Label skip = impl.newLabelForIndex(at);
        ImmutableTypeReference BINDER = new ImmutableTypeReference("Landroid/os/Binder;");
        ImmutableMethodReference binderInit = new ImmutableMethodReference("Landroid/os/Binder;", "<init>", Collections.<CharSequence>emptyList(), "V");
        ImmutableMethodReference asIf = new ImmutableMethodReference(stub, "asInterface", Collections.<CharSequence>singletonList("Landroid/os/IBinder;"), iface);
        impl.addInstruction(at,   new BuilderInstruction21t(Opcode.IF_NEZ, reg, skip));
        impl.addInstruction(at+1, new BuilderInstruction21c(Opcode.NEW_INSTANCE, reg, BINDER));
        impl.addInstruction(at+2, new BuilderInstruction35c(Opcode.INVOKE_DIRECT, 1, reg,0,0,0,0, binderInit));
        impl.addInstruction(at+3, new BuilderInstruction35c(Opcode.INVOKE_STATIC, 1, reg,0,0,0,0, asIf));
        impl.addInstruction(at+4, new BuilderInstruction11x(Opcode.MOVE_RESULT_OBJECT, reg));
        patched++;
        System.out.println("[proxy] " + stub + " in target -> non-null proxy (reg v" + reg + ")");
        return impl;
    }
}
