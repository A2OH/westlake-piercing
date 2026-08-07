// Host-side differential test for wl_fastlibc.cpp's strstr replacement (§532/§544).
// Compares our strstr against the real libc one across ordinary and degenerate inputs, with the
// §544 histogram both OFF and ON, and checks that the dump file is well formed.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

// ⚠️Pull in the C headers FIRST so their include guards fire before the renames below.
// libstdc++'s <stdlib.h> does `using std::getenv;` at namespace scope, and with the macro active
// that becomes `using std::wl_getenv;` -> "not declared in std".
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

// Our implementation, renamed so it can coexist with glibc's in one binary.
#define strstr wl_strstr
#define getenv wl_getenv
// The file calls getenv() from its constructor, i.e. BEFORE it defines getenv() further down.
// Normally <stdlib.h> supplies that declaration; under the rename it does not, so forward-declare.
// (No device-side equivalent: our getenv only reads `environ`, so a constructor call is safe.)
extern "C" char* wl_getenv(const char*);
#include "wl_fastlibc_under_test.cpp"
#undef strstr
#undef getenv

extern "C" char* wl_strstr(const char*, const char*);
extern "C" char* wl_getenv(const char*);

static int failures = 0;
static long checks = 0;

static void Check(const std::string& h, const std::string& n) {
    const char* got = wl_strstr(h.c_str(), n.c_str());
    const char* want = strstr(h.c_str(), n.c_str());
    ++checks;
    // Compare by offset, not pointer identity of a temporary.
    long g = got ? (got - h.c_str()) : -1;
    long w = want ? (want - h.c_str()) : -1;
    if (g != w) {
        if (++failures <= 10) {
            std::printf("MISMATCH h=\"%s\" n=\"%s\" got=%ld want=%ld\n",
                        h.substr(0, 60).c_str(), n.substr(0, 30).c_str(), g, w);
        }
    }
}

