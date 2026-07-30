/* fontpin — WESTLAKE §420: stop font data being unmapped while HarfBuzz still points into it.
 *
 * PROVEN failure: the child SIGSEGVs (code=1 SEGV_MAPERR) inside libskia_canvaskit's HarfBuzz
 * table-getter, reached from minikin::LayoutPiece <- LayoutCache::getOrCreate. The faulting address
 * was EXACTLY the base of a /system/fonts/HarmonyOS_Sans.ttf mapping that /proc/<pid>/maps had
 * listed moments earlier -- i.e. a use-after-munmap, not a bad font or a wrong length (mappings are
 * correctly page-rounded, and the crash persists with only HarmonyOS_Sans in fonts.xml).
 *
 * Fix: interpose mmap/munmap. Remember every mapping whose fd resolves to a font file, and refuse
 * to unmap those. Leaks a few MB of read-only file pages, which is irrelevant here and much
 * cheaper than the crash.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define WL_MAX_PINNED 512

static struct { void* addr; size_t len; } g_pinned[WL_MAX_PINNED];
static int g_count;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static void* (*real_mmap)(void*, size_t, int, int, int, off_t);
static int   (*real_munmap)(void*, size_t);

static int fd_is_font(int fd) {
    if (fd < 0) return 0;
    char link[64], path[512];
    snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    ssize_t n = readlink(link, path, sizeof(path) - 1);
    if (n <= 0) return 0;
    path[n] = '\0';
    return strstr(path, "/fonts/") != NULL ||
           strstr(path, ".ttf") != NULL || strstr(path, ".TTF") != NULL ||
           strstr(path, ".otf") != NULL || strstr(path, ".ttc") != NULL;
}

void* mmap(void* addr, size_t len, int prot, int flags, int fd, off_t off) {
    if (!real_mmap) real_mmap = (void* (*)(void*, size_t, int, int, int, off_t))dlsym(RTLD_NEXT, "mmap");
    void* r = real_mmap(addr, len, prot, flags, fd, off);
    if (r != MAP_FAILED && fd >= 0 && fd_is_font(fd)) {
        pthread_mutex_lock(&g_lock);
        if (g_count < WL_MAX_PINNED) {
            g_pinned[g_count].addr = r;
            g_pinned[g_count].len = len;
            g_count++;
        }
        pthread_mutex_unlock(&g_lock);
    }
    return r;
}

/* musl's <sys/mman.h> already #defines mmap64 to mmap, so interposing mmap covers both. */

int munmap(void* addr, size_t len) {
    if (!real_munmap) real_munmap = (int (*)(void*, size_t))dlsym(RTLD_NEXT, "munmap");
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_count; ++i) {
        /* Skip if this unmap touches a pinned font range at all, not just an exact match. */
        char* p = (char*)g_pinned[i].addr;
        char* q = (char*)addr;
        if (q < p + g_pinned[i].len && p < q + len) {
            pthread_mutex_unlock(&g_lock);
            return 0;   /* pretend success; keep the pages mapped */
        }
    }
    pthread_mutex_unlock(&g_lock);
    return real_munmap(addr, len);
}


/* ------------------------------------------------------------------
 * WESTLAKE §422 — network syscall tracing.
 * noice reports "The network is unreachable" but /proc/net/tcp never shows a socket for its uid,
 * even though a probe running as that same uid resolves api.trynoice.com and connects. Java stack
 * traces are empty in this runtime, so trace the syscalls themselves to see what the app really
 * attempts and with what errno.
 * ------------------------------------------------------------------ */
static int (*real_socket)(int, int, int);
static int (*real_connect)(int, const struct sockaddr*, socklen_t);
static int (*real_getaddrinfo)(const char*, const char*, const struct addrinfo*, struct addrinfo**);

int socket(int domain, int type, int protocol) {
    if (!real_socket) real_socket = (int (*)(int, int, int))dlsym(RTLD_NEXT, "socket");
    int fd = real_socket(domain, type, protocol);
    if (domain == AF_INET || domain == AF_INET6) {
        fprintf(stderr, "[NETTRACE] socket(%s,%d) = %d%s\n",
                domain == AF_INET ? "AF_INET" : "AF_INET6", type, fd,
                fd < 0 ? strerror(errno) : "");
        fflush(stderr);
    }
    return fd;
}

int connect(int fd, const struct sockaddr* addr, socklen_t len) {
    if (!real_connect) real_connect =
        (int (*)(int, const struct sockaddr*, socklen_t))dlsym(RTLD_NEXT, "connect");
    char ip[64] = "?";
    int port = 0;
    if (addr && addr->sa_family == AF_INET) {
        inet_ntop(AF_INET, &((struct sockaddr_in*)addr)->sin_addr, ip, sizeof(ip));
        port = ntohs(((struct sockaddr_in*)addr)->sin_port);
    } else if (addr && addr->sa_family == AF_INET6) {
        inet_ntop(AF_INET6, &((struct sockaddr_in6*)addr)->sin6_addr, ip, sizeof(ip));
        port = ntohs(((struct sockaddr_in6*)addr)->sin6_port);
    }
    int r = real_connect(fd, addr, len);
    if (addr && (addr->sa_family == AF_INET || addr->sa_family == AF_INET6)) {
        fprintf(stderr, "[NETTRACE] connect(fd=%d, %s:%d) = %d %s\n",
                fd, ip, port, r, r < 0 ? strerror(errno) : "OK");
        fflush(stderr);
    }
    return r;
}

int getaddrinfo(const char* node, const char* service,
                const struct addrinfo* hints, struct addrinfo** res) {
    if (!real_getaddrinfo) real_getaddrinfo =
        (int (*)(const char*, const char*, const struct addrinfo*, struct addrinfo**))
        dlsym(RTLD_NEXT, "getaddrinfo");
    int rc = real_getaddrinfo(node, service, hints, res);
    fprintf(stderr, "[NETTRACE] getaddrinfo(%s,%s) = %d %s\n",
            node ? node : "(null)", service ? service : "(null)", rc,
            rc == 0 ? "OK" : gai_strerror(rc));
    fflush(stderr);
    return rc;
}
