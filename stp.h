typedef struct task
{
	struct task *next;
	Window win;
	Pixmap icon;
	Pixmap mask;
	char *name;
	int pos_x;
	int width;
	unsigned int focused:1;
	unsigned int iconified:1;
	unsigned int icon_copied:1;
}
task;

typedef struct taskbar
{
	Window win;
	task *task_list;
	int num_tasks;
	int my_desktop;
	unsigned int hidden:1;
	unsigned int at_top:1;
}
taskbar;

#define MWM_HINTS_DECORATIONS         (1L << 1)
typedef struct _mwmhints
{
	unsigned long flags;
	unsigned long functions;
	unsigned long decorations;
	long inputMode;
	unsigned long status;
}
MWMHints;

#define WIN_STATE_STICKY          (1<<0)	/* everyone knows sticky */
#define WIN_STATE_MINIMIZED       (1<<1)	/* ??? */
#define WIN_STATE_MAXIMIZED_VERT  (1<<2)	/* window in maximized V state */
#define WIN_STATE_MAXIMIZED_HORIZ (1<<3)	/* window in maximized H state */
#define WIN_STATE_HIDDEN          (1<<4)	/* not on taskbar but window visible */
#define WIN_STATE_SHADED          (1<<5)	/* shaded (NeXT style) */
#define WIN_STATE_HID_WORKSPACE   (1<<6)	/* not on current desktop */
#define WIN_STATE_HID_TRANSIENT   (1<<7)	/* owner of transient is hidden */
#define WIN_STATE_FIXED_POSITION  (1<<8)	/* window is fixed in position even */
#define WIN_STATE_ARRANGE_IGNORE  (1<<9)	/* ignore for auto arranging */

#define WIN_HINTS_SKIP_FOCUS      (1<<0)	/* "alt-tab" skips this win */
#define WIN_HINTS_SKIP_WINLIST    (1<<1)	/* not in win list */
#define WIN_HINTS_SKIP_TASKBAR    (1<<2)	/* not on taskbar */
#define WIN_HINTS_GROUP_TRANSIENT (1<<3)	/* ??????? */
#define WIN_HINTS_FOCUS_ON_CLICK  (1<<4)	/* app only accepts focus when clicked */
#define WIN_HINTS_DO_NOT_COVER    (1<<5)	/* attempt to not cover this window */
