#include <X11/Xlib.h>
#include <X11/extensions/Xinerama.h>
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int main(void) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) return 1;
    int screen = DefaultScreen(dpy);
    int nscr = 1;
    XineramaScreenInfo *screens = NULL;
    if (!force_native_display && XineramaIsActive(dpy)) {
        screens = XineramaQueryScreens(dpy, &nscr);
        if (!screens) nscr = 1;
    }

    Window *bars = calloc(nscr, sizeof(Window));
    GC *gcs = calloc(nscr, sizeof(GC));

    for (int i = 0; i < nscr; i++) {
        int x = 0, y = 0, w = DisplayWidth(dpy, screen), h = 24;
        if (force_native_display) {
            x = 0;
            y = 0;
            w = native_screen_width;
        } else if (screens) {
            x = screens[i].x_org;
            y = screens[i].y_org;
            w = screens[i].width;
        }
        Window root = RootWindow(dpy, screen);
        Window bw = XCreateSimpleWindow(dpy, root, x, y, w, h, 0, 0, 0x222222);
        XSetWindowAttributes wa;
        wa.override_redirect = True;
        XChangeWindowAttributes(dpy, bw, CWOverrideRedirect, &wa);
        XMapWindow(dpy, bw);
        GC gc = XCreateGC(dpy, bw, 0, NULL);
        XSetForeground(dpy, gc, WhitePixel(dpy, screen));
        bars[i] = bw;
        gcs[i] = gc;
    }

    char buf[256];
    while (1) {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        snprintf(buf, sizeof(buf), "raijuwm — %02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
        for (int i = 0; i < nscr; i++) {
            XClearWindow(dpy, bars[i]);
            XDrawString(dpy, bars[i], gcs[i], 8, 16, buf, strlen(buf));
        }
        XFlush(dpy);
        sleep(1);
    }

    return 0;
}
