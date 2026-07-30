/* WESTLAKE 2026-07-22: minimal stand-in for AOSP's external/sqlite/android/sqlite3_android.h.
 * Only two entry points are referenced by android_database_SQLiteConnection.cpp. They install
 * Android-specific SQL helpers (_TOKENIZE, PHONE_NUMBERS_EQUAL) and the LOCALIZED collator,
 * which depend on ICU. Room / AppIntro use none of them, so we register nothing and report
 * success — a query that explicitly uses COLLATE LOCALIZED would fail, which nothing here does. */
#ifndef WL_SQLITE3_ANDROID_H
#define WL_SQLITE3_ANDROID_H
#include <sqlite3.h>
#ifdef __cplusplus
extern "C" {
#endif
int register_android_functions(sqlite3* handle, int utf16Storage);
int register_localized_collators(sqlite3* handle, const char* systemLocale, int utf16Storage);
#ifdef __cplusplus
}
#endif
#endif
