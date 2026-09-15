/*
 * The chapter/verse picker, with real GTK widgets and real keyboard input:
 * keys go through the X server (XTest) into GDK, the window, the entry and
 * its input method, exactly as typing does. Run under a display (xvfb-run).
 * Nothing sets the entry's text or emits "changed" in its place.
 *
 * The window mirrors the app's navbar: a toggle button as the selector and
 * a text view as the reading pane. Pickers are built and closed the way
 * navbar_versekey.cc does it (popover with an entry and number buttons,
 * destroyed from an idle after "closed").
 *
 * It starts with no focus widget in the window, which is how the app
 * starts. A control run first shows what that does to a bare GtkPopover:
 * closing it leaves the GtkWindow as its own focus widget, the next picker's
 * entry never receives focus-in and every close logs "has no handler with
 * id". Then 100 rounds through picker_entry.c must do none of that.
 *
 * Limits: this is X11. Wayland's text-input/fcitx path is not exercised.
 */
#include "main/picker_entry.h"

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <stdio.h>
#include <string.h>

#define ROUNDS 100

static int failures;
static int criticals;
static int alive_popovers;

#define CHECK(condition)                                                        \
	do {                                                                      \
		if (!(condition)) {                                                \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,    \
				#condition);                                        \
			failures++;                                               \
		}                                                                 \
	} while (0)

static GtkWidget *window, *bible, *anchor;

static GtkWidget *
bible_view(void)
{
	return bible;
}
static gboolean waited_too_long;

static GLogWriterOutput
count_criticals(GLogLevelFlags level, const GLogField *fields, gsize n,
		gpointer data)
{
	if (level & G_LOG_LEVEL_CRITICAL) {
		gsize i;

		criticals++;
		for (i = 0; i < n; i++)
			if (!strcmp(fields[i].key, "MESSAGE"))
				fprintf(stderr, "CRITICAL: %s\n",
					(const char *)fields[i].value);
		return G_LOG_WRITER_HANDLED;
	}
	return g_log_writer_default(level, fields, n, data);
}

/* only a guard so a broken display fails instead of hanging */
static gboolean
give_up_waiting(gpointer data)
{
	(void)data;
	waited_too_long = TRUE;
	return G_SOURCE_REMOVE;
}

/* Runs the main loop (blocking on events) until `done` holds, at most
 * `seconds`. The time is only a guard: the condition is what ends it. */
static gboolean
run_until(gboolean (*done)(gpointer), gpointer data, guint seconds)
{
	guint guard;

	waited_too_long = FALSE;
	guard = g_timeout_add_seconds(seconds, give_up_waiting, NULL);
	while (!done(data) && !waited_too_long)
		gtk_main_iteration_do(TRUE);
	if (!waited_too_long)
		g_source_remove(guard);
	return done(data);
}

static void
flush(void)
{
	while (gtk_events_pending())
		gtk_main_iteration_do(FALSE);
}

static gboolean
window_active(gpointer data)
{
	(void)data;
	return gtk_window_is_active(GTK_WINDOW(window));
}

/* ---- real keyboard ---------------------------------------------------- */

static void
x_key(KeySym sym)
{
	Display *dpy = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
	KeyCode code = XKeysymToKeycode(dpy, sym);

	XTestFakeKeyEvent(dpy, code, True, CurrentTime);
	XTestFakeKeyEvent(dpy, code, False, CurrentTime);
	XFlush(dpy);
}

/* ---- a picker, as navbar_versekey.cc builds it ------------------------ */

typedef struct {
	GtkWidget *popover;
	GtkWidget *entry;
	gboolean focused_in;
	gboolean picked;
	gboolean closed;
} Picker;

static void
popover_finalized(gpointer data, GObject *where)
{
	(void)data;
	(void)where;
	alive_popovers--;
}

static gboolean control_run;

static gboolean
destroy_idle(gpointer popover)
{
	/* as picker_destroy_idle(); the control keeps GTK's default */
	if (!control_run)
		gtk_popover_set_modal(GTK_POPOVER(popover), FALSE);
	gtk_widget_destroy(GTK_WIDGET(popover));
	return G_SOURCE_REMOVE;
}

static void
on_closed(GtkPopover *popover, gpointer data)
{
	Picker *p = data;

	p->closed = TRUE;
	/* as picker_closed(): destroy from an idle, handing it our ref */
	g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, destroy_idle, popover,
			g_object_unref);
}

static void
on_activate(GtkEntry *entry, gpointer data)
{
	Picker *p = data;

	(void)entry;
	if (strcmp(gtk_entry_get_text(GTK_ENTRY(p->entry)), "12") == 0) {
		p->picked = TRUE;
		gtk_popover_popdown(GTK_POPOVER(p->popover));
	}
}

