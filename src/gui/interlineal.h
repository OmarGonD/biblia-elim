#ifndef __GUI_INTERLINEAL_H__
#define __GUI_INTERLINEAL_H__

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

GtkWidget *gui_interlineal_wrap(GtkWidget *html_master);
void gui_interlineal_set_active(gboolean active);
void gui_interlineal_rellenar(void);
void gui_interlineal_ficha(const char *strong);
GtkWidget *gui_interlineal_tabla_widget(const char *key);
void gui_verse_tools_popup(const char *key);
void on_interlineal_activate(GtkCheckMenuItem *menuitem, gpointer user_data);

#ifdef __cplusplus
}
#endif
#endif
