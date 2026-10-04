/*
 * picker_entry.c - the field at the top of a navbar picker popover
 */

#include "main/picker_entry.h"
#include "gui/widget_helpers.h"

#define FALLBACK_KEY "picker-entry-fallback"

static GtkWidget *
pick_target(PickerFocusFallback fallback, GtkWidget *anchor)
{
	GtkWidget *target = fallback ? fallback() : NULL;

	if (target && gtk_widget_is_drawable(target) &&
	    gtk_widget_get_focusable(target))
		return target;
	return (anchor && gtk_widget_is_drawable(anchor)) ? anchor : NULL;
}

void
picker_entry_settle_window_focus(GtkWidget *anchor, PickerFocusFallback fallback)
{
	GtkWidget *top, *target;

	if (!anchor)
		return;
	top = gui_widget_get_toplevel(anchor);
	if (!GTK_IS_WINDOW(top) || gtk_window_get_focus(GTK_WINDOW(top)))
		return; /* a real widget: it gets the focus back */
	target = pick_target(fallback, anchor);
	if (target)
		gtk_widget_grab_focus(target);
}

/* GtkEntry hands the focus to its GtkText child */
static gboolean
picker_entry_has_focus(GtkWidget *entry)
{
	GtkRoot *root = gtk_widget_get_root(entry);
	GtkWidget *focus = root ? gtk_root_get_focus(root) : NULL;

	return focus && (focus == entry || gtk_widget_is_ancestor(focus, entry));
}

static void
on_popover_map(GtkWidget *popover, gpointer data)
{
	GtkWidget *entry = GTK_WIDGET(data);

	(void)popover;
	if (!gtk_widget_get_mapped(entry))
		return;
	if (!picker_entry_has_focus(entry)) {
		/* gtk_widget_grab_focus() answers FALSE here and leaves the
		 * focus where it was (the reading pane) -- the popover opens
		 * but typing goes nowhere. Setting it on the root works. */
		GtkRoot *root = gtk_widget_get_root(entry);
		GtkWidget *text = GTK_WIDGET(
		    gtk_editable_get_delegate(GTK_EDITABLE(entry)));

		if (!gtk_widget_grab_focus(entry) && root)
			gtk_root_set_focus(root, text ? text : entry);
	}
	if (gtk_entry_get_text_length(GTK_ENTRY(entry)) > 0)
		gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
}

/* after the window's focus changed: only the "no focus at all" case, which
 * is what closing a picker leaves when the widget that had the focus
 * before stopped being drawable meanwhile (e.g. the Bible pane rebuilt
 * behind it). */
static void
on_window_focus_widget(GtkWindow *window, GParamSpec *pspec, gpointer data)
{
	GtkWidget *entry = GTK_WIDGET(data);
	GtkWidget *popover = gtk_widget_get_ancestor(entry, GTK_TYPE_POPOVER);
	GtkWidget *target;

	(void)pspec;
	if (gtk_window_get_focus(window) || !popover ||
	    gtk_widget_get_mapped(popover))
		return;
	target = pick_target(
	    (PickerFocusFallback)g_object_get_data(G_OBJECT(entry), FALLBACK_KEY),
	    gtk_widget_get_parent(popover));
	if (target)
		gtk_widget_grab_focus(target);
}

void
picker_entry_focus_on_open(GtkWidget *popover, GtkWidget *entry,
			   PickerFocusFallback fallback)
{
	GtkWidget *top = gui_widget_get_toplevel(popover);

	gtk_widget_set_focusable(entry, TRUE);
	g_object_set_data(G_OBJECT(entry), FALLBACK_KEY, (gpointer)fallback);
	/* the entry is the popover's child: it outlives every "map" */
	g_signal_connect(popover, "map", G_CALLBACK(on_popover_map), entry);
	if (GTK_IS_WINDOW(top))
		g_signal_connect_object(top, "notify::focus-widget",
					G_CALLBACK(on_window_focus_widget), entry,
					G_CONNECT_AFTER);
	if (gtk_widget_get_mapped(popover))
		on_popover_map(popover, entry);
}
