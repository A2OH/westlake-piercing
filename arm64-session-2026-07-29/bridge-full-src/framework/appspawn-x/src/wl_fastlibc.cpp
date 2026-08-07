// wl_fastlibc.cpp — §532: preempt the two musl string routines that dominate this runtime.
//
// WHY THIS FILE EXISTS
// --------------------
// A 14 s hiperf capture of the app during a list scroll attributed CPU like this:
//
//     58.4%  /system/lib/ld-musl-aarch64.so.1      <- the C library, not the interpreter
//     31.1%  libart.so
//      7.7%  kernel
//
// and inside musl, after mapping every unnamed sample back through the symbol table:
//
//     39.31%  strstr        <- ONE function, 2/5 of the whole process
//      4.15%  memchr        )
//      3.79%  strchr        )  called BY strstr and getenv
//      2.92%  strcmp        )
//      2.79%  getenv
//
// The caller is westlake's own libart. Its per-invoke tracing predicates run chains like
//   strstr(descriptor, "SurfaceControl") ... strstr(method_name, "nativeGetObjectSchemaInfo")
// on EVERY interpreted call — 103 strstr sites across the patches, most of them leftovers from
// an unrelated app port (Realm, McDonalds) that can never match here. They all miss, and a miss
// is the expensive case: musl's strstr hands any needle of 5+ bytes to twoway_strstr, which
// rebuilds a 256-entry byte-shift table on EVERY call before it starts scanning. Setup cost is
// paid in full, per invoke, to learn nothing.
//
// libart is NOT rebuildable here (the deployed e1af9bb5… is not reproducible from source), so the
// call sites cannot be removed. But strstr is reached through libart's PLT, and appspawn-x is at
// the head of the global symbol scope — so replacing the CALLEE fixes every one of the 103 sites
// at once, without touching the caller. Adapt at the ABI boundary.
//
// CORRECTNESS
// -----------
// Both functions below are drop-in: same signature, same semantics, same return values as the
// musl originals. strstr keeps a degenerate-input escape hatch to the real implementation so we
// cannot turn a pathological O(n·m) case into a hang — see the guard below. getenv is not cached
// (a cache would go stale on setenv/putenv from another thread); it is literally musl's own
// algorithm with a first-byte filter in front of the strncmp, so it cannot disagree with it.

#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

extern "C" char** environ;

