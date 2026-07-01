import com.android.tools.smali.dexlib2.Opcode;
import com.android.tools.smali.dexlib2.Opcodes;
import com.android.tools.smali.dexlib2.DexFileFactory;
import com.android.tools.smali.dexlib2.dexbacked.DexBackedDexFile;
import com.android.tools.smali.dexlib2.iface.DexFile;
import com.android.tools.smali.dexlib2.iface.Method;
import com.android.tools.smali.dexlib2.iface.MethodImplementation;
import com.android.tools.smali.dexlib2.iface.instruction.Instruction;
import com.android.tools.smali.dexlib2.iface.instruction.ReferenceInstruction;
import com.android.tools.smali.dexlib2.iface.instruction.formats.Instruction35c;
import com.android.tools.smali.dexlib2.iface.reference.MethodReference;
import com.android.tools.smali.dexlib2.immutable.ImmutableMethod;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableMethodReference;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableFieldReference;
import com.android.tools.smali.dexlib2.immutable.reference.ImmutableTypeReference;
import com.android.tools.smali.dexlib2.builder.MutableMethodImplementation;
import com.android.tools.smali.dexlib2.builder.Label;
import com.android.tools.smali.dexlib2.builder.BuilderInstruction;
import com.android.tools.smali.dexlib2.builder.instruction.*;
import com.android.tools.smali.dexlib2.rewriter.DexRewriter;
import com.android.tools.smali.dexlib2.rewriter.MethodRewriter;
import com.android.tools.smali.dexlib2.rewriter.Rewriter;
import com.android.tools.smali.dexlib2.rewriter.RewriterModule;
import com.android.tools.smali.dexlib2.rewriter.Rewriters;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;

import java.util.Arrays;
import java.util.Collections;

public class PatchMS {
    static int patched = 0;
    static final String MS = "Landroid/media/session/MediaSession;";

    public static void main(String[] args) throws Exception {
        String in = args[0], out = args[1];
        DexBackedDexFile dex = DexFileFactory.loadDexFile(in, Opcodes.forApi(34));
        DexRewriter rewriter = new DexRewriter(new RewriterModule() {
            @Override public Rewriter<Method> getMethodRewriter(Rewriters rewriters) {
                return new MethodRewriter(rewriters) {
                    @Override public Method rewrite(Method m) {
                        if (m.getDefiningClass().equals(MS) && m.getName().equals("<init>")
                                && m.getImplementation() != null && hasCreateSession(m.getImplementation())) {
                            MutableMethodImplementation impl = patch(m.getImplementation());
                            if (impl != null)
                                return new ImmutableMethod(m.getDefiningClass(), m.getName(),
                                        m.getParameters(), m.getReturnType(), m.getAccessFlags(),
                                        m.getAnnotations(), m.getHiddenApiRestrictions(), impl);
                        }
                        return super.rewrite(m);
                    }
                };
            }
        });
        DexFile pd = rewriter.getDexFileRewriter().rewrite(dex);
        DexPool.writeTo(out, pd);
        System.out.println("[patchMS] wrote " + out + " patched=" + patched);
        if (patched == 0) throw new RuntimeException("MediaSession.<init> not patched!");
    }

    static boolean isCreateSession(Instruction ins) {
        if (!(ins instanceof ReferenceInstruction)) return false;
        Object r = ((ReferenceInstruction) ins).getReference();
        return (r instanceof MethodReference) && ((MethodReference) r).getName().equals("createSession");
    }
    static boolean hasCreateSession(MethodImplementation impl) {
        for (Instruction i : impl.getInstructions()) if (isCreateSession(i)) return true;
        return false;
    }

