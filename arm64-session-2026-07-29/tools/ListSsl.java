import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.iface.*;
import java.io.File;
public class ListSsl {
    public static void main(String[] a) throws Exception {
        for (String p : a) {
            DexFile d;
            try { d = DexFileFactory.loadDexFile(new File(p), Opcodes.forApi(34)); }
            catch (Exception e) { System.out.println(p+": LOAD FAIL "+e); continue; }
            int tot=0, hit=0, abstractM=0, codeM=0;
            StringBuilder sb=new StringBuilder();
            for (ClassDef c : d.getClasses()) { tot++;
                String t=c.getType();
                if (t.startsWith("Ljavax/net/ssl/") || t.contains("conscrypt") || t.contains("Conscrypt")) {
                    hit++;
                    int am=0, cm=0;
                    for (Method m : c.getMethods()) { if (m.getImplementation()==null) am++; else cm++; }
                    abstractM+=am; codeM+=cm;
                    if (hit<=14) sb.append("   ").append(t)
                        .append(" code=").append(cm).append(" nocode=").append(am).append('\n');
                }
            }
            System.out.println(new File(p).getName()+": classes="+tot+" sslClasses="+hit
                +" methodsWithCode="+codeM+" withoutCode="+abstractM);
            System.out.print(sb);
        }
    }
}