namespace {

// ---------------------------------------------------------------------------
// §544: haystack histogram — turning the predicate tax into a Java-level probe.
//
// The open question after §542/§543 was "what Java runs continuously while the app is idle?".
// Native profiling could not answer it: hiperf only ever shows the interpreter recursing through
// INVOKE_VIRTUAL -> DoCall -> ExecuteSwitchImplCpp, with no managed frames, and the usual ways in
// are all closed here (no SignalCatcher, so kill -3 kills the app; getStackTrace() returns empty;
// printStackTrace is a no-op).
//
// But libart calls GetNameView() and then strstr(name, "...") on every interpreted invoke — and
// we own strstr. The HAYSTACK it hands us is the method name / class descriptor of whatever is
// executing. Sampling haystacks into a frequency table therefore names the hot loop directly,
// with no libart rebuild (forbidden) and no dex injection.
//
// Off unless WL_STRSTR_HIST=1, and the switch is read once in a constructor, so the cost when
// disabled is one predictable branch on an already-loaded bool.
// ---------------------------------------------------------------------------
// Overridable so the host correctness test can exercise the real dump path (see
// tools/test_wl_fastlibc.cpp); the device never overrides it.
#ifndef WL_HIST_PATH
#define WL_HIST_PATH "/data/local/tmp/wl_strstr_hist.txt"
#endif

constexpr int kHistSlots = 8192;          // power of two; mask instead of modulo
constexpr int kHistTextMax = 48;          // longest prefix kept per distinct string

struct HistSlot {
    uint64_t hash;
    uint64_t count;
    char text[kHistTextMax];
};

HistSlot g_hist[kHistSlots];
bool g_hist_on = false;
uint64_t g_hist_calls = 0;                // every strstr call, sampled or not
uint64_t g_hist_samples = 0;              // calls actually recorded

// Sample 1 call in 64. libart runs ~100 predicates per invoke, all with the SAME haystack and
// different needles, so a 1/64 sample still reproduces the distribution of method names faithfully
// while keeping the hashing cost off 63 of every 64 calls.
constexpr uint64_t kSampleMask = 63;
// One dump per this many samples. The table is CLEARED after each dump, so every file is a fresh
// window rather than a cumulative total dominated by startup — which is what makes an "idle" read
// meaningful.
constexpr uint64_t kDumpEvery = 200000;

// Raw syscalls only: no stdio. This runs underneath libart, and fopen/printf would risk
// re-entering string routines we are in the middle of.
__attribute__((noinline)) void HistDump() {
    int fd = open(WL_HIST_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return;
    }
    char hdr[128];
    int n = 0;
    const char* kH = "# calls=";
    for (const char* q = kH; *q; ++q) hdr[n++] = *q;
    // tiny inline u64 -> decimal, twice; avoids snprintf
    auto put = [&](uint64_t v) {
        char tmp[24];
        int t = 0;
        if (v == 0) tmp[t++] = '0';
        while (v) { tmp[t++] = char('0' + (v % 10)); v /= 10; }
        while (t) hdr[n++] = tmp[--t];
    };
    put(g_hist_calls);
    hdr[n++] = ' '; hdr[n++] = 's'; hdr[n++] = '='; put(g_hist_samples);
    hdr[n++] = '\n';
    (void)write(fd, hdr, n);

    for (int i = 0; i < kHistSlots; ++i) {
        if (g_hist[i].count == 0) {
            continue;
        }
        char line[80];
        int m = 0;
        uint64_t v = g_hist[i].count;
        char tmp[24];
        int t = 0;
        if (v == 0) tmp[t++] = '0';
        while (v) { tmp[t++] = char('0' + (v % 10)); v /= 10; }
        while (t) line[m++] = tmp[--t];
        line[m++] = '\t';
        for (int k = 0; k < kHistTextMax && g_hist[i].text[k] != '\0'; ++k) {
            line[m++] = g_hist[i].text[k];
        }
        line[m++] = '\n';
        (void)write(fd, line, m);
    }
    close(fd);

    // Fresh window for the next dump.
    for (int i = 0; i < kHistSlots; ++i) {
        g_hist[i].hash = 0;
        g_hist[i].count = 0;
        g_hist[i].text[0] = '\0';
    }
    g_hist_samples = 0;
}

__attribute__((noinline)) void HistRecord(const char* h) {
    // FNV-1a over the kept prefix.
    uint64_t hv = 1469598103934665603ULL;
    int len = 0;
    while (len < kHistTextMax && h[len] != '\0') {
        hv ^= (unsigned char)h[len];
        hv *= 1099511628211ULL;
        ++len;
    }
    if (hv == 0) {
        hv = 1;  // 0 marks an empty slot
    }
    uint32_t idx = (uint32_t)(hv) & (kHistSlots - 1);
    for (int probe = 0; probe < 8; ++probe) {
        HistSlot& s = g_hist[idx];
        if (s.hash == hv) {
            ++s.count;
            break;
        }
        if (s.hash == 0) {
            s.hash = hv;
            s.count = 1;
            int k = 0;
            for (; k < len; ++k) {
                char c = h[k];
                // keep the file line-oriented even if a haystack contains control bytes
                s.text[k] = (c == '\n' || c == '\t') ? ' ' : c;
            }
            if (k < kHistTextMax) {
                s.text[k] = '\0';
            }
            break;
        }
        idx = (idx + 1) & (kHistSlots - 1);
    }
    if (++g_hist_samples >= kDumpEvery) {
        HistDump();
    }
}

// RTLD_NEXT skips our own definition and finds musl's.
//
// ⚠️Resolved in a constructor, NOT lazily on first use. The lazy version cost two straight
// startup failures: dlsym takes the dynamic linker's lock, and this strstr is called FROM the
// linker (it parses paths) and from arbitrary points during early startup, so a first use in
// the wrong place deadlocks a child that had already reached MainActivity. A constructor runs
// after relocation is complete and before any of that, so the hot path only ever loads a
// pointer that is already set.
using StrStrFn = char* (*)(const char*, const char*);
StrStrFn g_real_strstr = nullptr;

__attribute__((constructor)) void ResolveRealStrStr() {
    g_real_strstr = reinterpret_cast<StrStrFn>(dlsym(RTLD_NEXT, "strstr"));
    // §544: read the histogram switch ONCE, here, for the same reason the dlsym is here — getenv
    // on the hot path is exactly the per-invoke cost this file exists to remove.
    const char* v = getenv("WL_STRSTR_HIST");
    g_hist_on = (v != nullptr && v[0] == '1');
}

inline StrStrFn RealStrStr() {
    return g_real_strstr;
}

// How many first-byte hits we are willing to reject, and how many haystack bytes we are willing
// to scan, before concluding the input is degenerate (a long haystack, or one whose bytes keep
// colliding with the needle's first byte) and handing the rest to the real two-way implementation
// — whose whole point is that worst case. For the westlake predicates the hit count is 0 or 1 and
// the haystack is a method name or class descriptor, so neither branch is taken on the hot path.
constexpr int kFirstByteHitBudget = 64;
constexpr long kScanByteBudget = 512;

}  // namespace

