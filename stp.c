#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "stp.h"
#include "config.h"

#define DEBUG printf("%s:\t%d\n", __FUNCTION__, __LINE__)

Display *dd;
Window root_win;
GC fore_gc;
taskbar tb;
int scr_screen;
int scr_depth;
int scr_width;
int scr_height;
int text_y;
int pager_size;
unsigned int tag_mask;
static char stext[256] = "";

XftDraw *xftdraw;
XftFont *xfs;
XGlyphInfo *extents;

struct colors {
    unsigned short red, green, blue;
} cols[] = {
    {0xd75c, 0xd75c, 0xd75c},         /* 0. light gray */
    {0xbefb, 0xbaea, 0xbefb},         /* 1. mid gray */
    {0xaefb, 0xaaea, 0xaefb},         /* 2. dark gray */
    {0xefbe, 0xefbe, 0xefbe},         /* 3. white */
    {0x4000, 0x4000, 0x4000},         /* 4. darkest gray */
    {0x0000, 0x0000, 0x0000},         /* 5. black */
    {0x2000, 0x2000, 0x2000},
    {0x6000, 0x6000, 0x6000},
    {0x8000, 0x8000, 0x8000},
    {0xa000, 0xa000, 0xa000},
};

#define PALETTE_COUNT (sizeof(cols) / sizeof(cols[0].red) / 3)

unsigned long palette[PALETTE_COUNT];

char *atom_names[] = {
    /* clients */
    "WM_STATE",
    "_MOTIF_WM_HINTS",
    "_NET_WM_STATE",
    "_NET_WM_STATE_SKIP_TASKBAR",
    "_NET_WM_STATE_SHADED",
    "_NET_WM_DESKTOP",
    "_NET_WM_WINDOW_TYPE",
    "_NET_WM_WINDOW_TYPE_DOCK", /* 8 */
    "_NET_WM_STRUT",
    "_WIN_HINTS",
    /* root */
    "_NET_CLIENT_LIST",
    "_NET_NUMBER_OF_DESKTOPS",
    "_NET_CURRENT_DESKTOP",
};

#define ATOM_COUNT (sizeof(atom_names) / sizeof(atom_names[0]))

Atom atoms[ATOM_COUNT];
Atom netwmname;

#define atom_WM_STATE atoms[0]
#define atom__MOTIF_WM_HINTS atoms[1]
#define atom__NET_WM_STATE atoms[2]
#define atom__NET_WM_STATE_SKIP_TASKBAR atoms[3]
#define atom__NET_WM_STATE_SHADED atoms[4]
#define atom__NET_WM_DESKTOP atoms[5]
#define atom__NET_WM_WINDOW_TYPE atoms[6]
#define atom__NET_WM_WINDOW_TYPE_DOCK atoms[7]
#define atom__NET_WM_STRUT atoms[8]
#define atom__WIN_HINTS atoms[9]
#define atom__NET_CLIENT_LIST atoms[10]
#define atom__NET_NUMBER_OF_DESKTOPS atoms[11]
#define atom__NET_CURRENT_DESKTOP atoms[12]

void *get_prop_data(Window win, Atom prop, Atom type, int *items)
{
    Atom type_ret;
    int format_ret;
    unsigned long items_ret;
    unsigned long after_ret;
    unsigned char *prop_data;
    prop_data = 0;
    XGetWindowProperty(dd, win, prop, 0, 4096, False,
                        type, &type_ret, &format_ret, &items_ret,
                        &after_ret, &prop_data);

    if(items) {
        *items = items_ret;
    }

    return prop_data;
}

void inline set_foreground(int index)
{
    XSetForeground(dd, fore_gc, palette[index]);
}

void inline fill_rect(int x, int y, int a, int b)
{
    XFillRectangle(dd, tb.win, fore_gc, x, y, a, b);
}

int generic_get_int(Window win, Atom at)
{
    int num = 0;
    unsigned long *data;
    data = get_prop_data(win, at, XA_CARDINAL, 0);

    if(data) {
        num = *data;
        XFree(data);
    }

    return num;
}

int inline find_desktop(Window win)
{
    return generic_get_int(win, atom__NET_WM_DESKTOP);
}

int is_iconified(Window win)
{
    unsigned long *data;
    int ret = 0;
    data = get_prop_data(win, atom_WM_STATE, atom_WM_STATE, 0);

    if(data) {
        if(data[0] == IconicState) {
            ret = 1;
        }

        XFree(data);
    }

    return ret;
}

