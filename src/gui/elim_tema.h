#ifndef __ELIM_TEMA_H__
#define __ELIM_TEMA_H__

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

void gui_elim_tema_init(void);
void gui_elim_tema_marcar_listo(void);
void gui_elim_tema_aplicar(void);
void gui_elim_tema_set(const char *mode);
void gui_elim_tema_bind_menu(GtkBuilder *gxml);
const char *gui_elim_tema_bg(void);
const char *gui_elim_tema_fg(void);

#ifdef __cplusplus
}
#endif
#endif
