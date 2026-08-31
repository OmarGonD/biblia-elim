#ifndef __LECTURA_SYNC_H__
#define __LECTURA_SYNC_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

void main_lectura_sync_actualizar(void);
/* same rendering as main_lectura_sync_actualizar(), but for whatever
 * verse the user is currently scrolled to in the main pane rather than
 * settings.currentverse -- the "reading focus" following. */
void main_lectura_sync_focus_verse(const gchar *key_text);
gchar *main_lectura_sync_default_module(void);

#ifdef __cplusplus
}
#endif
#endif
