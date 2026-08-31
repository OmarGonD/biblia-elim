#ifndef __GUI_LECTURA_SYNC_H__
#define __GUI_LECTURA_SYNC_H__

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

GtkWidget *gui_lectura_sync_wrap(GtkWidget *html_master);
void gui_lectura_sync_set_visible(gboolean visible);
void gui_lectura_sync_actualizar(void);
void gui_lectura_sync_rellenar_combo(void);
void gui_lectura_sync_set_ref(const char *ref);
void gui_lectura_sync_escribir(const char *html);
void gui_lectura_sync_ficha_nota(const char *mod, const char *osis,
				const char *cita);
void gui_lectura_sync_ficha_clear(void);
gboolean gui_lectura_sync_ficha_activa(void);
void on_lectura_sync_activate(GtkCheckMenuItem *menuitem, gpointer user_data);

#ifdef __cplusplus
}
#endif
#endif
