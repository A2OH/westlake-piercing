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
      int idx=0;
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
        // ★Print the instruction INDEX and its registers. Without these, injecting a trace call at
        // an index is guesswork (branch targets are not shown), and logging a VALUE is impossible
        // because you cannot name the register holding it.
        StringBuilder rg=new StringBuilder();
        if (ins instanceof OneRegisterInstruction) rg.append(" v").append(((OneRegisterInstruction)ins).getRegisterA());
        if (ins instanceof TwoRegisterInstruction) rg.append(",v").append(((TwoRegisterInstruction)ins).getRegisterB());
        if (ins instanceof ThreeRegisterInstruction) rg.append(",v").append(((ThreeRegisterInstruction)ins).getRegisterC());
        if (ins instanceof RegisterRangeInstruction) { RegisterRangeInstruction rr=(RegisterRangeInstruction)ins;
          rg.append(" v").append(rr.getStartRegister()).append("..+").append(rr.getRegisterCount()); }
        if (ins instanceof FiveRegisterInstruction) { FiveRegisterInstruction f=(FiveRegisterInstruction)ins;
          rg.setLength(0); rg.append(" {");
          int n=f.getRegisterCount();
          int[] rs={f.getRegisterC(),f.getRegisterD(),f.getRegisterE(),f.getRegisterF(),f.getRegisterG()};
          for(int i=0;i<n;i++){ if(i>0) rg.append(","); rg.append("v").append(rs[i]); }
          rg.append("}"); }
        System.out.println("   ["+(idx++)+"] "+ins.getOpcode()+rg+ref);
      }
    }
  }
}}