int inline get_current_desktop(void)
{
    return generic_get_int(root_win, atom__NET_CURRENT_DESKTOP);
}

int inline get_number_of_desktops(void)
{
    return generic_get_int(root_win, atom__NET_NUMBER_OF_DESKTOPS);
}

void add_task(Window win, int focus)
{
    task *tk, *list;

    if(win == tb.win) {
        return;
    }

    /* is this window on a different desktop? */
    if(tb.my_desktop != find_desktop(win)) {
        return;
    }

    tk = calloc(1, sizeof(task));
    tk->win = win;
    tk->focused = focus;

    XTextProperty prop;
    XGetTextProperty(dd, win, &prop, netwmname);
    tk->name = (char *)prop.value;

    tk->iconified = is_iconified(win);
    XSelectInput(dd, win, PropertyChangeMask | FocusChangeMask |
                  StructureNotifyMask);
    /* now append it to our linked list */
    tb.num_tasks++;
    list = tb.task_list;

    if(!list) {
        tb.task_list = tk;
        return;
    }

    while(1) {
        if(!list->next) {
            list->next = tk;
            return;
        }

        list = list->next;
    }
}

void inline gui_sync(void)
{
    XSync(dd, False);
}

void inline set_prop(Window win, Atom at, Atom type, long val)
{
    XChangeProperty(dd, win, at, type, 32,
                     PropModeReplace,(unsigned char *) &val, 1);
}

Window gui_create_taskbar(void)
{
    Window win;
    MWMHints mwm;
    XSizeHints size_hints;
    XWMHints wmhints;
    XSetWindowAttributes att;
    unsigned long strut[4];
    att.background_pixel = palette[0];
    att.event_mask = ButtonPressMask | ExposureMask;
    win = XCreateWindow(dd, root_win, 0, 0, WINWIDTH, WINHEIGHT,
            0, CopyFromParent, InputOutput, CopyFromParent,
            CWBackPixel | CWEventMask, &att);

    strut[0] = 0;
    strut[1] = 0;
    strut[2] = 0;
    strut[3] = WINHEIGHT;
    XChangeProperty(dd, win, atom__NET_WM_STRUT, XA_CARDINAL, 32,
                     PropModeReplace,(unsigned char *) strut, 4);
    /* reside on ALL desktops */
    set_prop(win, atom__NET_WM_DESKTOP, XA_CARDINAL, 0xFFFFFFFF);
    set_prop(win, atom__NET_WM_WINDOW_TYPE, XA_ATOM,
              atom__NET_WM_WINDOW_TYPE_DOCK);
    /* use old gnome hint since sawfish doesn't support _NET_WM_STRUT */
    set_prop(win, atom__WIN_HINTS, XA_CARDINAL,
              WIN_HINTS_SKIP_FOCUS | WIN_HINTS_SKIP_WINLIST |
              WIN_HINTS_SKIP_TASKBAR | WIN_HINTS_DO_NOT_COVER);
    /* borderless motif hint */
    bzero(&mwm, sizeof(mwm));
    mwm.flags = MWM_HINTS_DECORATIONS;
    XChangeProperty(dd, win, atom__MOTIF_WM_HINTS, atom__MOTIF_WM_HINTS, 32,
                     PropModeReplace,(unsigned char *) &mwm,
                     sizeof(MWMHints) / 4);
    /* make sure the WM obays our window position */
    size_hints.flags = PPosition;
    /*XSetWMNormalHints(dd, win, &size_hints);*/
    XChangeProperty(dd, win, XA_WM_NORMAL_HINTS, XA_WM_SIZE_HINTS, 32,
                     PropModeReplace,(unsigned char *) &size_hints,
                     sizeof(XSizeHints) / 4);
    /* make our window unfocusable */
    wmhints.flags = InputHint;
    wmhints.input = False;
    /*XSetWMHints(dd, win, &wmhints);*/
    XChangeProperty(dd, win, XA_WM_HINTS, XA_WM_HINTS, 32, PropModeReplace,
                    (unsigned char *) &wmhints, sizeof(XWMHints) / 4);
    XMapWindow(dd, win);
    xftdraw = XftDrawCreate(dd, win, DefaultVisual(dd, scr_screen),
                             DefaultColormap(dd, scr_screen));
    return win;
}

