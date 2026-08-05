// FindClassRefs <dex> <Lclass;> [<Lclass;> ...]
//
// List every method of the given class(es) that the dex actually references, with a call count.
//
// §506: the audio bring-up was costing one ~15-minute device round per missing native — bind one,
// rebuild, redeploy, wait out a 5-minute library sync, watch it die on the next one. Each unbound
// native throws UnsatisfiedLinkError, which is an Error, which kills the calling HandlerThread with
// no report (ThreadGroup.uncaughtException is a no-op here), so they only surface one at a time.
//
// The dex already knows the whole answer statically: whatever ExoPlayer can call on
// android/media/AudioTrack is in its method reference table. Enumerate that once, bind the lot, and
// the rounds collapse into one.
import com.android.tools.smali.dexlib2.*;
import com.android.tools.smali.dexlib2.iface.*;
import com.android.tools.smali.dexlib2.iface.instruction.*;
import com.android.tools.smali.dexlib2.iface.reference.MethodReference;
import java.io.File;
import java.util.*;

public class FindClassRefs {
    public static void main(String[] a) throws Exception {
        if (a.length < 2) {
            System.out.println("usage: FindClassRefs <dex> <Lclass;> [...]");
            System.exit(1);
        }
        Set<String> want = new HashSet<>(Arrays.asList(a).subList(1, a.length));
        DexFile in = DexFileFactory.loadDexFile(new File(a[0]), Opcodes.forApi(34));

        // Counting call sites, not just presence: a method referenced from many places is on a hot
        // path and far more likely to be reached than one referenced once from a rarely-taken branch.
        Map<String, Integer> hits = new TreeMap<>();
        for (ClassDef c : in.getClasses()) {
            for (Method m : c.getMethods()) {
                MethodImplementation impl = m.getImplementation();
                if (impl == null) continue;
                for (Instruction ins : impl.getInstructions()) {
                    if (!(ins instanceof ReferenceInstruction)) continue;
                    Object ref = ((ReferenceInstruction) ins).getReference();
                    if (!(ref instanceof MethodReference)) continue;
                    MethodReference mr = (MethodReference) ref;
                    if (!want.contains(mr.getDefiningClass())) continue;
                    StringBuilder sig = new StringBuilder(mr.getDefiningClass());
                    sig.append("->").append(mr.getName()).append('(');
                    for (CharSequence p : mr.getParameterTypes()) sig.append(p);
                    sig.append(')').append(mr.getReturnType());
                    hits.merge(sig.toString(), 1, Integer::sum);
                }
            }
        }
        for (Map.Entry<String, Integer> e : hits.entrySet()) {
            System.out.printf("%5d  %s%n", e.getValue(), e.getKey());
        }
        System.out.println("-- " + hits.size() + " distinct method(s) referenced --");
    }
}
