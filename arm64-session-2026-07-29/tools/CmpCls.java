import com.android.tools.smali.dexlib2.DexFileFactory;
import com.android.tools.smali.dexlib2.Opcodes;
import com.android.tools.smali.dexlib2.iface.*;
import java.io.File; import java.util.*;
public class CmpCls {
    public static void main(String[] a) throws Exception {
        String type = a[2];
        DexFile d = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.getDefault());
        for (ClassDef c : d.getClasses()) {
            if (!c.getType().equals(type)) continue;
            List<String> ms = new ArrayList<>();
            for (Method m : c.getMethods()) {
                int sz = 0;
                if (m.getImplementation() != null)
                    for (Object i : m.getImplementation().getInstructions()) sz++;
                ms.add(m.getName() + m.getParameterTypes() + " insns=" + sz);
            }
            Collections.sort(ms);
            for (String s : ms) System.out.println(a[1] + "\t" + s);
            List<String> fs = new ArrayList<>();
            for (Field f : c.getFields()) fs.add("FIELD " + f.getName() + " : " + f.getType());
            Collections.sort(fs);
            for (String s : fs) System.out.println(a[1] + "\t" + s);
            return;
        }
        System.out.println(a[1] + "\tCLASS NOT FOUND");
    }
}