void *statusloop(void *unused)
{
    FILE *fp;
    while(1) {
        fp = popen(STATUS_BIN ,"r");
        fgets(stext , sizeof(stext) , fp);
        sleep(STATUS_DELAY);
        pclose(fp);
    }
}

void sigchld(int unused)
{
    if(signal(SIGCHLD, sigchld) == SIG_ERR) {
        exit(1);
    }

    while(0 < waitpid(-1, NULL, WNOHANG));
}

void gui_init(void)
{
    XGCValues gcv;
    XColor xcl;
    unsigned int i;
    i = 0;

    pthread_t status_thread;
    pthread_create(&status_thread, NULL, statusloop, NULL);

    sigchld(0); // dwm say it can clean up zombie process, seems good

    tag_mask = 0;

    do {
        xcl.red = cols[i].red;
        xcl.green = cols[i].green;
        xcl.blue = cols[i].blue;
        XAllocColor(dd, DefaultColormap(dd, scr_screen), &xcl);
        palette[i] = xcl.pixel;
        i++;
    } while(i < PALETTE_COUNT);

    xfs = XftFontOpenName(dd, scr_screen, XFT_FONT);
    extents = malloc(sizeof(XGlyphInfo));

    gcv.graphics_exposures = False;
    text_y = xfs->ascent +((WINHEIGHT -(xfs->ascent + xfs->descent)) / 2);
    fore_gc = XCreateGC(dd, root_win, GCGraphicsExposures, &gcv);
}

void inline draw_bigbox(int x, int width)
{
    set_foreground(4);
    fill_rect(x, 0, width, WINHEIGHT);
}

void inline draw_tinybox(int x, int width)
{
    set_foreground(1);
    fill_rect(x + 2, 2, width, width);
}

void inline default_status(void)
{
    if(stext[0] == '\0') {
    time_t curtime;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;
    strftime(stext, 256, "%Y-%m-%d %I:%M %P", localtime(&curtime));
    }
}

void draw_status(void)
{
    XftColor col;

    default_status();

    int len = strlen(stext);
    XftTextExtentsUtf8(dd, xfs,(const FcChar8 *)stext, len, extents);
	int stext_width = extents->xOff + 1;

    col.color.alpha = 0xffff;
    col.color.red = cols[1].red;
    col.color.green = cols[1].green;
    col.color.blue = cols[1].blue;

    XftDrawStringUtf8(xftdraw, &col, xfs, WINWIDTH - stext_width, text_y,(XftChar8 *)stext, len);
}

void gui_draw_task(task *tk)
{
    int len;
    int x = tk->pos_x;
    int taskw = tk->width;
    XGlyphInfo ext;
    XftColor col;

    if(!tk->name) {
        return;
    }

    if(tk->focused) {
        draw_bigbox(x, taskw);
    } else {
        set_foreground(6);
        fill_rect(x, 0, taskw, WINHEIGHT);
    }

    register int text_x = x + 2;
    /* check how many chars can fit */
    len = strlen(tk->name);

    while(len > 0) {
        XftTextExtentsUtf8(dd, xfs,(const XftChar8 *)tk->name, len, &ext);
        if(ext.width < taskw -(text_x - x) - 1) {
            break;
        }
        len--;
    }

    col.color.alpha = 0xffff;

    if(tk->iconified) {
        col.color.red = cols[2].red;
        col.color.green = cols[2].green;
        col.color.blue = cols[2].blue;
    } else {
        col.color.red = cols[1].red;
        col.color.green = cols[1].green;
        col.color.blue = cols[1].blue;
    }

    /* draw task's name here */
    XftDrawStringUtf8(xftdraw, &col, xfs, text_x, text_y,(XftChar8 *)tk->name, len);
}

void toggle_shade(Window win)
{
    XClientMessageEvent xev;
    xev.type = ClientMessage;
    xev.window = win;
    xev.message_type = atom__NET_WM_STATE;
    xev.format = 32;
    xev.data.l[0] = 2;  /* toggle */
    xev.data.l[1] = atom__NET_WM_STATE_SHADED;
    xev.data.l[2] = 0;
    XSendEvent(dd, root_win, False, SubstructureNotifyMask,(XEvent *) &xev);
}

