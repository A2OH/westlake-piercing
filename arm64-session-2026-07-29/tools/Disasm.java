import com.android.tools.smali.dexlib2.*; import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.iface.instruction.*;
import com.android.tools.smali.dexlib2.iface.reference.*;
import java.io.File;
public class Disasm { public static void main(String[] a) throws Exception {
  DexFile d=DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
  for (ClassDef c: d.getClasses()) {
    if (!c.getType().equals(a[1])) continue;
    for (Method m: c.getMethods()) {
      if (!m.getName().equals(a[2])) continue;
      StringBuilder ps=new StringBuilder(); for(CharSequence p:m.getParameterTypes()) ps.append(p);
      System.out.println("== "+c.getType()+"."+m.getName()+"("+ps+")"+m.getReturnType());
      MethodImplementation im=m.getImplementation();
      if (im==null) { System.out.println("   <no code / native>"); continue; }
      for (Instruction ins: im.getInstructions()) {
        String ref="";
        if (ins instanceof ReferenceInstruction) {
          Reference r=((ReferenceInstruction)ins).getReference();
          if (r instanceof MethodReference) { MethodReference mr=(MethodReference)r;
            StringBuilder q=new StringBuilder(); for(CharSequence p:mr.getParameterTypes()) q.append(p);
            ref=" "+mr.getDefiningClass()+"->"+mr.getName()+"("+q+")"+mr.getReturnType(); }
          else if (r instanceof FieldReference) { FieldReference fr=(FieldReference)r;
            ref=" "+fr.getDefiningClass()+"->"+fr.getName()+":"+fr.getType(); }
          else if (r instanceof StringReference) ref=" \""+((StringReference)r).getString()+"\"";
          else if (r instanceof TypeReference) ref=" "+((TypeReference)r).getType();
        }
        System.out.println("   "+ins.getOpcode()+ref);
      }
    }
  }
}}
