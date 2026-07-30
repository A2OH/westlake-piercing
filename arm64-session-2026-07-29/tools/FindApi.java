import com.android.tools.smali.dexlib2.*; import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.iface.value.*; import java.io.File;
public class FindApi {
  static String val(EncodedValue v){
    if (v instanceof StringEncodedValue) return ((StringEncodedValue)v).getValue();
    if (v instanceof ArrayEncodedValue){ StringBuilder b=new StringBuilder();
      for (EncodedValue e: ((ArrayEncodedValue)v).getValue()) b.append(val(e)).append(","); return b.toString(); }
    return String.valueOf(v);
  }
  public static void main(String[] a) throws Exception {
    DexFile d=DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
    for (ClassDef c: d.getClasses()) for (Method m: c.getMethods()) {
      for (Annotation an: m.getAnnotations()) {
        StringBuilder s=new StringBuilder();
        for (AnnotationElement el: an.getElements()) s.append(el.getName()).append("=").append(val(el.getValue())).append(" ");
        if (s.toString().contains(a[1])) {
          StringBuilder ps=new StringBuilder(); for(CharSequence p:m.getParameterTypes()) ps.append(p).append(" ");
          System.out.println(c.getType()+"."+m.getName()+"("+ps+")"+m.getReturnType());
          for (Annotation an2: m.getAnnotations()){
            StringBuilder s2=new StringBuilder();
            for (AnnotationElement el: an2.getElements()) s2.append(el.getName()).append("=").append(val(el.getValue())).append(" ");
            System.out.println("     @"+an2.getType()+" "+s2);
          }
        }
      }
    }
  }
}
