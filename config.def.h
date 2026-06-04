/* xlibre default config — create config.h to override values */

#ifndef CONFIG_H
#define CONFIG_H

/* modifier to use for keybindings */
#define MOD Mod1Mask /* Alt */

/* default terminal and dmenu commands */
static const char *termcmd[] = { "xterm", NULL };
static const char *dmenucmd[] = { "dmenu_run", NULL };

/* keysyms for actions */
/* Alt+q -> kill focused window */
#define KEY_KILL    XK_q
/* Alt+Return -> spawn default terminal */
#define KEY_TERMINAL XK_Return
/* Alt+p -> spawn dmenu */
#define KEY_DMENU   XK_p
/* Alt+Space -> cycle layouts (master-stack, monocle) */
#define KEY_LAYOUT  XK_space
/* Alt+Down -> focus next window in current workspace */
#define KEY_FOCUS_NEXT XK_Down
/* Alt+Up -> focus previous window in current workspace */
#define KEY_FOCUS_PREV XK_Up
/* Alt+Right -> next workspace */
#define KEY_WS_NEXT XK_Right
/* Alt+Left -> previous workspace */
#define KEY_WS_PREV XK_Left

/* number of workspaces */
static const int workspace_count = 4;

/* master area factor (0.0 - 1.0) for master-stack layout */
static const float mfactor = 0.6f;

/* native display forcing */
static const int force_native_display = 1;
static const int native_screen_width = 1920;
static const int native_screen_height = 1080;

/* gaps and borders */
/* outer gap (pixels) between windows and screen edge */
static const int gap_outer = 12;
/* inner gap (pixels) between windows */
static const int gap_inner = 8;
/* window border width in pixels */
static const int borderpx = 2;
/* border colors (named color strings) */
static const char *col_focus = "#ff7b72";   /* focused window border */
static const char *col_unfocus = "#222222"; /* unfocused window border */


#endif /* CONFIG_H */
