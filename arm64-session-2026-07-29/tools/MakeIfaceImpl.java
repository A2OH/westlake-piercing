// MakeIfaceImpl <in.dex> <out.dex> <Liface;> [<Liface;> ...]
//
// Emit a CONCRETE class implementing each named interface, with type-default method bodies, straight
// to dex. Generated classes are named Lwestlake/impl/<SimpleName>Impl;.
//
// Why this exists (§473): westlake's libart mishandles java.lang.reflect.Proxy receivers -- see §436.
// FindMethodToCall re-validates a resolved method with GetNameView()/GetSignature(), which AOSP
// forbids on proxy ArtMethods (DCHECK(!IsProxyMethod()), compiled out by -DNDEBUG), so a proxy's dex
// index is read against the WRONG dex and a bogus signature mismatch is manufactured:
//     NoSuchMethodError: No InvokeType(4) method createSession(...) in class ISessionManager
// Every dynamic-Proxy service stub is therefore a landmine that goes off the first time real
// framework code invokes an interface method on it. §440 and §464 defused individual call sites with
// dex surgery, but the media-session path has many (ISessionManager.createSession, then
// ISession.getController, then the whole MediaController/ISessionController chain).
//
// The defect only applies to Proxy receivers, so the fix is to stop being a Proxy: a normal class
// implementing the same interface dispatches through the ordinary path and is never revalidated.
//
// Bodies return type defaults. If a method's return type is another interface being generated in the
// same run, it returns a fresh instance of THAT impl instead of null -- callers dereference these
// immediately (MediaSession.<init> calls ISession.getController() and uses the result).
//
// Generated classes extend android.os.Binder so asBinder() can return `this` and the object is a
// usable IBinder.
import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.immutable.*;
import com.android.tools.smali.dexlib2.immutable.instruction.*;
import com.android.tools.smali.dexlib2.immutable.reference.*;
import com.android.tools.smali.dexlib2.writer.io.FileDataStore;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;
import java.io.File;
import java.util.*;

public class MakeIfaceImpl {
    static final String BINDER = "Landroid/os/Binder;";
    static final String IBINDER = "Landroid/os/IBinder;";

    static String implNameFor(String iface) {
        String s = iface.substring(1, iface.length() - 1);      // strip L ;
        int i = s.lastIndexOf('/');
        return "Lwestlake/impl/" + s.substring(i + 1) + "Impl;";
    }

    /** Register width of a type: long/double take two. */
    static int width(String t) { return (t.equals("J") || t.equals("D")) ? 2 : 1; }

