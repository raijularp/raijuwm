#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include "config.h"
#include <X11/extensions/Xinerama.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct Client Client;
struct Client {
    Window win;
    int workspace;
    Client *next;
};

static Display *dpy;
static int screen;
static Window root;
static int current_workspace = 0;
static int suppress_unmap_notify = 0;
static Client *clients = NULL;
static Window focused = 0;
static unsigned long colpixel_focus = 0;
static unsigned long colpixel_unfocus = 0;
static XineramaScreenInfo *xinerama_screens = NULL;
static int xinerama_n = 1;

/* dragging state */
enum { DRAG_NONE = 0, DRAG_MOVE, DRAG_RESIZE };
static int drag_mode = DRAG_NONE;
static Window drag_win = 0;
static int drag_start_x = 0, drag_start_y = 0;
static int drag_win_x = 0, drag_win_y = 0, drag_win_w = 0, drag_win_h = 0;

/* EWMH/ICCCM atoms */
static Atom net_supported_atom, net_supporting_wm_check, net_wm_name_atom, net_client_list_atom, net_active_window_atom;
static Atom wm_protocols_atom, wm_delete_atom;

/* program restart flag and argv storage */
static volatile sig_atomic_t reload_req = 0;
static char **g_argv = NULL;

typedef enum { LAYOUT_MASTERSTACK, LAYOUT_MONOCLE } Layout;
static Layout layout = LAYOUT_MASTERSTACK;

void die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void arrange(void);
void set_focused(Window w);

void add_client(Window w) {
    Client *c = calloc(1, sizeof(Client));
    c->win = w;
    c->workspace = current_workspace;
    c->next = clients;
    clients = c;
}

Client *client_by_window(Window w) {
    for (Client *c = clients; c; c = c->next) {
        if (c->win == w) return c;
    }
    return NULL;
}

int count_workspace_clients(int ws) {
    int n = 0;
    for (Client *c = clients; c; c = c->next) {
        if (c->workspace == ws) n++;
    }
    return n;
}

Client *first_client_in_workspace(int ws) {
    for (Client *c = clients; c; c = c->next) {
        if (c->workspace == ws) return c;
    }
    return NULL;
}

Client *next_client_in_workspace(Client *c, int ws) {
    for (Client *n = c ? c->next : NULL; n; n = n->next) {
        if (n->workspace == ws) return n;
    }
    return NULL;
}

void focus_next_client(int dir) {
    int cnt = count_workspace_clients(current_workspace);
    if (cnt == 0) return;

    Client **list = calloc(cnt, sizeof(Client *));
    int i = 0;
    int current_index = 0;
    for (Client *c = clients; c; c = c->next) {
        if (c->workspace == current_workspace) {
            list[i] = c;
            if (c->win == focused) current_index = i;
            i++;
        }
    }

    int next_index = (cnt + current_index + (dir > 0 ? 1 : -1)) % cnt;
    Client *next = list[next_index];
    free(list);

    if (next) {
        set_focused(next->win);
        XSetInputFocus(dpy, next->win, RevertToPointerRoot, CurrentTime);
    }
}

void switch_workspace(int ws) {
    if (ws < 0 || ws >= workspace_count || ws == current_workspace) return;
    current_workspace = ws;

    suppress_unmap_notify = 1;
    for (Client *c = clients; c; c = c->next) {
        if (c->workspace == current_workspace) {
            XMapWindow(dpy, c->win);
        } else {
            XUnmapWindow(dpy, c->win);
        }
    }
    XSync(dpy, False);
    suppress_unmap_notify = 0;

    Client *first = first_client_in_workspace(current_workspace);
    if (first) {
        set_focused(first->win);
        XSetInputFocus(dpy, first->win, RevertToPointerRoot, CurrentTime);
    } else {
        set_focused(0);
    }

    arrange();
}

void remove_client(Window w) {
    Client **pc = &clients;
    while (*pc) {
        if ((*pc)->win == w) {
            Client *tmp = *pc;
            *pc = tmp->next;
            free(tmp);
            return;
        }
        pc = &(*pc)->next;
    }
}

int count_clients() {
    int n = 0;
    for (Client *c = clients; c; c = c->next) n++;
    return n;
}

