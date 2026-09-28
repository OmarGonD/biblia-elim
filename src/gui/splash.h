#ifndef _GUI_SPLASH_H_
#define _GUI_SPLASH_H_

#include <glib.h>

gboolean gui_splash_done(void);
void gui_splash_step(gchar *text, gdouble progress, gint step);
void gui_splash_init(void);

#endif /* !_GUI_SPLASH_H_ */
