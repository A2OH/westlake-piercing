package android.media;

/**
 * WESTLAKE §471 — replaces the adapter-mainline-stubs.jar version, which had setMediaServiceManager()
 * but no getMediaServiceManager(), the method MediaSessionManager's constructor actually calls.
 *
 * Keeps the original members (<init>, registerServiceWrappers, setMediaServiceManager) so nothing that
 * already resolves against this class breaks, and adds the getter. If the platform never calls the
 * setter — it does not here — the getter creates the manager on demand rather than returning null,
 * because a null would just move the crash one frame down into MediaSessionManager.
 */
public class MediaFrameworkPlatformInitializer {

    private static volatile MediaServiceManager sMediaServiceManager;

    public MediaFrameworkPlatformInitializer() {}

    public static void setMediaServiceManager(MediaServiceManager mediaServiceManager) {
        sMediaServiceManager = mediaServiceManager;
    }

    public static MediaServiceManager getMediaServiceManager() {
        MediaServiceManager m = sMediaServiceManager;
        if (m == null) {
            synchronized (MediaFrameworkPlatformInitializer.class) {
                m = sMediaServiceManager;
                if (m == null) {
                    m = new MediaServiceManager();
                    sMediaServiceManager = m;
                    System.err.println("[WESTLAKE-471] created MediaServiceManager on demand");
                    System.err.flush();
                }
            }
        }
        return m;
    }

    public static void registerServiceWrappers() { }
}