void arrange() {
    int n = count_workspace_clients(current_workspace);
    if (n == 0) return;

    /* refresh screen info each arrange to handle hotplug */
    if (!force_native_display && XineramaIsActive(dpy)) {
        if (xinerama_screens) XFree(xinerama_screens);
        xinerama_screens = XineramaQueryScreens(dpy, &xinerama_n);
        if (xinerama_n <= 0) xinerama_n = 1;
    } else {
        if (xinerama_screens) { XFree(xinerama_screens); xinerama_screens = NULL; }
        xinerama_n = 1;
    }

    /* distribute clients across Xinerama screens */
    int total = n;
    int per = total / xinerama_n;
    int rem = total % xinerama_n;
    Client *cur = NULL;
    for (Client *c = clients; c; c = c->next) {
        if (c->workspace == current_workspace) {
            cur = c;
            break;
        }
    }
    for (int si = 0; si < xinerama_n && cur; si++) {
        int this_count = per + (si < rem ? 1 : 0);
        int sw, sh, sx, sy;
        if (force_native_display) {
            sw = native_screen_width;
            sh = native_screen_height;
            sx = 0;
            sy = 0;
        } else if (xinerama_screens) {
            sw = xinerama_screens[si].width;
            sh = xinerama_screens[si].height;
            sx = xinerama_screens[si].x_org;
            sy = xinerama_screens[si].y_org;
        } else {
            sw = DisplayWidth(dpy, screen);
            sh = DisplayHeight(dpy, screen);
            sx = 0; sy = 0;
        }

        if (this_count == 0) continue;

        /* handle monocle per-screen */
        if (layout == LAYOUT_MONOCLE) {
            int x = sx + gap_outer, y = sy + gap_outer;
            int w = sw - 2 * gap_outer;
            int h = sh - 2 * gap_outer;
            for (int k = 0; k < this_count && cur; k++, cur = next_client_in_workspace(cur, current_workspace)) {
                XMoveResizeWindow(dpy, cur->win, x, y, w, h);
            }
            continue;
        }

        /* master-stack on this screen */
        int usable_w = sw - 2 * gap_outer;
        int usable_h = sh - 2 * gap_outer;
        int master_w = (this_count == 1) ? usable_w : (int)(usable_w * mfactor);
        int stack_w = usable_w - master_w;

        if (cur) {
            XMoveResizeWindow(dpy, cur->win, sx + gap_outer, sy + gap_outer, master_w, usable_h);
            cur = next_client_in_workspace(cur, current_workspace);
        }

        int stack_n = this_count - 1;
        int y = sy + gap_outer;
        for (int k = 0; k < stack_n && cur; k++, cur = next_client_in_workspace(cur, current_workspace)) {
            int h = (stack_n > 0) ? (usable_h / stack_n) : usable_h;
            XMoveResizeWindow(dpy, cur->win, sx + gap_outer + master_w + gap_inner, y, stack_w - gap_inner, h - gap_inner);
            y += h;
        }
    }
    XSync(dpy, False);
}

void set_focused(Window w) {
    focused = w;
    for (Client *c = clients; c; c = c->next) {
        unsigned long col = (c->win == focused) ? colpixel_focus : colpixel_unfocus;
        XSetWindowBorder(dpy, c->win, col);
    }
}

static void handle_sighup(int sig) {
    (void)sig;
    reload_req = 1;
}

void cycle_layout() {
    layout = (layout == LAYOUT_MASTERSTACK) ? LAYOUT_MONOCLE : LAYOUT_MASTERSTACK;
    arrange();
}

void manage(Window w) {
    XWindowAttributes wa;
    if (!XGetWindowAttributes(dpy, w, &wa) || wa.override_redirect)
        return;
    add_client(w);
    XSelectInput(dpy, w, StructureNotifyMask | PropertyChangeMask);
    XMapWindow(dpy, w);
    XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
    XSetWindowBorderWidth(dpy, w, borderpx);
    set_focused(w);
    arrange();
}

void spawn(const char *const cmd[]) {
    if (fork() == 0) {
        setsid();
        execvp(cmd[0], (char *const *)cmd);
        _exit(0);
    }
}

int xerror(Display *d, XErrorEvent *e) { return 0; }
int xerrorstart(Display *d, XErrorEvent *e) {
    die("Another window manager is already running");
    return 0;
}

