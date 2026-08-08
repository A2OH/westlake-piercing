// Write a NEW dex containing exactly ONE class pulled from a source dex.
// Needed because DexMerge treats its helper dex as "replace/add ALL of these classes" — handing it a
// whole foreign dex would overwrite every class in the base. This narrows the helper to one type.
import com.android.tools.smali.dexlib2.DexFileFactory;
import com.android.tools.smali.dexlib2.Opcodes;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;
import java.io.File;
public class ExtractCls {
    public static void main(String[] a) throws Exception {
        DexFile src = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.getDefault());
        DexPool pool = new DexPool(Opcodes.getDefault());
        int n = 0;
        for (ClassDef c : src.getClasses()) {
            if (c.getType().equals(a[2])) { pool.internClass(c); n++; }
        }
        if (n != 1) { System.out.println("FAIL: found " + n + " of " + a[2]); System.exit(2); }
        pool.writeTo(new com.android.tools.smali.dexlib2.writer.io.FileDataStore(new File(a[1])));
        System.out.println("extracted " + a[2] + " -> " + a[1]);
    }
}