int main() {
    std::vector<std::string> haystacks = {
        "", "a", "ab", "abc", "aaaaaaaa", "abababababab",
        "Landroid/view/SurfaceControl;", "nativeGetObjectSchemaInfo",
        "Lcom/github/ashutoshgngwr/noice/MainActivity;", "onCreate", "invokeSuspend",
        "Ljava/lang/System;", "currentTimeMillis",
        std::string(600, 'x'),                       // exceeds kScanByteBudget
        std::string(600, 'x') + "needle",            // match past the budget
        std::string(200, 'a') + "ab",                // many first-byte hits then a match
        std::string(100, 'a'),                       // many first-byte hits, no match
    };
    std::vector<std::string> needles = {
        "", "a", "b", "ab", "abc", "aab", "x", "needle", "SurfaceControl", "Schema",
        "onCreate", "zzz", std::string(50, 'x'), "aaaaaaaaaaab",
    };

    // Pass 1: histogram OFF (default — constructor already ran with WL_STRSTR_HIST unset).
    for (auto& h : haystacks)
        for (auto& n : needles) Check(h, n);
    long pass1 = checks;
    std::printf("pass1 (hist off): %ld cases, %d failures\n", pass1, failures);

    // Pass 2: histogram ON — force the sampling path and a dump.
    g_hist_on = true;
    for (int rep = 0; rep < 400; ++rep)
        for (auto& h : haystacks)
            for (auto& n : needles) Check(h, n);
    std::printf("pass2 (hist on):  %ld cases total, %d failures\n", checks, failures);
    std::printf("hist: calls=%llu samples=%llu\n",
                (unsigned long long)g_hist_calls, (unsigned long long)g_hist_samples);

    // Force a dump regardless of whether the threshold was hit, then validate the file.
    HistDump();
    FILE* f = std::fopen(WL_HIST_PATH, "r");
    if (!f) {
        std::printf("DUMP MISSING at %s\n", WL_HIST_PATH);
        ++failures;
    } else {
        char line[256];
        int lines = 0, bad = 0;
        while (std::fgets(line, sizeof line, f)) {
            ++lines;
            if (line[0] == '#') continue;
            char* tab = std::strchr(line, '\t');
            if (!tab) { ++bad; continue; }
            for (char* p = line; p < tab; ++p)
                if (*p < '0' || *p > '9') { ++bad; break; }
        }
        std::fclose(f);
        std::printf("dump: %d lines, %d malformed\n", lines, bad);
        if (bad) ++failures;
        if (lines < 2) { std::printf("DUMP EMPTY\n"); ++failures; }
    }

    // §562 SWAR alignment sweep: the word scan reads 8-byte ALIGNED words after a byte prologue,
    // so every (start alignment x length) combination must be exercised, not just heap-aligned
    // strings. Walk a buffer at all 8 offsets, all lengths 0..40, against the real strstr.
    {
        char buf[128];
        int afail = 0; long achecks = 0;
        const char* needles[] = {"a","z","ab","abc","xyz","q","Landroid","/","\0z"+1};
        for (int off = 0; off < 8; ++off)
            for (int len = 0; len <= 40; ++len) {
                char* h = buf + off;
                for (int i = 0; i < len; ++i) h[i] = (char)('a' + (i * 7 + off) % 26);
                h[len] = '\0';
                for (const char* n : needles) {
                    const char* a = wl_strstr(h, n);
                    const char* b = strstr(h, n);
                    ++achecks;
                    long ga = a ? a - h : -1, gb = b ? b - h : -1;
                    if (ga != gb && ++afail <= 5)
                        std::printf("SWAR MISMATCH off=%d len=%d n=\"%s\" got=%ld want=%ld\n",
                                    off, len, n, ga, gb);
                }
            }
        std::printf("swar alignment sweep: %ld comparisons, %d failures\n", achecks, afail);
        failures += afail;
    }

    // §557 getenv differential: cached-index lookups must agree with the real getenv, including
    // across a setenv that CHANGES a value and an unsetenv that REMOVES one (the two cases an
    // index cache has to survive).
    {
        const char* names[] = {"PATH","HOME","LANG","NOT_SET_XYZ","WL_TEST_A","SHELL","USER","TERM"};
        int gfail = 0; long gchecks = 0;
        setenv("WL_TEST_A", "one", 1);
        for (int round = 0; round < 3; ++round) {
            if (round == 1) setenv("WL_TEST_A", "two-longer", 1);   // value change
            if (round == 2) unsetenv("WL_TEST_A");                  // removal
            for (int rep = 0; rep < 50; ++rep)
                for (const char* n : names) {
                    const char* a = wl_getenv(n);
                    const char* b = getenv(n);
                    ++gchecks;
                    bool same = (a == nullptr && b == nullptr) ||
                                (a != nullptr && b != nullptr && std::strcmp(a, b) == 0);
                    if (!same && ++gfail <= 5)
                        std::printf("GETENV MISMATCH round=%d %s: ours=%s real=%s\n",
                                    round, n, a ? a : "(null)", b ? b : "(null)");
                }
        }
        // ★The dangerous direction for a NEGATIVE cache: query while absent (caches "absent"),
        // then create it, then query again. A cache that does not invalidate returns null forever.
        for (int rep = 0; rep < 20; ++rep) (void)wl_getenv("WL_LATE_ADD");   // cache it as absent
        setenv("WL_LATE_ADD", "appeared", 1);
        for (int rep = 0; rep < 20; ++rep) {
            const char* a = wl_getenv("WL_LATE_ADD");
            const char* b = getenv("WL_LATE_ADD");
            ++gchecks;
            bool same = (a == nullptr && b == nullptr) ||
                        (a != nullptr && b != nullptr && std::strcmp(a, b) == 0);
            if (!same && ++gfail <= 5)
                std::printf("GETENV STALE-NEGATIVE: ours=%s real=%s\n",
                            a ? a : "(null)", b ? b : "(null)");
        }
        unsetenv("WL_LATE_ADD");

        std::printf("getenv: %ld comparisons, %d failures\n", gchecks, gfail);
        failures += gfail;
    }

    std::printf(failures == 0 ? "\nALL OK (%ld comparisons)\n" : "\nFAILURES (%ld comparisons)\n",
                checks);
    return failures ? 1 : 0;
}
