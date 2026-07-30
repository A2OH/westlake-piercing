import com.android.tools.smali.dexlib2.*; import com.android.tools.smali.dexlib2.iface.*; import java.io.File;
public class HasCls { public static void main(String[] a) throws Exception {
  DexFile d=DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
  for (ClassDef c: d.getClasses()) if (c.getType().startsWith(a[1])) System.out.println("  "+c.getType());
}}
