/*
 * picker_entry.h - the field at the top of a navbar picker popover
 *
 * The chapter, verse and book pickers open with the focus in an entry so
 * the reader can type straight away, and give the focus back when they
 * close. Both halves go through GtkPopover's modality (gtkpopover.c):
 * opening records the window's focus widget, closing grabs it back -- or,
 * when there is none to give back (none was recorded, or it stopped being
 * drawable meanwhile, e.g. the Bible pane re-rendering behind the picker),
 * grabs the focus onto the GtkWindow itself.
 *
 * That last case breaks the window: once the GtkWindow is its own focus
 * widget, the next focus change sends it a focus-out, which GtkWindow takes
 * as the toplevel losing the keyboard (gtk_window_focus_out_event()); from
 * then on no widget in it receives focus-in, so a picker's entry looks
 * focused but its input method never starts. A popover opened in that
 * state also records the window as the focus to give back, and every close
 * then disconnects an "unmap" handler its own
 * g_signal_handlers_disconnect_by_data() already removed ("instance has no
 * handler with id" criticals).
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
 * window has a real focus widget for GtkPopover to record: when it has
 * none, or the window itself holds the focus, the focus goes to the
 * fallback (or `anchor`). If moving it away from the window itself made
 * GTK mark an active window inactive, the window's own focus-in handling
 * is run again, so the state matches what the compositor still has. */
void picker_entry_settle_window_focus(GtkWidget *anchor,
				      PickerFocusFallback fallback);

/* Gives `entry` the focus when `popover` maps (now, if it already has),
 * unless it already has it -- GtkPopover focuses its first focusable child
 * on its own. Whatever the entry holds is selected, so typing replaces it.
 * One request, no retries.
 *
 * Until the entry is destroyed, if GTK puts the focus on the window itself
 * while the popover is closed (its "give the focus back" found nothing),
 * the focus moves on to the fallback at once, as in
 * picker_entry_settle_window_focus(). The handler is tied to `entry`, not
 * to the popover: GtkPopover disconnects everything on the window whose
 * data is the popover. */
void picker_entry_focus_on_open(GtkWidget *popover, GtkWidget *entry,
				PickerFocusFallback fallback);

G_END_DECLS

#endif /* XIPHOS_PICKER_ENTRY_H */