static gboolean
on_focus_in(GtkWidget *entry, GdkEventFocus *event, gpointer data)
{
	Picker *p = data;

	(void)entry;
	(void)event;
	p->focused_in = TRUE;
	return FALSE;
}

static void
open_picker(Picker *p, gboolean settle)
{
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gint i;

	memset(p, 0, sizeof(*p));
	p->popover = gtk_popover_new(anchor);
	g_object_ref_sink(p->popover);
	alive_popovers++;
	g_object_weak_ref(G_OBJECT(p->popover), popover_finalized, NULL);

	p->entry = gtk_entry_new();
	gtk_entry_set_input_purpose(GTK_ENTRY(p->entry), GTK_INPUT_PURPOSE_DIGITS);
	gtk_box_pack_start(GTK_BOX(box), p->entry, FALSE, FALSE, 0);
	for (i = 1; i <= 5; i++) {
		gchar *label = g_strdup_printf("%d", i);
		gtk_box_pack_start(GTK_BOX(box), gtk_button_new_with_label(label),
				   FALSE, FALSE, 0);
		g_free(label);
	}
	gtk_container_add(GTK_CONTAINER(p->popover), box);
	gtk_widget_show_all(box);
	g_signal_connect(p->entry, "focus-in-event", G_CALLBACK(on_focus_in), p);
	g_signal_connect(p->entry, "activate", G_CALLBACK(on_activate), p);
	g_signal_connect(p->popover, "closed", G_CALLBACK(on_closed), p);

	if (settle)
		picker_entry_settle_window_focus(anchor, bible_view);
	gtk_popover_popup(GTK_POPOVER(p->popover));
	if (settle)
		picker_entry_focus_on_open(p->popover, p->entry, bible_view);
	else
		gtk_widget_grab_focus(p->entry);
}

static gboolean
picker_focused(gpointer data)
{
	Picker *p = data;
	return p->focused_in && gtk_widget_has_focus(p->entry);
}

static gboolean
picker_closed(gpointer data)
{
	Picker *p = data;
	return p->closed;
}

static gboolean
text_is_12(gpointer data)
{
	Picker *p = data;
	return strcmp(gtk_entry_get_text(GTK_ENTRY(p->entry)), "12") == 0;
}

/* opens a picker, checks it has the keyboard, then closes it with Escape
 * or by typing 12 + Enter; returns FALSE on the first thing that failed */
static gboolean
picker_round_full(gboolean settle, gboolean type_and_enter,
		  gboolean pane_rebuilt, gboolean verbose)
{
	Picker p;
	gboolean ok = TRUE;
	GtkWidget *focus;

	/* the reading pane holds the focus before the picker, as when reading */
	if (pane_rebuilt && gtk_window_get_focus(GTK_WINDOW(window)) != bible)
		gtk_widget_grab_focus(bible);
	open_picker(&p, settle);
	if (pane_rebuilt) {
		/* the pane re-renders behind the open picker: GtkPopover's
		 * recorded focus goes away ("unmap"), so on close it has
		 * nothing to give the focus back to */
		gtk_widget_hide(bible);
		gtk_widget_show(bible);
		flush();
	}
	if (!run_until(picker_focused, &p, 3)) {
		if (verbose)
			printf("  entry did not receive focus-in (is_focus=%d "
			       "window_active=%d)\n",
			       gtk_widget_is_focus(p.entry), window_active(NULL));
		ok = FALSE;
	}
	if (type_and_enter) {
		x_key(XK_1);
		x_key(XK_2);
		if (!run_until(text_is_12, &p, 3)) {
			if (verbose)
				printf("  typed 12, entry reads '%s'\n",
				       gtk_entry_get_text(GTK_ENTRY(p.entry)));
			ok = FALSE;
		}
		x_key(XK_Return);
	} else {
		x_key(XK_Escape);
	}
	if (!run_until(picker_closed, &p, 3)) {
		gtk_popover_popdown(GTK_POPOVER(p.popover));
		run_until(picker_closed, &p, 3);
		ok = FALSE;
	}
	if (type_and_enter && !p.picked)
		ok = FALSE;
	flush();
	focus = gtk_window_get_focus(GTK_WINDOW(window));
	if (focus == window || !window_active(NULL)) {
		if (verbose)
			printf("  after close: window is its own focus=%d active=%d\n",
			       focus == window, window_active(NULL));
		ok = FALSE;
	}
	if (pane_rebuilt && settle && focus != bible) {
		if (verbose)
			printf("  after close: focus on %s, not the reading pane\n",
			       focus ? G_OBJECT_TYPE_NAME(focus) : "nothing");
		ok = FALSE;
	}
	return ok;
}

