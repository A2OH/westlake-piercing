import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.iface.*;
import java.io.File;
public class Verify {
    public static void main(String[] a) throws Exception {
        for (String p : a) {
            DexFile d = DexFileFactory.loadDexFile(new File(p), Opcodes.forApi(34));
            int cls=0, meth=0, withCode=0;
            for (ClassDef c : d.getClasses()) { cls++;
                for (Method m : c.getMethods()) { meth++; if (m.getImplementation()!=null) withCode++; }
                if (c.getType().equals("Lwestlake/WlProxy;")) {
                    System.out.println("  FOUND " + c.getType() + " flags=0x"+Integer.toHexString(c.getAccessFlags()));
                    for (Method m : c.getMethods())
                        System.out.println("     " + m.getName() + " flags=0x"+Integer.toHexString(m.getAccessFlags())
                            + (m.getImplementation()!=null?" [code]":" [ABSTRACT]"));
                }
                if (c.getType().equals("Lq6/b;"))
                    System.out.println("  q6.b methods=" + java.util.stream.StreamSupport
                        .stream(c.getMethods().spliterator(),false).count());
            }
            System.out.println(new File(p).getName()+": classes="+cls+" methods="+meth+" withCode="+withCode);
        }
    }
}
