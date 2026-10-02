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
/* La misma ficha, diciendo además cómo está la palabra en este
 * versículo. morph puede ser NULL. */
void gui_interlineal_ficha_morf(const char *strong, const char *morph);
/* Igual, con el versículo (key; NULL = el del interlineal) y la posición de la palabra en el versículo de
 * Tisch (0 = desconocida) para mostrar la ficha enriquecida. */
void gui_interlineal_ficha_ctx(const char *strong, const char *morph,
			       const char *key, int pos);
GtkWidget *gui_interlineal_tabla_widget(const char *key);
/* Returns the popover (for tests); NULL without a key. */
GtkWidget *gui_verse_tools_popup(const char *key);
void on_interlineal_activate(gpointer menuitem, gpointer user_data);

#ifdef __cplusplus
}
#endif
#endif