    public static void main(String[] a) throws Exception {
        if (a.length < 3) {
            System.out.println("usage: MakeIfaceImpl <in.dex> <out.dex> <Liface;> [...]");
            System.exit(1);
        }
        DexFile in = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
        List<String> wanted = new ArrayList<>(Arrays.asList(a).subList(2, a.length));
        Map<String, ClassDef> found = new LinkedHashMap<>();
        for (ClassDef c : in.getClasses()) if (wanted.contains(c.getType())) found.put(c.getType(), c);
        for (String w : wanted) if (!found.containsKey(w)) {
            System.out.println("FAIL: interface not in dex: " + w);
            System.exit(2);
        }
        // Interface -> generated impl, so a method returning one of them can hand back a real object.
        Map<String, String> implOf = new LinkedHashMap<>();
        for (String w : wanted) implOf.put(w, implNameFor(w));

        DexPool pool = new DexPool(Opcodes.forApi(34));
        int total = 0;
        for (String iface : wanted) {
            ClassDef def = found.get(iface);
            String implType = implOf.get(iface);
            List<Method> methods = new ArrayList<>();
            List<Method> direct = new ArrayList<>();

            // <init>()V : super Binder(). MUST be a direct method.
            direct.add(new ImmutableMethod(implType, "<init>", null, "V",
                    AccessFlags.PUBLIC.getValue() | AccessFlags.CONSTRUCTOR.getValue(), null, null,
                    new ImmutableMethodImplementation(1, Arrays.<com.android.tools.smali.dexlib2.iface.instruction.Instruction>asList(
                            new ImmutableInstruction35c(Opcode.INVOKE_DIRECT, 1, 0, 0, 0, 0, 0,
                                    new ImmutableMethodReference(BINDER, "<init>",
                                            Collections.<CharSequence>emptyList(), "V")),
                            new ImmutableInstruction10x(Opcode.RETURN_VOID)), null, null)));

            int n = 0;
            for (Method m : def.getVirtualMethods()) {
                if (m.getImplementation() != null) continue;       // only abstract interface methods
                int paramRegs = 1;                                  // this
                for (CharSequence p : m.getParameterTypes()) paramRegs += width(p.toString());
                String ret = m.getReturnType();
                List<com.android.tools.smali.dexlib2.iface.instruction.Instruction> body = new ArrayList<>();
                int locals;

                if (ret.equals("V")) {
                    locals = 0;
                    body.add(new ImmutableInstruction10x(Opcode.RETURN_VOID));
                } else if (ret.equals("J") || ret.equals("D")) {
                    locals = 2;
                    body.add(new ImmutableInstruction21s(Opcode.CONST_WIDE_16, 0, 0));
                    body.add(new ImmutableInstruction11x(Opcode.RETURN_WIDE, 0));
                } else if (ret.length() == 1) {                     // Z B C S I F
                    locals = 1;
                    body.add(new ImmutableInstruction11n(Opcode.CONST_4, 0, 0));
                    body.add(new ImmutableInstruction11x(Opcode.RETURN, 0));
                } else if (implOf.containsKey(ret)) {
                    // hand back a real instance rather than null: callers dereference immediately
                    locals = 1;
                    body.add(new ImmutableInstruction21c(Opcode.NEW_INSTANCE, 0,
                            new ImmutableTypeReference(implOf.get(ret))));
                    body.add(new ImmutableInstruction35c(Opcode.INVOKE_DIRECT, 1, 0, 0, 0, 0, 0,
                            new ImmutableMethodReference(implOf.get(ret), "<init>",
                                    Collections.<CharSequence>emptyList(), "V")));
                    body.add(new ImmutableInstruction11x(Opcode.RETURN_OBJECT, 0));
                } else {
                    locals = 1;
                    body.add(new ImmutableInstruction11n(Opcode.CONST_4, 0, 0));
                    body.add(new ImmutableInstruction11x(Opcode.RETURN_OBJECT, 0));
                }
                int regCount = locals + paramRegs;
                methods.add(new ImmutableMethod(implType, m.getName(), m.getParameters(), ret,
                        AccessFlags.PUBLIC.getValue(), null, null,
                        new ImmutableMethodImplementation(regCount, body, null, null)));
                n++;
            }
            boolean hasAsBinder = false;
            for (Method m : methods) if ("asBinder".equals(m.getName())) hasAsBinder = true;
            if (!hasAsBinder) {
                // `this` is p0; with no locals it is v0. We extend Binder, so it IS an IBinder.
                methods.add(new ImmutableMethod(implType, "asBinder", null, IBINDER,
                        AccessFlags.PUBLIC.getValue(), null, null,
                        new ImmutableMethodImplementation(1, Arrays.<com.android.tools.smali.dexlib2.iface.instruction.Instruction>asList(
                                new ImmutableInstruction11x(Opcode.RETURN_OBJECT, 0)), null, null)));
                n++;
            }
            pool.internClass(new ImmutableClassDef(implType,
                    AccessFlags.PUBLIC.getValue() | AccessFlags.FINAL.getValue(),
                    BINDER, Collections.singletonList(iface), null, null, null, null,
                    direct, methods));
            System.out.println("  " + implType + " implements " + iface + " (" + n + " methods)");
            total += n;
        }
        pool.writeTo(new FileDataStore(new File(a[1])));
        System.out.println("generated " + wanted.size() + " impl class(es), " + total + " methods");
    }
}
