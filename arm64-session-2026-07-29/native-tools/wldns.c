/* wldns <uid> <host> — resolve a host and try connecting to EVERY returned address as <uid>.
 * Catches the two remaining candidates for "Network is unreachable": DNS failure, and an
 * AAAA/IPv6 address with no global IPv6 route (the board only has a link-local v6 address). */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <uid> <host>\n", argv[0]); return 2; }
    uid_t uid = (uid_t)atoi(argv[1]);
    if (uid != 0) { setgid(uid); if (setuid(uid) < 0) { printf("setuid: %s\n", strerror(errno)); return 1; } }
    printf("uid=%d resolving %s\n", (int)getuid(), argv[2]);
    struct addrinfo hints, *res = NULL, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    int rc = getaddrinfo(argv[2], "443", &hints, &res);
    if (rc != 0) { printf("getaddrinfo FAILED: %s (rc=%d errno=%s)\n", gai_strerror(rc), rc, strerror(errno)); return 1; }
    for (p = res; p; p = p->ai_next) {
        char ip[64] = "?";
        if (p->ai_family == AF_INET)
            inet_ntop(AF_INET, &((struct sockaddr_in*)p->ai_addr)->sin_addr, ip, sizeof(ip));
        else if (p->ai_family == AF_INET6)
            inet_ntop(AF_INET6, &((struct sockaddr_in6*)p->ai_addr)->sin6_addr, ip, sizeof(ip));
        int s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s < 0) { printf("  %-40s socket FAILED %s\n", ip, strerror(errno)); continue; }
        if (connect(s, p->ai_addr, p->ai_addrlen) < 0)
            printf("  %-40s (%s) connect FAILED: %s\n", ip, p->ai_family == AF_INET6 ? "v6" : "v4", strerror(errno));
        else
            printf("  %-40s (%s) connect OK\n", ip, p->ai_family == AF_INET6 ? "v6" : "v4");
        close(s);
    }
    freeaddrinfo(res);
    return 0;
}
