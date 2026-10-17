#ifndef ELIM_SELECTION_TOOLBAR_H
#define ELIM_SELECTION_TOOLBAR_H

#include "gui/widget_helpers.h"

/* Keep the reader interactive while the selection toolbar is open. GTK's
 * modal popover grab can swallow the click that starts the next selection. */
static inline gboolean
selection_toolbar_outside_press(GtkWidget *root, GuiButtonEvent *event,
                                gpointer data)
{
    GtkWidget *toolbar = data;
    (void)root;
    if (gtk_widget_get_visible(toolbar) &&
        (!event->target || (event->target != toolbar &&
         !gtk_widget_is_ancestor(event->target, toolbar))))
        gtk_popover_popdown(GTK_POPOVER(toolbar));
    return FALSE;
}

static inline gboolean
selection_toolbar_escape(GtkWidget *root, GuiKeyEvent *event, gpointer data)
{
    GtkWidget *toolbar = data;
    (void)root;
    if (event->keyval != GDK_KEY_Escape || !gtk_widget_get_visible(toolbar))
        return FALSE;
    gtk_popover_popdown(GTK_POPOVER(toolbar));
    return TRUE;
}

static inline void
selection_toolbar_install(GtkWidget *toolbar)
{
    GtkWidget *root = GTK_WIDGET(gtk_widget_get_root(toolbar));
    gtk_popover_set_autohide(GTK_POPOVER(toolbar), FALSE);
    gui_widget_on_button(root, GTK_PHASE_CAPTURE,
                         selection_toolbar_outside_press, NULL, toolbar);
    gui_widget_on_key_phase(root, GTK_PHASE_CAPTURE,
                            selection_toolbar_escape, NULL, toolbar);
}
#endif