    static MutableMethodImplementation patch(MethodImplementation orig) {
        MutableMethodImplementation impl = new MutableMethodImplementation(orig);
        int ci = -1, mgr = -1;
        for (int i = 0; i < impl.getInstructions().size(); i++) {
            Instruction ins = impl.getInstructions().get(i);
            if (isCreateSession(ins)) { ci = i; mgr = ((Instruction35c) ins).getRegisterC(); break; }
        }
        if (ci < 0) return null;

        ImmutableTypeReference BINDER = new ImmutableTypeReference("Landroid/os/Binder;");
        ImmutableTypeReference TOKEN = new ImmutableTypeReference("Landroid/media/session/MediaSession$Token;");
        ImmutableMethodReference binderInit = new ImmutableMethodReference("Landroid/os/Binder;", "<init>", Collections.<CharSequence>emptyList(), "V");
        ImmutableMethodReference iSessAs = new ImmutableMethodReference("Landroid/media/session/ISession$Stub;", "asInterface", Collections.<CharSequence>singletonList("Landroid/os/IBinder;"), "Landroid/media/session/ISession;");
        ImmutableMethodReference iCtrlAs = new ImmutableMethodReference("Landroid/media/session/ISessionController$Stub;", "asInterface", Collections.<CharSequence>singletonList("Landroid/os/IBinder;"), "Landroid/media/session/ISessionController;");
        ImmutableMethodReference myUid = new ImmutableMethodReference("Landroid/os/Process;", "myUid", Collections.<CharSequence>emptyList(), "I");
        ImmutableMethodReference tokenInit = new ImmutableMethodReference("Landroid/media/session/MediaSession$Token;", "<init>", Arrays.<CharSequence>asList("I", "Landroid/media/session/ISessionController;"), "V");
        ImmutableFieldReference fBinder = new ImmutableFieldReference(MS, "mBinder", "Landroid/media/session/ISession;");
        ImmutableFieldReference fToken = new ImmutableFieldReference(MS, "mSessionToken", "Landroid/media/session/MediaSession$Token;");
        ImmutableFieldReference fCtrl = new ImmutableFieldReference(MS, "mController", "Landroid/media/session/MediaController;");

        Label real = impl.newLabelForIndex(ci); // createSession = normal path
        int at = ci;
        // this register: .registers 8, ins=4 -> this = 8-4 = 4
        int THIS = orig.getRegisterCount() - 4;
        BuilderInstruction[] seq = new BuilderInstruction[] {
            new BuilderInstruction21t(Opcode.IF_NEZ, mgr, real),
            new BuilderInstruction21c(Opcode.NEW_INSTANCE, 0, BINDER),
            new BuilderInstruction35c(Opcode.INVOKE_DIRECT, 1, 0,0,0,0,0, binderInit),
            new BuilderInstruction35c(Opcode.INVOKE_STATIC, 1, 0,0,0,0,0, iSessAs),
            new BuilderInstruction11x(Opcode.MOVE_RESULT_OBJECT, 0),
            new BuilderInstruction22c(Opcode.IPUT_OBJECT, 0, THIS, fBinder),
            new BuilderInstruction21c(Opcode.NEW_INSTANCE, 1, BINDER),
            new BuilderInstruction35c(Opcode.INVOKE_DIRECT, 1, 1,0,0,0,0, binderInit),
            new BuilderInstruction35c(Opcode.INVOKE_STATIC, 1, 1,0,0,0,0, iCtrlAs),
            new BuilderInstruction11x(Opcode.MOVE_RESULT_OBJECT, 1),
            new BuilderInstruction21c(Opcode.NEW_INSTANCE, 2, TOKEN),
            new BuilderInstruction35c(Opcode.INVOKE_STATIC, 0, 0,0,0,0,0, myUid),
            new BuilderInstruction11x(Opcode.MOVE_RESULT, 3),
            new BuilderInstruction35c(Opcode.INVOKE_DIRECT, 3, 2,3,1,0,0, tokenInit),
            new BuilderInstruction22c(Opcode.IPUT_OBJECT, 2, THIS, fToken),
            new BuilderInstruction11n(Opcode.CONST_4, 0, 0),
            new BuilderInstruction22c(Opcode.IPUT_OBJECT, 0, THIS, fCtrl),
            new BuilderInstruction10x(Opcode.RETURN_VOID),
        };
        for (int k = 0; k < seq.length; k++) impl.addInstruction(at + k, seq[k]);
        patched++;
        System.out.println("[patchMS] inserted null-manager branch (mgr=v" + mgr + " this=v" + THIS + ", " + seq.length + " ins)");
        return impl;
    }
}
