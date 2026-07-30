// DexMerge <base.dex> <helper.dex> <out.dex>
//
// Merge helper classes into a base dex, REPLACING any class the helper redefines. Used to add
// classes to a boot-classpath jar and to swap out one that shipped incomplete (§471:
// android.media.MediaFrameworkPlatformInitializer was missing getMediaServiceManager()).
//
// ★A DexPool rewrite renumbers indices, which is fine for these jars — §464 already rewrote
// framework.jar this way and it boots — but it drops debug info, so stack traces from rewritten
// classes lose line numbers.
import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.writer.io.FileDataStore;
import com.android.tools.smali.dexlib2.writer.pool.DexPool;
import java.io.File;
import java.util.*;

public class DexMerge {
    public static void main(String[] a) throws Exception {
        if (a.length < 3) { System.out.println("usage: DexMerge <base.dex> <helper.dex> <out.dex>"); System.exit(1); }
        DexFile base   = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));
        DexFile helper = DexFileFactory.loadDexFile(new File(a[1]), Opcodes.forApi(34));

        Map<String, ClassDef> repl = new LinkedHashMap<>();
        for (ClassDef c : helper.getClasses()) repl.put(c.getType(), c);

        DexPool pool = new DexPool(Opcodes.forApi(34));
        int kept = 0, replaced = 0;
        Set<String> seen = new HashSet<>();
        for (ClassDef c : base.getClasses()) {
            ClassDef r = repl.get(c.getType());
            if (r != null) { pool.internClass(r); replaced++; System.out.println("  REPLACED " + c.getType()); }
            else           { pool.internClass(c); kept++; }
            seen.add(c.getType());
        }
        int added = 0;
        for (Map.Entry<String, ClassDef> e : repl.entrySet()) {
            if (seen.contains(e.getKey())) continue;
            pool.internClass(e.getValue()); added++;
            System.out.println("  ADDED    " + e.getKey());
        }
        System.out.println("kept=" + kept + " replaced=" + replaced + " added=" + added);
        // Every helper class must land somewhere, or the jar ships silently incomplete.
        if (replaced + added != repl.size()) {
            System.out.println("FAIL: helper had " + repl.size() + " classes but only "
                    + (replaced + added) + " were written");
            System.exit(2);
        }
        pool.writeTo(new FileDataStore(new File(a[2])));
    }
}
