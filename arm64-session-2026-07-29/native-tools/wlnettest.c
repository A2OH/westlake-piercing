/* wlnettest <uid> [gid1 gid2 ...] — drop to a uid and report exactly why its sockets fail.
 * Distinguishes the two candidate causes: EPERM = OHOS netsys eBPF socket deny,
 * ENETUNREACH = policy routing (uid not in the default network's uid ranges). */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <grp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <uid> [gids...]\n", argv[0]); return 2; }
    uid_t uid = (uid_t)atoi(argv[1]);
    gid_t gids[8]; int ng = 0;
    for (int i = 2; i < argc && ng < 8; ++i) gids[ng++] = (gid_t)atoi(argv[i]);
    if (ng > 0 && setgroups(ng, gids) < 0) printf("setgroups: %s\n", strerror(errno));
    if (setgid(uid) < 0) printf("setgid: %s\n", strerror(errno));
    if (setuid(uid) < 0) { printf("setuid failed: %s\n", strerror(errno)); return 1; }
    printf("running as uid=%d\n", (int)getuid());

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { printf("socket() FAILED: %s (errno=%d)\n", strerror(errno), errno); return 1; }
    printf("socket() ok\n");
    struct sockaddr_in a; memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET; a.sin_port = htons(53); a.sin_addr.s_addr = inet_addr("8.8.8.8");
    if (connect(s, (struct sockaddr*)&a, sizeof(a)) < 0)
        printf("connect(8.8.8.8:53) FAILED: %s (errno=%d)\n", strerror(errno), errno);
    else
        printf("connect(8.8.8.8:53) OK\n");
    close(s);
    return 0;
}
