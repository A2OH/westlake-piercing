package android.media;

import android.os.IBinder;
import android.os.ServiceManager;

/**
 * WESTLAKE §471 — the media service manager this runtime never had.
 *
 * android.media.session.MediaSessionManager's constructor does, in bytecode:
 *     MediaFrameworkPlatformInitializer.getMediaServiceManager()
 *         .getMediaSessionServiceRegisterer()
 *         .get()                                  -> IBinder
 *     ISessionManager$Stub.asInterface(binder)     -> mService
 *
 * adapter-mainline-stubs.jar shipped MediaFrameworkPlatformInitializer WITHOUT
 * getMediaServiceManager(), and had no MediaServiceManager class at all, so the very first hop threw
 *     NoSuchMethodError: No InvokeType(0) method getMediaServiceManager()
 * and MediaSessionManager could never be constructed — which is why getSystemService(MEDIA_SESSION_
 * SERVICE) returned null and MediaSession.<init> NPE'd (a2hlab audio gap 2).
 *
 * This mirrors AOSP's shape rather than inventing one, so the existing call sites resolve unchanged.
 * get() goes through ServiceManager, which is where §467 already seeds a "media_session" binder whose
 * queryLocalInterface() hands back a local ISessionManager — so asInterface() uses it directly and
 * never transacts.
 */
public class MediaServiceManager {

    /** Mirrors android.os.ServiceManager.ServiceRegisterer: a named handle to one service binder. */
    public static final class ServiceRegisterer {
        private final String mServiceName;

        public ServiceRegisterer(String serviceName) { mServiceName = serviceName; }

        public IBinder get() { return ServiceManager.getService(mServiceName); }

        /** AOSP throws ServiceNotFoundException here; callers in this runtime only use get(). */
        public IBinder getOrThrow() { return get(); }

        public void register(IBinder service) { ServiceManager.addService(mServiceName, service); }

        public IBinder tryGet() { return get(); }
    }

    public static final String MEDIA_SESSION_SERVICE = "media_session";
    public static final String MEDIA_TRANSCODING_SERVICE = "media.transcoding";
    public static final String MEDIA_COMMUNICATION_SERVICE = "media_communication";

    private final ServiceRegisterer mSession = new ServiceRegisterer(MEDIA_SESSION_SERVICE);
    private final ServiceRegisterer mTranscoding = new ServiceRegisterer(MEDIA_TRANSCODING_SERVICE);
    private final ServiceRegisterer mCommunication = new ServiceRegisterer(MEDIA_COMMUNICATION_SERVICE);

    public MediaServiceManager() {}

    public ServiceRegisterer getMediaSessionServiceRegisterer() { return mSession; }

    public ServiceRegisterer getMediaTranscodingServiceRegisterer() { return mTranscoding; }

    public ServiceRegisterer getMediaCommunicationServiceRegisterer() { return mCommunication; }
}
