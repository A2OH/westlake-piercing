/* WESTLAKE 2026-07-22: no-op implementations — see sqlite3_android.h for rationale. */
#include "sqlite3_android.h"
#include <string>
#include <cstring>
#include <clocale>
#include <stdio.h>
extern "C" int register_android_functions(sqlite3* handle, int utf16Storage) {
    (void)handle; (void)utf16Storage;
    return SQLITE_OK;   // Android SQL extensions not needed by Room
}
/* WESTLAKE 2026-07-22 (§158): Room runs "REINDEX LOCALIZED" when it opens its database. With no
 * collation of that name registered, sqlite3 fails with
 *   "unable to identify the object to be reindexed (code 1 SQLITE_ERROR)"
 * that SQLiteException lands on a Kotlin coroutine, the port's no-op ThreadGroup.uncaughtException
 * discards it, the coroutine dies, MainActivity.onCreate never returns, the Looper never dispatches
 * the next Activity launch, and no window is ever created. AOSP backs LOCALIZED with ICU; Room only
 * requires that a collation of this NAME exist and order sanely, so a strcoll-based comparator
 * (locale-aware via the process locale) is sufficient here.
 */
static int wl_localized_cmp(void*, int lLen, const void* lhs, int rLen, const void* rhs) {
    std::string a(static_cast<const char*>(lhs), static_cast<size_t>(lLen));
    std::string b(static_cast<const char*>(rhs), static_cast<size_t>(rLen));
    int r = strcoll(a.c_str(), b.c_str());
    if (r != 0) return (r < 0) ? -1 : 1;
    // Stable tiebreak so equal collation keys still order deterministically.
    if (a == b) return 0;
    return (a < b) ? -1 : 1;
}

extern "C" int register_localized_collators(sqlite3* handle, const char* systemLocale,
                                            int utf16Storage) {
    (void)systemLocale; (void)utf16Storage;
    if (handle == nullptr) return SQLITE_OK;
    int rc = sqlite3_create_collation(handle, "LOCALIZED", SQLITE_UTF8, nullptr, wl_localized_cmp);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[WESTLAKE-COLLATE] LOCALIZED registration failed rc=%d\n", rc);
        fflush(stderr);
        return rc;
    }
    // AOSP also exposes PHONEBOOK; register it with the same comparator so any use resolves.
    sqlite3_create_collation(handle, "PHONEBOOK", SQLITE_UTF8, nullptr, wl_localized_cmp);
    fprintf(stderr, "[WESTLAKE-COLLATE] LOCALIZED + PHONEBOOK registered\n");
    fflush(stderr);
    return SQLITE_OK;
}
