import com.android.tools.smali.dexlib2.*; import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.iface.instruction.*;
import com.android.tools.smali.dexlib2.iface.reference.*; import java.io.File;
public class DisasmA { public static void main(String[] a) throws Exception {
  DexFile d=DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
  for (ClassDef c: d.getClasses()) { if(!c.getType().equals(a[1])) continue;
    for (Method m: c.getMethods()) { if(!m.getName().equals(a[2])) continue;
      MethodImplementation im=m.getImplementation();
      System.out.println("registers="+im.getRegisterCount());
      int addr=0, idx=0;
      for (Instruction ins: im.getInstructions()) {
        String ref="";
        if (ins instanceof ReferenceInstruction) {
          Reference r=((ReferenceInstruction)ins).getReference();
          if (r instanceof MethodReference){ MethodReference mr=(MethodReference)r;
            ref=" "+mr.getDefiningClass()+"->"+mr.getName(); }
          else if (r instanceof FieldReference) ref=" "+((FieldReference)r).getName();
          else if (r instanceof TypeReference) ref=" "+((TypeReference)r).getType();
        }
        String regs="";
        if (ins instanceof OneRegisterInstruction) regs=" v"+((OneRegisterInstruction)ins).getRegisterA();
        System.out.printf("  [%3d] 0x%03x %s%s%s%n", idx, addr, ins.getOpcode(), regs, ref);
        addr+=ins.getCodeUnits(); idx++;
      }
    }
  }
}}
