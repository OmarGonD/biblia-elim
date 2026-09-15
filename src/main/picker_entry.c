/*
 * picker_entry.c - the field at the top of a navbar picker popover
 */

#include "main/picker_entry.h"

#define FALLBACK_KEY "picker-entry-fallback"

/* Moves the focus off the window itself (or out of "no focus") onto
 * `target`, keeping the window active if it was. */
static void
move_focus_off_window(GtkWindow *window, GtkWidget *target)
{
	GtkWidget *top = GTK_WIDGET(window);
	gboolean was_self = gtk_window_get_focus(window) == top;
	gboolean was_active = gtk_window_is_active(window);

	gtk_widget_grab_focus(target);

	/* Leaving the window-as-focus state sent the window a focus-out
	 * (gtk_window_real_set_focus() -> do_focus_change()), which GtkWindow
	 * reads as losing the keyboard. The compositor did not take it away,
	 * so run GtkWindow's focus-in handling again to match. */
	if (was_self && was_active && !gtk_window_is_active(window)) {
		GdkEvent *event = gdk_event_new(GDK_FOCUS_CHANGE);
		GdkSeat *seat =
		    gdk_display_get_default_seat(gtk_widget_get_display(top));

		event->focus_change.window =
		    g_object_ref(gtk_widget_get_window(top));
		event->focus_change.send_event = TRUE;
		event->focus_change.in = TRUE;
		if (seat)
			gdk_event_set_device(event, gdk_seat_get_keyboard(seat));
		gtk_widget_send_focus_change(top, event);
		gdk_event_free(event);
	}
}

static GtkWidget *
pick_target(PickerFocusFallback fallback, GtkWidget *anchor)
{
	GtkWidget *target = fallback ? fallback() : NULL;

	if (target && gtk_widget_is_drawable(target) &&
	    gtk_widget_get_can_focus(target))
		return target;
	return (anchor && gtk_widget_is_drawable(anchor)) ? anchor : NULL;
}

void
picker_entry_settle_window_focus(GtkWidget *anchor, PickerFocusFallback fallback)
{
	GtkWidget *top, *focus, *target;

	if (!anchor)
		return;
	top = gtk_widget_get_toplevel(anchor);
	if (!GTK_IS_WINDOW(top))
		return;
	focus = gtk_window_get_focus(GTK_WINDOW(top));
	if (focus && focus != top)
		return; /* a real widget: GtkPopover gives it back */
	target = pick_target(fallback, anchor);
	if (target)
		move_focus_off_window(GTK_WINDOW(top), target);
}

static void
on_popover_map(GtkWidget *popover, gpointer data)
{
	GtkWidget *entry = GTK_WIDGET(data);

	(void)popover;
	if (!gtk_widget_get_mapped(entry))
		return;
	if (!gtk_widget_is_focus(entry))
		gtk_widget_grab_focus(entry);
	if (gtk_entry_get_text_length(GTK_ENTRY(entry)) > 0)
		gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
}

/* after GtkWindow has set the focus: only the window-itself case. Opening
 * a picker never puts the focus there (GtkPopover moves it to NULL, then to
 * itself); closing does when it has nothing to give back -- and with
 * transitions the popover is still visible at that point (popping down),
 * so its visibility says nothing. */
static void
on_window_set_focus(GtkWindow *window, GtkWidget *widget, gpointer data)
{
	GtkWidget *entry = GTK_WIDGET(data);
	GtkWidget *popover = gtk_widget_get_ancestor(entry, GTK_TYPE_POPOVER);
	GtkWidget *target;

	if (widget != GTK_WIDGET(window) || !popover)
		return;
	target = pick_target(
	    (PickerFocusFallback)g_object_get_data(G_OBJECT(entry), FALLBACK_KEY),
	    gtk_popover_get_relative_to(GTK_POPOVER(popover)));
	if (target)
		move_focus_off_window(window, target);
}

void
picker_entry_focus_on_open(GtkWidget *popover, GtkWidget *entry,
			   PickerFocusFallback fallback)
{
	GtkWidget *top = gtk_widget_get_toplevel(popover);

	gtk_widget_set_can_focus(entry, TRUE);
	g_object_set_data(G_OBJECT(entry), FALLBACK_KEY, (gpointer)fallback);
	/* the entry is the popover's child: it outlives every "map" */
	g_signal_connect(popover, "map", G_CALLBACK(on_popover_map), entry);
	if (GTK_IS_WINDOW(top))
		g_signal_connect_object(top, "set-focus",
					G_CALLBACK(on_window_set_focus), entry,
					G_CONNECT_AFTER);
	if (gtk_widget_get_mapped(popover))
		on_popover_map(popover, entry);
}
