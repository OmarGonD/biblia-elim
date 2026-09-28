/*
 * picker_entry.h - the field at the top of a navbar picker popover
 *
 * The chapter, verse and book pickers open with the focus in an entry so
 * the reader can type straight away, and give the focus back when they
 * close. GtkPopover gives it back to the widget that had it when the
 * popover opened; when there is none (the window had no focus widget, or
 * that widget stopped being drawable meanwhile, e.g. the Bible pane
 * re-rendering behind the picker), the window is left without a focus
 * widget and the keyboard reaches nothing until something is clicked.
 * These helpers make sure there always is one.
 */
#ifndef XIPHOS_PICKER_ENTRY_H
#define XIPHOS_PICKER_ENTRY_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Where the keyboard focus goes when GTK has nothing to give it back to:
 * asked at that moment (the reading pane may have been rebuilt), NULL for
 * "the picker's anchor". */
typedef GtkWidget *(*PickerFocusFallback)(void);

/* Call before popping up a picker anchored at `anchor`. Makes sure the
 * window has a real focus widget to give the focus back to: when it has
 * none, the focus goes to the fallback (or `anchor`). */
void picker_entry_settle_window_focus(GtkWidget *anchor,
				      PickerFocusFallback fallback);

/* Gives `entry` the focus when `popover` maps (now, if it already has),
 * unless it already has it -- GtkPopover focuses its first focusable child
 * on its own. Whatever the entry holds is selected, so typing replaces it.
 * One request, no retries.
 *
 * Until the entry is destroyed, if the window is left without a focus
 * widget while the popover is closed (its "give the focus back" found
 * nothing), the focus moves on to the fallback at once, as in
 * picker_entry_settle_window_focus(). */
void picker_entry_focus_on_open(GtkWidget *popover, GtkWidget *entry,
				PickerFocusFallback fallback);

G_END_DECLS

#endif /* XIPHOS_PICKER_ENTRY_H */
