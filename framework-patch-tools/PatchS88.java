import com.android.tools.smali.dexlib2.Opcode;
import com.android.tools.smali.dexlib2.Opcodes;
import com.android.tools.smali.dexlib2.DexFileFactory;
import com.android.tools.smali.dexlib2.dexbacked.DexBackedDexFile;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.immutable.ImmutableMethod;
import com.android.tools.smali.dexlib2.immutable.ImmutableMethodImplementation;
import com.android.tools.smali.dexlib2.immutable.instruction.*;
import com.android.tools.smali.dexlib2.immutable.reference.*;
import com.android.tools.smali.dexlib2.rewriter.*;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;
import java.util.*;

// Rewrite SystemServiceRegistry$88.createService(ContextImpl) to use ServiceManager.getService
// (no-throw) instead of getServiceOrThrow, so getSystemService("shortcut") returns a non-null
// ShortcutManager (with null mService) instead of null.
public class PatchS88 {
    static int patched = 0;
    static final String C = "Landroid/app/SystemServiceRegistry$88;";
    public static void main(String[] a) throws Exception {
        DexBackedDexFile dex = DexFileFactory.loadDexFile(a[0], Opcodes.forApi(34));
        DexRewriter rw = new DexRewriter(new RewriterModule() {
            @Override public Rewriter<Method> getMethodRewriter(Rewriters rs) {
                return new MethodRewriter(rs) {
                    @Override public Method rewrite(Method m) {
                        if (m.getDefiningClass().equals(C) && m.getName().equals("createService")
                                && m.getReturnType().equals("Landroid/content/pm/ShortcutManager;")) {
                            patched++;
                            return new ImmutableMethod(m.getDefiningClass(), m.getName(),
                                m.getParameters(), m.getReturnType(), m.getAccessFlags(),
                                m.getAnnotations(), m.getHiddenApiRestrictions(), body());
                        }
                        return super.rewrite(m);
                    }
                };
            }
        });
        DexPool.writeTo(a[1], rw.getDexFileRewriter().rewrite(dex));
        System.out.println("[s88] wrote " + a[1] + " patched=" + patched);
        if (patched != 1) throw new RuntimeException("expected 1 got " + patched);
    }
    static ImmutableMethodImplementation body() {
        ImmutableMethodReference getSvc = new ImmutableMethodReference(
            "Landroid/os/ServiceManager;", "getService",
            Collections.<CharSequence>singletonList("Ljava/lang/String;"), "Landroid/os/IBinder;");
        ImmutableMethodReference asIf = new ImmutableMethodReference(
            "Landroid/content/pm/IShortcutService$Stub;", "asInterface",
            Collections.<CharSequence>singletonList("Landroid/os/IBinder;"), "Landroid/content/pm/IShortcutService;");
        ImmutableMethodReference smInit = new ImmutableMethodReference(
            "Landroid/content/pm/ShortcutManager;", "<init>",
            Arrays.<CharSequence>asList("Landroid/content/Context;", "Landroid/content/pm/IShortcutService;"), "V");
        ImmutableTypeReference SM = new ImmutableTypeReference("Landroid/content/pm/ShortcutManager;");
        // .registers 5 : this=v3, p1(ctx)=v4 ; locals v0,v1,v2
        List<ImmutableInstruction> ins = new ArrayList<>();
        ins.add(new ImmutableInstruction21c(Opcode.CONST_STRING, 0, new ImmutableStringReference("shortcut")));
        ins.add(new ImmutableInstruction35c(Opcode.INVOKE_STATIC, 1, 0,0,0,0,0, getSvc));
        ins.add(new ImmutableInstruction11x(Opcode.MOVE_RESULT_OBJECT, 0));   // v0 = binder (may be null)
        ins.add(new ImmutableInstruction21c(Opcode.NEW_INSTANCE, 1, SM));      // v1 = new ShortcutManager
        ins.add(new ImmutableInstruction35c(Opcode.INVOKE_STATIC, 1, 0,0,0,0,0, asIf));
        ins.add(new ImmutableInstruction11x(Opcode.MOVE_RESULT_OBJECT, 2));   // v2 = service (may be null)
        ins.add(new ImmutableInstruction35c(Opcode.INVOKE_DIRECT, 3, 1,4,2,0,0, smInit)); // (v1, v4=ctx, v2)
        ins.add(new ImmutableInstruction11x(Opcode.RETURN_OBJECT, 1));
        return new ImmutableMethodImplementation(5, ins, null, null);
    }
}
