import com.android.tools.smali.dexlib2.Opcode;
import com.android.tools.smali.dexlib2.Opcodes;
import com.android.tools.smali.dexlib2.DexFileFactory;
import com.android.tools.smali.dexlib2.dexbacked.DexBackedDexFile;
import com.android.tools.smali.dexlib2.iface.DexFile;
import com.android.tools.smali.dexlib2.iface.Method;
import com.android.tools.smali.dexlib2.iface.MethodImplementation;
import com.android.tools.smali.dexlib2.iface.instruction.Instruction;
import com.android.tools.smali.dexlib2.iface.instruction.ReferenceInstruction;
import com.android.tools.smali.dexlib2.iface.instruction.OneRegisterInstruction;
import com.android.tools.smali.dexlib2.iface.reference.MethodReference;
import com.android.tools.smali.dexlib2.immutable.ImmutableMethod;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableMethodReference;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableTypeReference;
import com.android.tools.smali.dexlib2.builder.MutableMethodImplementation;
import com.android.tools.smali.dexlib2.builder.Label;
import com.android.tools.smali.dexlib2.builder.instruction.BuilderInstruction11x;
import com.android.tools.smali.dexlib2.builder.instruction.BuilderInstruction21c;
import com.android.tools.smali.dexlib2.builder.instruction.BuilderInstruction21t;
import com.android.tools.smali.dexlib2.builder.instruction.BuilderInstruction35c;
import com.android.tools.smali.dexlib2.rewriter.DexRewriter;
import com.android.tools.smali.dexlib2.rewriter.MethodRewriter;
import com.android.tools.smali.dexlib2.rewriter.Rewriter;
import com.android.tools.smali.dexlib2.rewriter.RewriterModule;
import com.android.tools.smali.dexlib2.rewriter.Rewriters;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;

import java.util.Collections;

public class PatchPI {
    static int patched = 0;

    public static void main(String[] args) throws Exception {
        String in = args[0], out = args[1];
        DexBackedDexFile dex = DexFileFactory.loadDexFile(in, Opcodes.forApi(34));

        DexRewriter rewriter = new DexRewriter(new RewriterModule() {
            @Override public Rewriter<Method> getMethodRewriter(Rewriters rewriters) {
                return new MethodRewriter(rewriters) {
                    @Override public Method rewrite(Method m) {
                        if (m.getDefiningClass().equals("Landroid/app/PendingIntent;")
                                && m.getImplementation() != null
                                && callsGetIntentSender(m.getImplementation())) {
                            MutableMethodImplementation impl =
                                    patchImpl(m.getName(), m.getImplementation());
                            if (impl != null) {
                                return new ImmutableMethod(
                                        m.getDefiningClass(), m.getName(), m.getParameters(),
                                        m.getReturnType(), m.getAccessFlags(),
                                        m.getAnnotations(), m.getHiddenApiRestrictions(),
                                        impl);
                            }
                        }
                        return super.rewrite(m);
                    }
                };
            }
        });

        DexFile patchedDex = rewriter.getDexFileRewriter().rewrite(dex);
        DexPool.writeTo(out, patchedDex);
        System.out.println("[patch] wrote " + out + "  methods patched=" + patched);
        if (patched == 0) throw new RuntimeException("NO methods patched!");
    }

    static boolean isGIS(Instruction ins) {
        if (!(ins instanceof ReferenceInstruction)) return false;
        Object r = ((ReferenceInstruction) ins).getReference();
        if (!(r instanceof MethodReference)) return false;
        MethodReference mr = (MethodReference) r;
        return mr.getName().equals("getIntentSenderWithFeature")
                || mr.getName().equals("getIntentSender");
    }

    static boolean callsGetIntentSender(MethodImplementation impl) {
        for (Instruction ins : impl.getInstructions()) if (isGIS(ins)) return true;
        return false;
    }

    static MutableMethodImplementation patchImpl(String name, MethodImplementation orig) {
        MutableMethodImplementation impl = new MutableMethodImplementation(orig);
        java.util.List<com.android.tools.smali.dexlib2.builder.BuilderInstruction> list =
                impl.getInstructions();
        // find the getIntentSender* invoke; the immediately-following move-result-object
        // holds the target register.
        int gi = -1;
        for (int i = 0; i < list.size(); i++) {
            if (isGIS(list.get(i))) { gi = i; break; }
        }
        if (gi < 0 || gi + 1 >= list.size()) return null;
        Instruction mr = list.get(gi + 1);
        if (mr.getOpcode() != Opcode.MOVE_RESULT_OBJECT) {
            System.out.println("[patch] " + name + ": no move-result-object after GIS, skip");
            return null;
        }
        int reg = ((OneRegisterInstruction) mr).getRegisterA();
        int insertAt = gi + 2;

        // label at the original instruction following move-result (the skip target)
        Label skip = impl.newLabelForIndex(insertAt);

        ImmutableTypeReference binderType = new ImmutableTypeReference("Landroid/os/Binder;");
        ImmutableMethodReference binderInit = new ImmutableMethodReference(
                "Landroid/os/Binder;", "<init>",
                Collections.<CharSequence>emptyList(), "V");
        ImmutableMethodReference asInterface = new ImmutableMethodReference(
                "Landroid/content/IIntentSender$Stub;", "asInterface",
                Collections.<CharSequence>singletonList("Landroid/os/IBinder;"),
                "Landroid/content/IIntentSender;");

        // if-nez reg, :skip
        impl.addInstruction(insertAt,     new BuilderInstruction21t(Opcode.IF_NEZ, reg, skip));
        // new-instance reg, Binder
        impl.addInstruction(insertAt + 1, new BuilderInstruction21c(Opcode.NEW_INSTANCE, reg, binderType));
        // invoke-direct {reg}, Binder-><init>()V
        impl.addInstruction(insertAt + 2, new BuilderInstruction35c(Opcode.INVOKE_DIRECT, 1, reg, 0, 0, 0, 0, binderInit));
        // invoke-static {reg}, IIntentSender$Stub->asInterface(IBinder)IIntentSender
        impl.addInstruction(insertAt + 3, new BuilderInstruction35c(Opcode.INVOKE_STATIC, 1, reg, 0, 0, 0, 0, asInterface));
        // move-result-object reg
        impl.addInstruction(insertAt + 4, new BuilderInstruction11x(Opcode.MOVE_RESULT_OBJECT, reg));

        patched++;
        System.out.println("[patch] " + name + ": inserted proxy-substitution (target v" + reg + ")");
        return impl;
    }
}
