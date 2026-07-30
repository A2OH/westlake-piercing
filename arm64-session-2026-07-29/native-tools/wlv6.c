/* wlv6 <uid> — reproduce what Java/libcore actually does: an AF_INET6 dual-stack socket
 * connecting to an IPv4-mapped address (::ffff:a.b.c.d). That is how Android's IoBridge
 * connects, and it needs different routing from a plain AF_INET socket. */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static void try6(const char* label, const char* addr6) {
    int s = socket(AF_INET6, SOCK_STREAM, 0);
    if (s < 0) { printf("  %-28s socket(AF_INET6) FAILED: %s\n", label, strerror(errno)); return; }
    int off = 0;
    setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    struct sockaddr_in6 a; memset(&a, 0, sizeof(a));
    a.sin6_family = AF_INET6; a.sin6_port = htons(443);
    inet_pton(AF_INET6, addr6, &a.sin6_addr);
    if (connect(s, (struct sockaddr*)&a, sizeof(a)) < 0)
        printf("  %-28s connect FAILED: %s (errno=%d)\n", label, strerror(errno), errno);
    else
        printf("  %-28s connect OK\n", label);
    close(s);
}

int main(int argc, char** argv) {
    uid_t uid = (uid_t)(argc > 1 ? atoi(argv[1]) : 0);
    if (uid != 0) { setgid(uid); if (setuid(uid) < 0) { printf("setuid: %s\n", strerror(errno)); return 1; } }
    printf("uid=%d\n", (int)getuid());
    try6("v4-mapped ::ffff:35.94.160.101", "::ffff:35.94.160.101");
    try6("global v6 (google dns)", "2001:4860:4860::8888");
    return 0;
}
