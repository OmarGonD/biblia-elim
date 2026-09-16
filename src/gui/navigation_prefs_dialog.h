/*
 * navigation_prefs_dialog.h - Ver > Navegación y rueda
 */
#ifndef XIPHOS_NAVIGATION_PREFS_DIALOG_H
#define XIPHOS_NAVIGATION_PREFS_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Opens the dialog over the main window (or presents it when it is already
 * open). Changes apply at once; they are stored when a setting is let go
 * (slider released, option chosen, reset) and when the dialog closes. */
void gui_navigation_prefs_dialog_show(void);

G_END_DECLS

#endif /* XIPHOS_NAVIGATION_PREFS_DIALOG_H */
