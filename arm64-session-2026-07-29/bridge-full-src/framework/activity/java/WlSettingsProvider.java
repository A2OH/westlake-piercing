package adapter.activity;

/**
 * WESTLAKE §611e — minimal in-process "settings" ContentProvider.
 *
 * This port has no settings service, so Settings$NameValueCache's provider lookup
 * returns null and any code path that reads settings through the provider API NPEs.
 * First caller found: androidx SearchView inflate → TextView Editor →
 * TextClassificationConstants → DeviceConfig.getProperty →
 * Settings$Config.getStrings → getStringsForPrefix (Material Catalog, cat_toc_header).
 *
 * AppSchedulerBridge appends a synthetic ProviderInfo (authority "settings") pointing
 * here, so ActivityThread.installContentProviders publishes it into the process-local
 * provider map at bind time — acquireProvider finds it BEFORE any AMS call, so the
 * stub AMS is never involved.
 *
 * Semantics: every read reports "unset" — GET_* methods return a Bundle without a
 * value entry (getString -> null), LIST_* methods return an empty serializable map.
 * Callers then take their documented defaults, exactly like a fresh device image.
 */
public class WlSettingsProvider extends android.content.ContentProvider {
    @Override public boolean onCreate() { return true; }

    @Override
    public android.os.Bundle call(String method, String arg, android.os.Bundle extras) {
        android.os.Bundle b = new android.os.Bundle();
        if (method != null && method.startsWith("LIST")) {
            b.putSerializable("value", new java.util.HashMap<String, String>());
        }
        return b;
    }

    @Override
    public android.database.Cursor query(android.net.Uri uri, String[] projection,
            String selection, String[] selectionArgs, String sortOrder) {
        return new android.database.MatrixCursor(new String[] { "name", "value" });
    }

    @Override public String getType(android.net.Uri uri) { return null; }

    @Override
    public android.net.Uri insert(android.net.Uri uri, android.content.ContentValues values) {
        return null;
    }

    @Override public int delete(android.net.Uri uri, String selection, String[] args) { return 0; }

    @Override
    public int update(android.net.Uri uri, android.content.ContentValues values,
            String selection, String[] args) {
        return 0;
    }
}