void switch_desk(int new_desk)
{
    XClientMessageEvent xev;

    if(get_number_of_desktops() <= new_desk) {
        return;
    }

    xev.type = ClientMessage;
    xev.window = root_win;
    xev.message_type = atom__NET_CURRENT_DESKTOP;
    xev.format = 32;
    xev.data.l[0] = new_desk;
    XSendEvent(dd, root_win, False, SubstructureNotifyMask,(XEvent *) &xev);
}

void pager_draw_button(int x, int num)
{
    char label[8];
    XftColor col;

    if(num == tb.my_desktop) {
        /* current desktop */
        draw_bigbox(x, PAGER_BUTTON_WIDTH);
    } else {
        set_foreground(6);
        fill_rect(x, 0, PAGER_BUTTON_WIDTH + 1, WINHEIGHT);
    }

    if(((tag_mask >> num) & 1) == 1) {
        draw_tinybox(x, 3);
    }

    int str_length = sizeof(tags_name) / sizeof(tags_name[1]);
    if (num < str_length) {
        strcpy(label, tags_name[num]);
    } else {
        label[0] = '1' + num;
    }

    col.color.alpha = 0xffff;
    col.color.red = cols[1].red;
    col.color.green = cols[1].green;
    col.color.blue = cols[1].blue;

    XftDrawStringUtf8(xftdraw, &col, xfs, x +((PAGER_BUTTON_WIDTH - PAGER_DIGIT_WIDTH) / 3), text_y,(const FcChar8 *)label, strlen(label));
}

void draw_pager(void)
{
    int desks, i;
    int x = 0;
    desks = get_number_of_desktops();

    for(i = 0; i < desks; i++) {
        pager_draw_button(x, i);

        if(i > 8) {
            break;
        }

        x += PAGER_BUTTON_WIDTH;
    }

    pager_size = x;
}

void gui_draw_taskbar(int sig)
{
    if(sig != SIGALRM) {
        return;
    }

    task *tk;
    int x, width, taskw;
    draw_pager();
    width = WINWIDTH -(pager_size);
    x = pager_size + 2;

    if(tb.num_tasks == 0) {
        goto clear;
    }

    taskw = width / tb.num_tasks;

    if(taskw > MAX_TASK_WIDTH) {
        taskw = MAX_TASK_WIDTH;
    }

    tk = tb.task_list;

    while(tk) {
        tk->pos_x = x;
        tk->width = taskw - 1;
        gui_draw_task(tk);
        x += taskw;
        tk = tk->next;
    }

    if(x <(width + pager_size + 2)) {
clear:
        set_foreground(6);
        fill_rect(x, 0, WINWIDTH, WINHEIGHT);
    }
    draw_status();
}

task *find_task(Window win)
{
    task *list = tb.task_list;

    while(list) {
        if(list->win == win) {
            return list;
        }

        list = list->next;
    }

    return 0;
}

void del_task(Window win)
{
    task *next, *prev = 0, *list = tb.task_list;

    while(list) {
        next = list->next;

        if(list->win == win) {
            /* unlink and free this task */
            tb.num_tasks--;

            if(list->mask != None) {
                XFreePixmap(dd, list->mask);
            }

            if(list->name) {
                XFree(list->name);
            }

            free(list);

            if(prev == 0) {
                tb.task_list = next;
            } else {
                prev->next = next;
            }

            return;
        }

        prev = list;
        list = next;
    }
}

void taskbar_read_clientlist(void)
{
    Window *win, focus_win;
    int num, i, rev, desk, new_desk = 0;
    task *list, *next;
    desk = get_current_desktop();

    if(desk != tb.my_desktop) {
        new_desk = 1;
        tb.my_desktop = desk;
    }

    XGetInputFocus(dd, &focus_win, &rev);
    win = get_prop_data(root_win, atom__NET_CLIENT_LIST, XA_WINDOW, &num);

    if(!win) {
        return;
    }

    /* remove windows that arn't in the _NET_CLIENT_LIST anymore */
    list = tb.task_list;

    if(list) {
        tag_mask |= 1 << tb.my_desktop;
    } else {
        tag_mask &= ~(1 << tb.my_desktop);
    }

    while(list) {
        list->focused =(focus_win == list->win);
        next = list->next;

        if(!new_desk)
            for(i = num - 1; i >= 0; i--)
                if(list->win == win[i]) {
                    goto dontdel;
                }

        del_task(list->win);
dontdel:
        list = next;
    }

    /* add any new windows */
    for(i = 0; i < num; i++) {
        if(!find_task(win[i])) {
            add_task(win[i],(win[i] == focus_win));
        }
    }

    XFree(win);
}

