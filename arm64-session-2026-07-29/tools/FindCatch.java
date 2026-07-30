import com.android.tools.smali.dexlib2.*; import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.iface.instruction.*; import java.io.File;
public class FindCatch { public static void main(String[] a) throws Exception {
  DexFile d=DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
  for (ClassDef c: d.getClasses()) {
    if (!c.getType().contains(a[1])) continue;
    for (Method m: c.getMethods()) {
      MethodImplementation im=m.getImplementation(); if(im==null) continue;
      int tb=0; StringBuilder h=new StringBuilder();
      for (TryBlock<? extends ExceptionHandler> t: im.getTryBlocks()) {
        tb++;
        for (ExceptionHandler eh: t.getExceptionHandlers())
          h.append("      handler type=").append(eh.getExceptionType())
           .append(" @0x").append(Integer.toHexString(eh.getHandlerCodeAddress())).append('\n');
      }
      if (tb>0) {
        int units=0; for (Instruction i: im.getInstructions()) units+=i.getCodeUnits();
        System.out.println(c.getType()+"."+m.getName()+"  tryBlocks="+tb+" units="+units);
        System.out.print(h);
      }
    }
  }
}}
