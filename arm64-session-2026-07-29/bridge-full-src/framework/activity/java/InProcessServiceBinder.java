/*
 * InProcessServiceBinder.java
 *
 * [FIX-AUDIO 2026-06-30] In-app Android services (e.g. noice
 * SoundPlaybackService) are NOT OHOS abilities, so routing bindService to OH
 * ConnectAbility fails and the app blocks forever waiting for
 * onServiceConnected. This binds such services IN-PROCESS the way
 * ActivityThread.handleBindService does: instantiate the Service, attach +
 * onCreate + onBind, then deliver the binder to the app's IServiceConnection.
 *
 * Runs on the main (Looper) thread, posted from
 * ActivityManagerAdapter.bindService, matching AOSP's asynchronous bind
 * contract (onServiceConnected is never delivered synchronously inside
 * bindService).
 *
 * Top-level (not anonymous/nested) so d8 dexes it without enclosing-method
 * issues.
 */
package adapter.activity;

import android.app.Application;
import android.app.ActivityThread;
import android.app.Service;
import android.app.IServiceConnection;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.util.Log;

import java.util.concurrent.ConcurrentHashMap;

public class InProcessServiceBinder implements Runnable {

    private static final String TAG = "OH_InProcSvc";

    // className -> live Service instance (created once, reused across binds)
    private static final ConcurrentHashMap<String, Service> sServices =
            new ConcurrentHashMap<>();

    private final Intent mService;
    private final ComponentName mComp;
    private final IServiceConnection mConnection;

    public InProcessServiceBinder(Intent service, ComponentName comp,
            IServiceConnection connection) {
        this.mService = service;
        this.mComp = comp;
        this.mConnection = connection;
    }

    /** True if comp targets a service in this app's own package. */
    public static boolean isInApp(ComponentName comp) {
        if (comp == null) return false;
        Application app = ActivityThread.currentApplication();
        if (app == null) return false;
        String pkg = app.getPackageName();
        return pkg != null && pkg.equals(comp.getPackageName());
    }

    @Override
    public void run() {
        try {
            Application app = ActivityThread.currentApplication();
            ActivityThread thread = ActivityThread.currentActivityThread();
            String cls = mComp.getClassName();
            Service svc = sServices.get(cls);
            if (svc == null) {
                Class<?> svcClass = app.getClassLoader().loadClass(cls);
                svc = (Service) svcClass.newInstance();
                svc.attach(app, thread, cls, new Binder(), app, null);
                svc.onCreate();
                sServices.put(cls, svc);
                Log.i(TAG, "created in-process service " + cls);
            }
            IBinder binder = svc.onBind(mService);
            mConnection.connected(mComp, binder, false);
            Log.i(TAG, "connected " + cls + " binder=" + binder);
        } catch (Throwable t) {
            Log.e(TAG, "in-process bind failed for " + mComp, t);
        }
    }
}