void handle_press(int x, int y, int button)
{
    task *tk;

    if(y > 0 && y < WINHEIGHT) {
        switch_desk(x / PAGER_BUTTON_WIDTH);
    }

    tk = tb.task_list;

    while(tk) {
        if(x > tk->pos_x && x < tk->pos_x + tk->width) {
            if(button == 3) {  /* right-click */
                toggle_shade(tk->win);
                return;
            }

            if(tk->iconified) {
                tk->iconified = 0;
                tk->focused = 1;
                XMapWindow(dd, tk->win);
            } else {
                if(tk->focused) {
                    tk->iconified = 1;
                    tk->focused = 0;
                    XIconifyWindow(dd, tk->win, scr_screen);
                } else {
                    tk->focused = 1;
                    XRaiseWindow(dd, tk->win);
                    XSetInputFocus(dd, tk->win, RevertToNone, CurrentTime);
                }
            }

            gui_sync();
            gui_draw_task(tk);
        } else {
            if(button == 1 && tk->focused) {
                tk->focused = 0;
                gui_draw_task(tk);
            }
        }

        tk = tk->next;
    }
}

void handle_focusin(Window win)
{
    task *tk;
    tk = tb.task_list;

    while(tk) {
        if(tk->focused) {
            if(tk->win != win) {
                tk->focused = 0;
                gui_draw_task(tk);
            }
        } else {
            if(tk->win == win) {
                tk->focused = 1;
                gui_draw_task(tk);
            }
        }

        tk = tk->next;
    }
}

void handle_propertynotify(Window win, Atom at)
{
    task *tk;

    if(win == root_win) {
        if(at == atom__NET_CLIENT_LIST || at == atom__NET_CURRENT_DESKTOP) {
            taskbar_read_clientlist();
            gui_draw_taskbar(SIGALRM);
        }

        return;
    }

    tk = find_task(win);

    if(!tk) {
        return;
    }

    if(at == XA_WM_NAME) {
        /* window's title changed */
        if(tk->name) {
            XFree(tk->name);
        }

        XTextProperty prop;
        XGetTextProperty(dd, win, &prop, netwmname);
        tk->name = (char *)prop.value;

        gui_draw_task(tk);
    } else if(at == atom_WM_STATE) {
        /* iconified state changed? */
        if(is_iconified(tk->win) != tk->iconified) {
            tk->iconified = !tk->iconified;
            gui_draw_task(tk);
        }
    }
}

void handle_error(Display *d, XErrorEvent *ev)
{
}

int main(void)
{
    XEvent ev;
    fd_set fd;
    int xfd;

    dd = XOpenDisplay(NULL);

    if(!dd) {
        return 0;
    }

    scr_screen = DefaultScreen(dd);
    scr_depth = DefaultDepth(dd, scr_screen);
    scr_height = DisplayHeight(dd, scr_screen);
    scr_width = DisplayWidth(dd, scr_screen);
    root_win = RootWindow(dd, scr_screen);

    XSelectInput(dd, root_win, PropertyChangeMask);
    XSetErrorHandler((XErrorHandler) handle_error);
    XInternAtoms(dd, atom_names, ATOM_COUNT, False, atoms);
    netwmname = XInternAtom(dd, "_NET_WM_NAME", False);

    gui_init();
    bzero(&tb, sizeof(struct taskbar));
    tb.win = gui_create_taskbar();
    xfd = ConnectionNumber(dd);
    gui_sync();

    gui_draw_taskbar(SIGALRM);
    signal(SIGALRM, gui_draw_taskbar);

    while(1) {
        FD_ZERO(&fd);
        FD_SET(xfd, &fd);
        select(xfd + 1, &fd, 0, 0, 0);
        alarm(STATUS_DELAY);

        while(XPending(dd)) {
            XNextEvent(dd, &ev);

            switch(ev.type) {
                case ButtonPress:
                    handle_press(ev.xbutton.x, ev.xbutton.y, ev.xbutton.button);
                    break;
                case DestroyNotify:
                    del_task(ev.xdestroywindow.window);
                case PropertyNotify:
                    handle_propertynotify(ev.xproperty.window, ev.xproperty.atom);
                    break;
                case FocusIn:
                    handle_focusin(ev.xfocus.window);
                    break;
            }
        }
    }

    free(extents);
    XftFontClose(dd, xfs);
    XFree(dd);
}
