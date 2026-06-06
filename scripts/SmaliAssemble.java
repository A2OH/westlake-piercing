import brut.androlib.mod.SmaliMod;
import com.android.tools.smali.dexlib2.Opcodes;
import com.android.tools.smali.dexlib2.writer.builder.DexBuilder;
import com.android.tools.smali.dexlib2.writer.io.FileDataStore;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.stream.*;

public class SmaliAssemble {
  public static void main(String[] a) throws Exception {
    String inDir=a[0], outDex=a[1]; int api=Integer.parseInt(a[2]);
    DexBuilder db=new DexBuilder(Opcodes.forApi(api));
    List<File> files=Files.walk(Paths.get(inDir)).filter(p->p.toString().endsWith(".smali")).map(Path::toFile).sorted().collect(Collectors.toList());
    int n=0;
    for(File f: files){ boolean ok=SmaliMod.assembleSmaliFile(f, db, api, false, false); if(!ok) throw new RuntimeException("assemble failed: "+f); n++; }
    db.writeTo(new FileDataStore(new File(outDex)));
    System.out.println("assembled "+n+" smali files -> "+outDex+" ("+new File(outDex).length()+" bytes)");
  }
}
