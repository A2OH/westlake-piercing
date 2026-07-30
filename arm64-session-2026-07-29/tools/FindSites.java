// FindSites <dex> — locate every invoke-interface on the two failing targets.
import com.android.tools.smali.dexlib2.DexFileFactory;
import com.android.tools.smali.dexlib2.Opcodes;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.iface.instruction.*;
import com.android.tools.smali.dexlib2.iface.instruction.formats.*;
import com.android.tools.smali.dexlib2.iface.reference.MethodReference;
import com.android.tools.smali.dexlib2.Opcode;
import java.io.File;

public class FindSites {
    public static void main(String[] a) throws Exception {
        DexFile d = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
        int hits = 0, totalIface = 0;
        for (ClassDef c : d.getClasses()) {
            for (Method m : c.getMethods()) {
                MethodImplementation impl = m.getImplementation();
                if (impl == null) continue;
                int addr = 0;
                for (Instruction ins : impl.getInstructions()) {
                    if (ins.getOpcode() == Opcode.INVOKE_INTERFACE ||
                        ins.getOpcode() == Opcode.INVOKE_INTERFACE_RANGE) {
                        totalIface++;
                        ReferenceInstruction ri = (ReferenceInstruction) ins;
                        MethodReference mr = (MethodReference) ri.getReference();
                        String t = mr.getDefiningClass();
                        if (t.equals("Lq6/b;") || t.equals("Lg9/f;")) {
                            hits++;
                            StringBuilder ps = new StringBuilder();
                            for (CharSequence p : mr.getParameterTypes()) ps.append(p);
                            System.out.printf("HIT %s.%s  @0x%x  %s->%s(%s)%s  opcode=%s regs=%d%n",
                                c.getType(), m.getName(), addr,
                                t, mr.getName(), ps, mr.getReturnType(),
                                ins.getOpcode(), ((FiveRegisterInstruction) ins).getRegisterCount());
                        }
                    }
                    addr += ins.getCodeUnits();
                }
            }
        }
        System.out.println("total invoke-interface in dex: " + totalIface + " ; hits on targets: " + hits);
    }
}
