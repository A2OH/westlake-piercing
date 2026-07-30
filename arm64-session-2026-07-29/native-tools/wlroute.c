/* wlroute — add a default IPv4 route to the MAIN table.
 * The board has no ip/route binary; OHOS uses policy routing and our adapter app's uid was never
 * added to the default network's uid ranges (it is installed via the .app->.apk hack, outside
 * OHOS's normal flow), so its unmarked sockets get ENETUNREACH ("network is unreachable").
 * usage: wlroute <gateway> <iface>            e.g. wlroute 10.145.202.1 wlan0 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/route.h>

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <gateway> <iface>\n", argv[0]); return 2; }
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) { perror("socket"); return 1; }
    struct rtentry rt;
    memset(&rt, 0, sizeof(rt));
    struct sockaddr_in* dst = (struct sockaddr_in*)&rt.rt_dst;
    struct sockaddr_in* gw  = (struct sockaddr_in*)&rt.rt_gateway;
    struct sockaddr_in* msk = (struct sockaddr_in*)&rt.rt_genmask;
    dst->sin_family = AF_INET; dst->sin_addr.s_addr = 0;
    msk->sin_family = AF_INET; msk->sin_addr.s_addr = 0;
    gw->sin_family  = AF_INET; gw->sin_addr.s_addr = inet_addr(argv[1]);
    rt.rt_flags = RTF_UP | RTF_GATEWAY;
    rt.rt_dev = argv[2];
    if (ioctl(s, SIOCADDRT, &rt) < 0) {
        fprintf(stderr, "SIOCADDRT: %s (errno=%d)\n", strerror(errno), errno);
        close(s); return 1;
    }
    printf("default via %s dev %s added to main table\n", argv[1], argv[2]);
    close(s);
    return 0;
}
