import com.android.tools.smali.dexlib2.*; import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.iface.instruction.*;
import com.android.tools.smali.dexlib2.iface.reference.*; import java.io.File;
public class FindCallers { public static void main(String[] a) throws Exception {
  DexFile d=DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
  for (ClassDef c: d.getClasses()) for (Method m: c.getMethods()) {
    MethodImplementation im=m.getImplementation(); if(im==null) continue;
    for (Instruction ins: im.getInstructions()) {
      if (!(ins instanceof ReferenceInstruction)) continue;
      Reference r=((ReferenceInstruction)ins).getReference();
      if (!(r instanceof MethodReference)) continue;
      MethodReference mr=(MethodReference)r;
      if ((mr.getDefiningClass()+"->"+mr.getName()).contains(a[1])) {
        StringBuilder ps=new StringBuilder(); for(CharSequence p:mr.getParameterTypes()) ps.append(p);
        System.out.println(c.getType()+"."+m.getName()+"  calls  "+mr.getDefiningClass()+"->"+mr.getName()+"("+ps+")");
      }
    }
  }
}}