// strstr — leftmost occurrence of n in h, or null.
//
// Everything is inline. A first version delegated to strchr/strlen/strncmp and, once musl's
// strstr was out of the profile, those three showed up in its place at 11.2% / 3.5% / 6.6% —
// the call overhead and the redundant strlen of a literal needle had simply become the new hot
// spot. Fusing them removes the calls entirely: a miss now costs one pass over a short method
// name with a one-byte compare, which is what these predicates deserve.
//
// The scan stops at the NUL in either operand, so a haystack shorter than the needle can never
// be over-read.
extern "C" char* strstr(const char* h, const char* n) {
    if (n[0] == '\0') {
        return const_cast<char*>(h);
    }
    // §544: diagnostic only, and compiled to a single predictable branch when off. See HistRecord.
    if (__builtin_expect(g_hist_on, false)) {
        if ((++g_hist_calls & kSampleMask) == 0) {
            HistRecord(h);
        }
    }
    const char n0 = n[0];

    // Handing off is only ever an optimisation, never a requirement for correctness: if the
    // constructor has not run yet (this can be called from the linker itself), real is null and
    // we simply keep scanning naively, which still returns the right answer.
    const StrStrFn real = RealStrStr();
    const char* p = h;
    int hits = 0;
    for (;;) {
        // §542: the first-byte scan is THE hot loop — at idle it was 41% of the main thread, because
        // libart runs ~100 of these predicates per interpreted invoke. It previously carried a
        // `p - h > budget` test on EVERY BYTE, which roughly doubled the cost of the one loop that
        // matters. The budget is only a degenerate-input heuristic, so it belongs outside.
        while (*p != '\0' && *p != n0) {
            ++p;
        }
        if (*p == '\0') {
            return nullptr;
        }
        const char* a = p + 1;
        const char* b = n + 1;
        while (*b != '\0' && *a == *b) {
            ++a;
            ++b;
        }
        if (*b == '\0') {
            return const_cast<char*>(p);
        }
        // Degenerate input (a haystack whose bytes keep colliding with the needle's first byte, or
        // one long enough that byte-at-a-time stops being the right tool): hand the rest to musl's
        // two-way search, whose whole point is that worst case. No match can exist before p, so
        // resuming there still yields the leftmost occurrence. Checked once per first-byte hit, not
        // once per byte.
        if (real != nullptr && (++hits >= kFirstByteHitBudget || p - h > kScanByteBudget)) {
            return real(p, n);
        }
        ++p;
    }
}

