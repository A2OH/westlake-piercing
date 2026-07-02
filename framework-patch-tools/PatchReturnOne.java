import com.android.tools.smali.dexlib2.Opcode;
import com.android.tools.smali.dexlib2.Opcodes;
import com.android.tools.smali.dexlib2.DexFileFactory;
import com.android.tools.smali.dexlib2.dexbacked.DexBackedDexFile;
import com.android.tools.smali.dexlib2.iface.DexFile;
import com.android.tools.smali.dexlib2.iface.Method;
import com.android.tools.smali.dexlib2.immutable.ImmutableMethod;
import com.android.tools.smali.dexlib2.immutable.ImmutableMethodImplementation;
import com.android.tools.smali.dexlib2.immutable.instruction.*;
import com.android.tools.smali.dexlib2.rewriter.*;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

// args: in out  Lclass;|method ...
// Replaces EVERY overload of each named method with: const/4 v0,1; return v0
// (i.e. return the int 1). Used to force AudioManager.requestAudioFocus to
// return AUDIOFOCUS_REQUEST_GRANTED so focus-gated apps proceed.
public class PatchReturnOne {
    static int patched = 0;
    static Set<String> targets = new HashSet<>(); // class|method

    public static void main(String[] args) throws Exception {
        String in = args[0], out = args[1];
        for (int i = 2; i < args.length; i++) targets.add(args[i]);
        DexBackedDexFile dex = DexFileFactory.loadDexFile(in, Opcodes.forApi(34));
        DexRewriter rw = new DexRewriter(new RewriterModule() {
            @Override public Rewriter<Method> getMethodRewriter(Rewriters rewriters) {
                return new MethodRewriter(rewriters) {
                    @Override public Method rewrite(Method m) {
                        String key = m.getDefiningClass() + "|" + m.getName();
                        if (targets.contains(key) && m.getImplementation() != null
                                && m.getReturnType().equals("I")) {
                            int ins = (((m.getAccessFlags() & 0x8) != 0) ? 0 : 1);
                            for (CharSequence pt : m.getParameterTypes())
                                ins += (pt.equals("J") || pt.equals("D")) ? 2 : 1;
                            System.out.println("[returnone] " + key + " " + m.getParameterTypes()
                                    + " -> return 1 (regs=" + (ins + 1) + ")");
                            patched++;
                            return new ImmutableMethod(m.getDefiningClass(), m.getName(),
                                    m.getParameters(), m.getReturnType(), m.getAccessFlags(),
                                    m.getAnnotations(), m.getHiddenApiRestrictions(), impl(ins + 1));
                        }
                        return super.rewrite(m);
                    }
                };
            }
        });
        DexFile pd = rw.getDexFileRewriter().rewrite(dex);
        DexPool.writeTo(out, pd);
        System.out.println("[returnone] wrote " + out + " patched=" + patched);
        if (patched < 1) throw new RuntimeException("no methods patched");
    }

    static ImmutableMethodImplementation impl(int regs) {
        List<ImmutableInstruction> ins = new ArrayList<>();
        ins.add(new ImmutableInstruction11n(Opcode.CONST_4, 0, 1)); // const/4 v0, 1
        ins.add(new ImmutableInstruction11x(Opcode.RETURN, 0));      // return v0
        return new ImmutableMethodImplementation(regs, ins, null, null);
    }
}
