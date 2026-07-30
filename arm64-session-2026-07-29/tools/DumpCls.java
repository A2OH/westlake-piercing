import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.iface.*;
import java.io.File;
public class DumpCls {
    public static void main(String[] a) throws Exception {
        DexFile d = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
        for (ClassDef c : d.getClasses()) {
            if (!c.getType().contains(a[1])) continue;
            System.out.println("CLASS "+c.getType()+" super="+c.getSuperclass());
            for (Method m : c.getMethods()) {
                int units=0;
                if (m.getImplementation()!=null)
                    for (com.android.tools.smali.dexlib2.iface.instruction.Instruction i
                         : m.getImplementation().getInstructions()) units+=i.getCodeUnits();
                StringBuilder ps=new StringBuilder();
                for (CharSequence p : m.getParameterTypes()) ps.append(p);
                System.out.println("   "+m.getName()+"("+ps+")"+m.getReturnType()+"  codeUnits="+units);
            }
        }
    }
}