// §557: getenv IS now interposed — but with an index cache, not a value cache.
//
// §532 measured getenv at 2.79% and deliberately left it alone: "a cache would go stale on
// setenv/putenv from another thread". That reasoning was right about a VALUE cache. Meanwhile a
// profile taken during real work (a tab switch) puts it at **7.2%**, because libart's per-invoke
// predicates call it on every interpreted invoke and musl's getenv is a linear scan of the whole
// environment — this child has ~40 entries, so a miss walks all of them.
//
// The trick that makes it safe: cache the INDEX into `environ`, never the value, and re-validate
// that index on every hit by checking `environ[i]` still begins with "name=". That is O(len(name))
// instead of O(entries), and it stays correct under mutation without interposing setenv/putenv:
//   * setenv replacing a value  -> environ[i] still starts with "name=", and we return a pointer
//                                  INTO the current environ[i], i.e. the NEW value. Correct.
//   * unsetenv / reordering     -> the check fails, we fall back to a full scan and re-cache.
//   * environ replaced wholesale-> same, the check fails and we rescan.
// A miss (name absent) is NOT cached: that is the one case where caching could invent a wrong
// answer after a later setenv, and it is also the case libart hits most, so it must stay honest.
// Host-verified against the real getenv in tools/test_wl_fastlibc.cpp.
namespace {
constexpr int kEnvSlots = 64;
struct EnvSlot { const char* name; int idx; };   // idx < 0 => known ABSENT
EnvSlot g_env[kEnvSlots];
int g_envCount = 0;

// §557b: absent names must be cached too, or nothing improves. libart's predicates mostly ask for
// debug variables that are NOT set, and a miss is the expensive case — it walks the entire
// environment every single invoke. Caching "absent" is only sound while the environment is
// unchanged, so stamp the cache with (environ pointer, entry count) and drop it whenever either
// moves: adding a variable changes the count (and usually the pointer, since setenv reallocs), and
// replacing an existing value cannot turn an absent name into a present one.
char** g_envPtr = nullptr;
int g_envLen = -1;

inline int wl_env_len() {
    char** e = environ;
    if (e == nullptr) return 0;
    int n = 0;
    while (e[n] != nullptr) ++n;
    return n;
}
// Returns true if the cache is still valid for the current environment; refreshes the stamp.
inline bool wl_env_stamp_ok() {
    char** e = environ;
    const int n = wl_env_len();
    if (e == g_envPtr && n == g_envLen) return true;
    g_envPtr = e; g_envLen = n; g_envCount = 0;   // environment moved -> drop everything
    return false;
}

// Does environ[i] start with "name="? Returns the value pointer, or null.
inline const char* wl_env_match(int i, const char* name) {
    char** e = environ;
    if (e == nullptr || i < 0) return nullptr;
    // walk to entry i without calling strlen on the whole array
    for (int k = 0; k < i; ++k) { if (e[k] == nullptr) return nullptr; }
    const char* entry = e[i];
    if (entry == nullptr) return nullptr;
    const char* n = name;
    while (*n != '\0' && *entry == *n) { ++entry; ++n; }
    return (*n == '\0' && *entry == '=') ? entry + 1 : nullptr;
}
}  // namespace

extern "C" char* getenv(const char* name) {
    if (name == nullptr || name[0] == '\0') return nullptr;
    const bool fresh = wl_env_stamp_ok();               // also clears the cache if environ moved
    if (fresh) {
        for (int s = 0; s < g_envCount; ++s) {
            if (g_env[s].name == name) {                // pointer compare: libart passes literals
                if (g_env[s].idx < 0) return nullptr;   // cached ABSENT, valid for this stamp
                const char* v = wl_env_match(g_env[s].idx, name);
                if (v != nullptr) return const_cast<char*>(v);
                break;                                   // moved within an unchanged environ -> rescan
            }
        }
    }
    // full scan
    char** e = environ;
    if (e != nullptr) {
        for (int i = 0; e[i] != nullptr; ++i) {
            const char* entry = e[i];
            const char* n = name;
            while (*n != '\0' && *entry == *n) { ++entry; ++n; }
            if (*n == '\0' && *entry == '=') {
                if (g_envCount < kEnvSlots) {           // remember WHERE, never WHAT
                    g_env[g_envCount].name = name;
                    g_env[g_envCount].idx = i;
                    ++g_envCount;
                }
                return const_cast<char*>(entry + 1);
            }
        }
    }
    if (g_envCount < kEnvSlots) {                        // remember ABSENT under the current stamp
        g_env[g_envCount].name = name;
        g_env[g_envCount].idx = -1;
        ++g_envCount;
    }
    return nullptr;
}
