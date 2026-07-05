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
// Replaces EVERY void-returning overload of each named method with: return-void.
// Used to no-op ContentResolver.registerContentObserver/unregisterContentObserver:
// OHOS has no "content" service, getContentService() is null, and the register
// call NPEs on the android.bg thread (kills the app) the moment a text field
// focuses (IME/TextView settings observers). Same graceful-degradation family as
// the other Patch* tools.
public class PatchReturnVoid {
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
                                && m.getReturnType().equals("V")) {
                            int ins = (((m.getAccessFlags() & 0x8) != 0) ? 0 : 1);
                            for (CharSequence pt : m.getParameterTypes())
                                ins += (pt.equals("J") || pt.equals("D")) ? 2 : 1;
                            System.out.println("[returnvoid] " + key + " " + m.getParameterTypes()
                                    + " -> return-void (regs=" + ins + ")");
                            patched++;
                            return new ImmutableMethod(m.getDefiningClass(), m.getName(),
                                    m.getParameters(), m.getReturnType(), m.getAccessFlags(),
                                    m.getAnnotations(), m.getHiddenApiRestrictions(), impl(ins));
                        }
                        return super.rewrite(m);
                    }
                };
            }
        });
        DexFile pd = rw.getDexFileRewriter().rewrite(dex);
        DexPool.writeTo(out, pd);
        System.out.println("[returnvoid] wrote " + out + " patched=" + patched);
        if (patched < 1) throw new RuntimeException("no methods patched");
    }

    static ImmutableMethodImplementation impl(int regs) {
        List<ImmutableInstruction> ins = new ArrayList<>();
        ins.add(new ImmutableInstruction10x(Opcode.RETURN_VOID));
        return new ImmutableMethodImplementation(regs, ins, null, null);
    }
}
