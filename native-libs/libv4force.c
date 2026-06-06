/* 2026-06-06 connectivity fix: the OHOS adapter JVM (libcore) creates dual-stack
 * AF_INET6 TCP sockets and connects to IPv4-mapped addresses (::ffff:a.b.c.d).
 * This device has NO IPv6 route, so that connect fails -> every live okhttp/HTTPS
 * request dies ("network unreachable"), while pure-AF_INET (libc) connects work.
 * Force TCP sockets to AF_INET and rewrite IPv4-mapped connect targets to
 * sockaddr_in, so the JVM's connections use the working IPv4 path. */
typedef unsigned int socklen_t;
struct sockaddr { unsigned short sa_family; char sa_data[26]; };
struct in_addr { unsigned int s_addr; };
struct sockaddr_in { unsigned short sin_family; unsigned short sin_port; struct in_addr sin_addr; unsigned char pad[16]; };
struct in6_addr { unsigned char s6_addr[16]; };
struct sockaddr_in6 { unsigned short sin6_family; unsigned short sin6_port; unsigned int sin6_flowinfo; struct in6_addr sin6_addr; unsigned int sin6_scope_id; };
#define AF_INET 2
#define AF_INET6 10
#define SOCK_STREAM 1
#define IPPROTO_IPV6 41
#define RTLD_NEXT ((void*)-1L)
extern void* dlsym(void* handle, const char* symbol);

typedef int (*socket_fn)(int,int,int);
typedef int (*connect_fn)(int,const struct sockaddr*,socklen_t);
typedef int (*setsockopt_fn)(int,int,int,const void*,socklen_t);
static socket_fn r_socket; static connect_fn r_connect; static setsockopt_fn r_setsockopt;

__attribute__((visibility("default")))
int socket(int domain, int type, int protocol) {
    if (!r_socket) r_socket = (socket_fn)dlsym(RTLD_NEXT, "socket");
    if (domain == AF_INET6 && (type & 0xf) == SOCK_STREAM) domain = AF_INET;  /* force IPv4 TCP */
    return r_socket(domain, type, protocol);
}

__attribute__((visibility("default")))
int connect(int fd, const struct sockaddr* addr, socklen_t len) {
    if (!r_connect) r_connect = (connect_fn)dlsym(RTLD_NEXT, "connect");
    if (addr && addr->sa_family == AF_INET6 && len >= (socklen_t)sizeof(struct sockaddr_in6)) {
        const struct sockaddr_in6* a6 = (const struct sockaddr_in6*)addr;
        const unsigned char* b = a6->sin6_addr.s6_addr;
        int mapped = !b[0]&&!b[1]&&!b[2]&&!b[3]&&!b[4]&&!b[5]&&!b[6]&&!b[7]&&!b[8]&&!b[9]&&b[10]==0xff&&b[11]==0xff;
        if (mapped) {
            struct sockaddr_in a4; unsigned char* p = (unsigned char*)&a4; int i;
            for (i = 0; i < (int)sizeof(a4); i++) p[i] = 0;
            a4.sin_family = AF_INET;
            a4.sin_port = a6->sin6_port;
            a4.sin_addr.s_addr = *(const unsigned int*)(b + 12);
            return r_connect(fd, (const struct sockaddr*)&a4, (socklen_t)sizeof(a4));
        }
    }
    return r_connect(fd, addr, len);
}

__attribute__((visibility("default")))
int setsockopt(int fd, int level, int optname, const void* val, socklen_t len) {
    if (!r_setsockopt) r_setsockopt = (setsockopt_fn)dlsym(RTLD_NEXT, "setsockopt");
    if (level == IPPROTO_IPV6) return 0;  /* swallow IPv6 opts on forced-IPv4 sockets */
    return r_setsockopt(fd, level, optname, val, len);
}
