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
import com.android.tools.smali.dexlib2.rewriter.DexRewriter;
import com.android.tools.smali.dexlib2.rewriter.MethodRewriter;
import com.android.tools.smali.dexlib2.rewriter.Rewriter;
import com.android.tools.smali.dexlib2.rewriter.RewriterModule;
import com.android.tools.smali.dexlib2.rewriter.Rewriters;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

// Replace the body of named (class,method) pairs with: return new ArrayList();
// Targets passed as args[2..] in the form  Lclass;|methodName
public class PatchEmptyList {
    static int patched = 0;
    static java.util.Set<String> targets = new java.util.HashSet<>();

    public static void main(String[] args) throws Exception {
        String in = args[0], out = args[1];
        for (int i = 2; i < args.length; i++) targets.add(args[i]);
        DexBackedDexFile dex = DexFileFactory.loadDexFile(in, Opcodes.forApi(34));
        DexRewriter rw = new DexRewriter(new RewriterModule() {
            @Override public Rewriter<Method> getMethodRewriter(Rewriters rewriters) {
                return new MethodRewriter(rewriters) {
                    @Override public Method rewrite(Method m) {
                        String key = m.getDefiningClass() + "|" + m.getName();
                        if (targets.contains(key) && m.getImplementation() != null) {
                            System.out.println("[emptylist] " + key);
                            patched++;
                            return new ImmutableMethod(m.getDefiningClass(), m.getName(),
                                    m.getParameters(), m.getReturnType(), m.getAccessFlags(),
                                    m.getAnnotations(), m.getHiddenApiRestrictions(), emptyListImpl());
                        }
                        return super.rewrite(m);
                    }
                };
            }
        });
        DexFile pd = rw.getDexFileRewriter().rewrite(dex);
        DexPool.writeTo(out, pd);
        System.out.println("[emptylist] wrote " + out + " patched=" + patched);
        if (patched != targets.size())
            throw new RuntimeException("expected " + targets.size() + " patched, got " + patched);
    }

    static ImmutableMethodImplementation emptyListImpl() {
        ImmutableTypeReference AL = new ImmutableTypeReference("Ljava/util/ArrayList;");
        ImmutableMethodReference alInit = new ImmutableMethodReference(
                "Ljava/util/ArrayList;", "<init>", Collections.<CharSequence>emptyList(), "V");
        List<ImmutableInstruction> ins = new ArrayList<>();
        ins.add(new ImmutableInstruction21c(Opcode.NEW_INSTANCE, 0, AL));
        ins.add(new ImmutableInstruction35c(Opcode.INVOKE_DIRECT, 1, 0,0,0,0,0, alInit));
        ins.add(new ImmutableInstruction11x(Opcode.RETURN_OBJECT, 0));
        return new ImmutableMethodImplementation(1, ins, null, null);
    }
}