int main(int argc, char **argv) {
    dpy = XOpenDisplay(NULL);
    if (!dpy) die("Failed to open display");
    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    /* allocate configured colors */
    {
        Colormap cmap = DefaultColormap(dpy, screen);
        XColor xc, exact;
        if (!XAllocNamedColor(dpy, cmap, col_focus, &xc, &exact))
            colpixel_focus = BlackPixel(dpy, screen);
        else
            colpixel_focus = xc.pixel;
        if (!XAllocNamedColor(dpy, cmap, col_unfocus, &xc, &exact))
            colpixel_unfocus = WhitePixel(dpy, screen);
        else
            colpixel_unfocus = xc.pixel;
    }

    /* setup EWMH atoms and supporting window */
    net_supported_atom = XInternAtom(dpy, "_NET_SUPPORTED", False);
    net_supporting_wm_check = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    net_wm_name_atom = XInternAtom(dpy, "_NET_WM_NAME", False);
    net_client_list_atom = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    net_active_window_atom = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    wm_protocols_atom = XInternAtom(dpy, "WM_PROTOCOLS", False);
    wm_delete_atom = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

    /* create a supporting window and set _NET_SUPPORTING_WM_CHECK */
    Window checkwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
    Atom utf8 = XInternAtom(dpy, "UTF8_STRING", False);
    const Atom supatoms[] = { net_supporting_wm_check, net_wm_name_atom, net_client_list_atom, net_active_window_atom };
    XChangeProperty(dpy, root, net_supported_atom, XA_ATOM, 32, PropModeReplace, (unsigned char *)supatoms, sizeof(supatoms)/sizeof(Atom));
    XChangeProperty(dpy, root, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&checkwin, 1);
    XChangeProperty(dpy, checkwin, net_supporting_wm_check, XA_WINDOW, 32, PropModeReplace, (unsigned char *)&checkwin, 1);
    const char *name = "raijuwm";
    XChangeProperty(dpy, checkwin, net_wm_name_atom, utf8, 8, PropModeReplace, (unsigned char *)name, strlen(name));

    XSetErrorHandler(xerrorstart);
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);
    XSync(dpy, False);
    XSetErrorHandler(xerror);

    /* grab keys (patchable via config.h) */
    KeyCode kc_kill = XKeysymToKeycode(dpy, KEY_KILL);
    KeyCode kc_term = XKeysymToKeycode(dpy, KEY_TERMINAL);
    KeyCode kc_dmenu = XKeysymToKeycode(dpy, KEY_DMENU);
    KeyCode kc_layout = XKeysymToKeycode(dpy, KEY_LAYOUT);
    KeyCode kc_focus_next = XKeysymToKeycode(dpy, KEY_FOCUS_NEXT);
    KeyCode kc_focus_prev = XKeysymToKeycode(dpy, KEY_FOCUS_PREV);
    KeyCode kc_ws_next = XKeysymToKeycode(dpy, KEY_WS_NEXT);
    KeyCode kc_ws_prev = XKeysymToKeycode(dpy, KEY_WS_PREV);
    XGrabKey(dpy, kc_kill, MOD, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, kc_term, MOD, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, kc_dmenu, MOD, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, kc_layout, MOD, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, kc_focus_next, MOD, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, kc_focus_prev, MOD, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, kc_ws_next, MOD, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, kc_ws_prev, MOD, root, True, GrabModeAsync, GrabModeAsync);

    /* grab mouse buttons for moving/resizing (Mod + Button1/Button3) */
    XGrabButton(dpy, Button1, MOD, root, True, ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, Button3, MOD, root, True, ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);

    /* spawn the status bar */
    spawn((const char *const[]){"./bar", NULL});

    /* setup SIGHUP handler for restart */
    g_argv = argv;
    signal(SIGHUP, handle_sighup);

    XEvent ev;
    while (1) {
        if (reload_req) {
            XCloseDisplay(dpy);
            execv(g_argv[0], g_argv);
        }
        XNextEvent(dpy, &ev);
        switch (ev.type) {
        case MapRequest: {
            XMapRequestEvent *e = &ev.xmaprequest;
            manage(e->window);
        } break;
        case DestroyNotify: {
            XDestroyWindowEvent *e = &ev.xdestroywindow;
            remove_client(e->window);
            set_focused(clients ? clients->win : 0);
            arrange();
        } break;
        case ConfigureRequest: {
            XConfigureRequestEvent *e = &ev.xconfigurerequest;
            XWindowChanges wc;
            wc.x = e->x;
            wc.y = e->y;
            wc.width = e->width;
            wc.height = e->height;
            wc.border_width = e->border_width;
            wc.sibling = e->above;
            wc.stack_mode = e->detail;
            XConfigureWindow(dpy, e->window, e->value_mask, &wc);
        } break;
        case ButtonPress: {
            XButtonEvent *e = &ev.xbutton;
            /* focus clicked window */
            if (e->subwindow) {
                set_focused(e->subwindow);
            }
            if ((e->state & MOD) && e->button == Button1) {
                /* begin move */
                Window fok = focused;
                if (fok) {
                    XWindowAttributes wa;
                    XGetWindowAttributes(dpy, fok, &wa);
                    drag_mode = DRAG_MOVE;
                    drag_win = fok;
                    drag_start_x = e->x_root;
                    drag_start_y = e->y_root;
                    drag_win_x = wa.x;
                    drag_win_y = wa.y;
                    drag_win_w = wa.width;
                    drag_win_h = wa.height;
                    XGrabPointer(dpy, root, False, PointerMotionMask|ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
                }
            }
            if ((e->state & MOD) && e->button == Button3) {
                /* begin resize */
                Window fok = focused;
                if (fok) {
                    XWindowAttributes wa;
                    XGetWindowAttributes(dpy, fok, &wa);
                    drag_mode = DRAG_RESIZE;
                    drag_win = fok;
                    drag_start_x = e->x_root;
                    drag_start_y = e->y_root;
                    drag_win_x = wa.x;
                    drag_win_y = wa.y;
                    drag_win_w = wa.width;
                    drag_win_h = wa.height;
                    XGrabPointer(dpy, root, False, PointerMotionMask|ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
                }
            }
        } break;
        case MotionNotify: {
            XMotionEvent *e = &ev.xmotion;
            if (drag_mode != DRAG_NONE && drag_win) {
                int dx = e->x_root - drag_start_x;
                int dy = e->y_root - drag_start_y;
                if (drag_mode == DRAG_MOVE) {
                    int nx = drag_win_x + dx;
                    int ny = drag_win_y + dy;
                    XMoveWindow(dpy, drag_win, nx, ny);
                } else if (drag_mode == DRAG_RESIZE) {
                    int nw = drag_win_w + dx;
                    int nh = drag_win_h + dy;
                    if (nw < 1) nw = 1;
                    if (nh < 1) nh = 1;
                    XResizeWindow(dpy, drag_win, nw, nh);
                }
            }
        } break;
        case ButtonRelease: {
            XButtonEvent *e = &ev.xbutton;
            if (drag_mode != DRAG_NONE) {
                drag_mode = DRAG_NONE;
                drag_win = 0;
                XUngrabPointer(dpy, CurrentTime);
                arrange();
            }
        } break;
        case ClientMessage: {
            XClientMessageEvent *e = &ev.xclient;
            if (e->message_type == wm_protocols_atom && (Atom)e->data.l[0] == wm_delete_atom) {
                XKillClient(dpy, e->window);
                remove_client(e->window);
                arrange();
            }
        } break;

        case KeyPress: {

            XKeyEvent *e = &ev.xkey;
            KeySym ks = XKeycodeToKeysym(dpy, e->keycode, 0);
            if ((e->state & MOD) && ks == KEY_KILL) {
                Window fok = focused;
                if (fok == None || fok == root) {
                    if (clients) fok = clients->win;
                }
                if (fok != None && fok != root) {
                    XKillClient(dpy, fok);
                    remove_client(fok);
                    if (clients) {
                        Client *next = first_client_in_workspace(current_workspace);
                        if (next) {
                            set_focused(next->win);
                            XSetInputFocus(dpy, next->win, RevertToPointerRoot, CurrentTime);
                        }
                    } else {
                        set_focused(0);
                    }
                    arrange();
                }
            }
            if ((e->state & MOD) && ks == KEY_LAYOUT) {
                cycle_layout();
            }
            if ((e->state & MOD) && ks == KEY_TERMINAL) {
                spawn(termcmd);
            }
            if ((e->state & MOD) && ks == KEY_DMENU) {
                spawn(dmenucmd);
            }
            if ((e->state & MOD) && ks == KEY_FOCUS_NEXT) {
                focus_next_client(1);
            }
            if ((e->state & MOD) && ks == KEY_FOCUS_PREV) {
                focus_next_client(-1);
            }
            if ((e->state & MOD) && ks == KEY_WS_NEXT) {
                switch_workspace((current_workspace + 1) % workspace_count);
            }
            if ((e->state & MOD) && ks == KEY_WS_PREV) {
                switch_workspace((current_workspace - 1 + workspace_count) % workspace_count);
            }
        } break;
        case UnmapNotify: {
            if (suppress_unmap_notify) break;
            XUnmapEvent *e = &ev.xunmap;
            remove_client(e->window);
            set_focused(clients ? clients->win : 0);
            arrange();
        } break;
        default:
            break;
        }
    }

    return 0;
}
