package adapter.activity;
import android.app.ActivityOptions;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;
import java.io.FileWriter;
import java.util.concurrent.ConcurrentHashMap;
public final class TransitionOptionsHolder {
    private static final ConcurrentHashMap<String, Bundle> MAP = new ConcurrentHashMap<String, Bundle>();
    private static int pid() { try { return android.os.Process.myPid(); } catch (Throwable t) { return -1; } }
    private static void log(String s) {
        try { FileWriter w = new FileWriter("/data/local/tmp/ohtransition.log", true); w.write("p"+pid()+" "+s+"\n"); w.close(); } catch (Throwable t) {}
    }
    /** diagnostic: called from bridgeStartAbility (confirms the Java A-side path + process). */
    public static void mark(Intent intent) {
        String cn = (intent != null && intent.getComponent() != null) ? intent.getComponent().getClassName() : "null";
        log("BRIDGE comp=" + cn);
    }
    public static void stashFromIntent(Intent intent, Bundle opts) {
        try {
            String cn = (intent != null && intent.getComponent() != null) ? intent.getComponent().getClassName() : "null";
            log("stash comp=" + cn + " opts=" + (opts == null ? "NULL" : ("sz=" + opts.size())));
            if (intent == null || opts == null) return;
            ComponentName c = intent.getComponent();
            if (c == null || c.getClassName() == null) return;
            MAP.put(c.getClassName(), opts);
            log("stash STORED " + c.getClassName());
        } catch (Throwable t) { log("stash EX " + t); }
    }
    public static ActivityOptions resolve(Intent intent) {
        try {
            String cn = (intent != null && intent.getComponent() != null) ? intent.getComponent().getClassName() : "null";
            log("resolve comp=" + cn + " mapsz=" + MAP.size());
            if (intent == null) return null;
            ComponentName c = intent.getComponent();
            if (c == null || c.getClassName() == null) return null;
            Bundle b = MAP.remove(c.getClassName());
            if (b == null) { log("resolve MISS " + c.getClassName()); return null; }
            ActivityOptions ao = (ActivityOptions) ActivityOptions.class.getMethod("fromBundle", Bundle.class).invoke(null, b);
            log("resolve HIT " + c.getClassName());
            return ao;
        } catch (Throwable t) { log("resolve EX " + t); return null; }
    }
}
