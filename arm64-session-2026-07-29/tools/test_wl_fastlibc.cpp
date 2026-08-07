// Host-side differential test for wl_fastlibc.cpp's strstr replacement (§532/§544).
// Compares our strstr against the real libc one across ordinary and degenerate inputs, with the
// §544 histogram both OFF and ON, and checks that the dump file is well formed.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

// Our implementation, renamed so it can coexist with glibc's in one binary.
#define strstr wl_strstr
#include "wl_fastlibc_under_test.cpp"
#undef strstr

extern "C" char* wl_strstr(const char*, const char*);

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

    std::printf(failures == 0 ? "\nALL OK (%ld comparisons)\n" : "\nFAILURES (%ld comparisons)\n",
                checks);
    return failures ? 1 : 0;
}
