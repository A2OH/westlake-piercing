import com.android.tools.smali.dexlib2.Opcode;
import com.android.tools.smali.dexlib2.Opcodes;
import com.android.tools.smali.dexlib2.DexFileFactory;
import com.android.tools.smali.dexlib2.dexbacked.DexBackedDexFile;
import com.android.tools.smali.dexlib2.iface.DexFile;
import com.android.tools.smali.dexlib2.iface.Method;
import com.android.tools.smali.dexlib2.immutable.ImmutableMethod;
import com.android.tools.smali.dexlib2.immutable.ImmutableMethodImplementation;
import com.android.tools.smali.dexlib2.immutable.instruction.*;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableMethodReference;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableTypeReference;
import com.android.tools.smali.dexlib2.rewriter.*;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

// args: in out  (Lclass;|method|Lnewtype;)...
// Replaces each named method body with: return new <newtype>();   (no-arg ctor)
public class PatchReturnNew {
    static int patched = 0;
    static Map<String,String> targets = new HashMap<>(); // class|method -> Lnewtype;

    public static void main(String[] args) throws Exception {
        String in = args[0], out = args[1];
        for (int i = 2; i < args.length; i++) {
            String[] p = args[i].split("\\|");
            targets.put(p[0] + "|" + p[1], p[2]);
        }
        DexBackedDexFile dex = DexFileFactory.loadDexFile(in, Opcodes.forApi(34));
        DexRewriter rw = new DexRewriter(new RewriterModule() {
            @Override public Rewriter<Method> getMethodRewriter(Rewriters rewriters) {
                return new MethodRewriter(rewriters) {
                    @Override public Method rewrite(Method m) {
                        String key = m.getDefiningClass() + "|" + m.getName();
                        String nt = targets.get(key);
                        if (nt != null && m.getImplementation() != null) {
                            int ins = (((m.getAccessFlags() & 0x8) != 0) ? 0 : 1);
                            for (CharSequence pt : m.getParameterTypes())
                                ins += (pt.equals("J") || pt.equals("D")) ? 2 : 1;
                            System.out.println("[returnnew] " + key + " -> new " + nt + " (regs=" + (ins+1) + ")");
                            patched++;
                            return new ImmutableMethod(m.getDefiningClass(), m.getName(),
                                    m.getParameters(), m.getReturnType(), m.getAccessFlags(),
                                    m.getAnnotations(), m.getHiddenApiRestrictions(), impl(nt, ins + 1));
                        }
                        return super.rewrite(m);
                    }
                };
            }
        });
        DexFile pd = rw.getDexFileRewriter().rewrite(dex);
        DexPool.writeTo(out, pd);
        System.out.println("[returnnew] wrote " + out + " patched=" + patched);
        if (patched != targets.size())
            throw new RuntimeException("expected " + targets.size() + " got " + patched);
    }

    static ImmutableMethodImplementation impl(String type, int regs) {
        ImmutableTypeReference T = new ImmutableTypeReference(type);
        ImmutableMethodReference init = new ImmutableMethodReference(
                type, "<init>", Collections.<CharSequence>emptyList(), "V");
        List<ImmutableInstruction> ins = new ArrayList<>();
        ins.add(new ImmutableInstruction21c(Opcode.NEW_INSTANCE, 0, T));
        ins.add(new ImmutableInstruction35c(Opcode.INVOKE_DIRECT, 1, 0,0,0,0,0, init));
        ins.add(new ImmutableInstruction11x(Opcode.RETURN_OBJECT, 0));
        return new ImmutableMethodImplementation(regs, ins, null, null);
    }
}