static gboolean
picker_round(gboolean settle, gboolean type_and_enter, gboolean verbose)
{
	return picker_round_full(settle, type_and_enter, FALSE, verbose);
}

/* the app starts with no focus widget in the window */
static void
reset_to_startup_focus(void)
{
	gtk_window_set_focus(GTK_WINDOW(window), NULL);
	flush();
}

static GtkWidget *
make_window(void)
{
	GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

	gtk_window_set_default_size(GTK_WINDOW(w), 400, 300);
	/* as the app's main window (main_window.c): the window itself can
	 * take the focus, which is what GtkPopover falls back to */
	gtk_widget_set_can_focus(w, TRUE);
	anchor = gtk_toggle_button_new_with_label("18");
	gtk_widget_set_focus_on_click(anchor, FALSE);
	bible = gtk_text_view_new();
	gtk_box_pack_start(GTK_BOX(box), anchor, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), bible, TRUE, TRUE, 0);
	gtk_container_add(GTK_CONTAINER(w), box);
	gtk_widget_show_all(w);
	return w;
}

static gboolean
take_x_focus(void)
{
	gdk_window_focus(gtk_widget_get_window(window), GDK_CURRENT_TIME);
	return run_until(window_active, NULL, 5);
}

int
main(int argc, char **argv)
{
	gint round, bad_rounds = 0, control_criticals;
	gboolean control_ok;

	g_log_set_writer_func(count_criticals, NULL, NULL);
	/* XTest needs X11, even when the session also offers Wayland */
	gdk_set_allowed_backends("x11");
	if (!gtk_init_check(&argc, &argv) ||
	    !GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
		printf("navbar_picker_entry_skipped=no-x11-display\n");
		return 77;
	}
	/* transitions on, as in the app: a popover popping down is still
	 * visible when GtkPopover gives the focus back */
	g_object_set(gtk_settings_get_default(), "gtk-enable-animations", TRUE,
		     NULL);

	/* control: a bare popover from the app's startup focus state */
	control_run = TRUE;
	window = make_window();
	CHECK(take_x_focus());
	reset_to_startup_focus();
	criticals = 0;
	picker_round(FALSE, FALSE, FALSE);          /* leaves window-as-focus */
	control_ok = picker_round(FALSE, TRUE, TRUE);
	control_criticals = criticals;
	printf("control (bare GtkPopover from no focus): second picker usable=%d "
	       "criticals=%d\n", control_ok, control_criticals);
	/* if GTK ever stops doing this, the fix is moot, not wrong */
	if (control_ok && control_criticals == 0)
		printf("control: this GTK no longer breaks without the fix\n");
	gtk_widget_destroy(window);
	flush();

	/* control: the pane re-renders behind a bare picker */
	window = make_window();
	CHECK(take_x_focus());
	picker_round_full(FALSE, FALSE, TRUE, FALSE);
	flush();
	printf("control (bare GtkPopover, pane re-rendered while open): window "
	       "is its own focus after close=%d\n",
	       gtk_window_get_focus(GTK_WINDOW(window)) == window);
	control_run = FALSE;
	gtk_widget_destroy(window);
	flush();

	/* the fix: same start, 100 rounds */
	window = make_window();
	CHECK(take_x_focus());
	reset_to_startup_focus();
	criticals = 0;
	for (round = 1; round <= ROUNDS; round++) {
		gboolean verbose = bad_rounds < 3;
		gboolean ok = TRUE;

		ok &= picker_round(TRUE, FALSE, verbose); /* chapter, Escape */
		ok &= picker_round(TRUE, FALSE, verbose); /* verse, Escape */
		ok &= picker_round(TRUE, TRUE, verbose);  /* chapter, 12 Enter */
		ok &= picker_round(TRUE, FALSE, verbose); /* verse, Escape */
		/* verse, pane re-rendered while open, Escape */
		ok &= picker_round_full(TRUE, FALSE, TRUE, verbose);
		if (!ok) {
			if (verbose)
				printf("round %d failed\n", round);
			bad_rounds++;
		}
	}
	flush();
	printf("rounds=%d bad_rounds=%d criticals=%d popovers_alive=%d "
	       "window_active=%d\n", ROUNDS, bad_rounds, criticals,
	       alive_popovers, window_active(NULL));
	CHECK(bad_rounds == 0);
	CHECK(criticals == 0);
	CHECK(alive_popovers == 0);

	gtk_widget_destroy(window);
	flush();
	printf("navbar_picker_entry_failures=%d\n", failures);
	return failures ? 1 : 0;
}
