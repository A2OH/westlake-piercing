/* touchfwd — WESTLAKE §421: make the PHYSICAL touchscreen drive noice.
 *
 * Real touches never reach the app: its windows are bridge-created sub-windows that OHOS's MMI
 * InputWindowsManager never registered, and subscribeMmi is disabled in the child (it SIGBUSes).
 * So OHOS delivers pointer events to SceneBoard/launcher instead, and the panel feels dead.
 *
 * This reads the touchscreen evdev node directly and re-emits each gesture into the bridge's
 * in-process injection channel (/data/local/tmp/noice_tap), which IS delivered. Taps become
 * "x y"; anything that moved far enough becomes a drag "x1 y1 x2 y2" so scrolling/sliders work.
 *
 * usage: touchfwd [/dev/input/event5] [&]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>

#define TAP_CHANNEL "/data/local/tmp/noice_tap"
#define DRAG_THRESHOLD 48      /* px before a gesture counts as a drag rather than a tap */

static void emit(const char* s) {
    int fd = open(TAP_CHANNEL, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return;
    write(fd, s, strlen(s));
    close(fd);
    fprintf(stderr, "[touchfwd] %s", s);
    fflush(stderr);
}

int main(int argc, char** argv) {
    const char* dev = argc > 1 ? argv[1] : "/dev/input/event5";
    int fd = open(dev, O_RDONLY);
    if (fd < 0) { fprintf(stderr, "touchfwd: open %s: %s\n", dev, strerror(errno)); return 1; }
    fprintf(stderr, "[touchfwd] forwarding %s -> %s\n", dev, TAP_CHANNEL);
    fflush(stderr);

    int x = -1, y = -1, downX = -1, downY = -1, touching = 0, sawDown = 0;
    struct input_event ev;
    while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X) x = ev.value;
            else if (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y) y = ev.value;
            else if (ev.code == ABS_MT_TRACKING_ID) {
                if (ev.value == -1) touching = 0;
                else if (!touching) { touching = 1; sawDown = 1; downX = x; downY = y; }
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            if (ev.value) { touching = 1; sawDown = 1; downX = x; downY = y; }
            else touching = 0;
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
            if (touching && sawDown && downX < 0) { downX = x; downY = y; }
            if (!touching && sawDown && x >= 0 && y >= 0) {
                char buf[64];
                int dx = x - downX, dy = y - downY;
                if (dx < 0) dx = -dx;
                if (dy < 0) dy = -dy;
                if (downX >= 0 && (dx > DRAG_THRESHOLD || dy > DRAG_THRESHOLD))
                    snprintf(buf, sizeof(buf), "%d %d %d %d\n", downX, downY, x, y);
                else
                    snprintf(buf, sizeof(buf), "%d %d\n", x, y);
                emit(buf);
                sawDown = 0; downX = downY = -1;
                usleep(120 * 1000);   /* let the bridge consume it before the next gesture */
            }
        }
    }
    fprintf(stderr, "touchfwd: read ended: %s\n", strerror(errno));
    return 1;
}
