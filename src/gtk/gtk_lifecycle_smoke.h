#ifndef GTK_LIFECYCLE_SMOKE_H
#define GTK_LIFECYCLE_SMOKE_H

#include <glib.h>

G_BEGIN_DECLS

/* Test-only integration hook. It is inert unless the environment variable
 * BIBLIA_ELIM_GTK_LIFECYCLE_SMOKE is exactly "1". */
void gtk_lifecycle_smoke_install(void);
void gtk_lifecycle_smoke_schedule(void);
int gtk_lifecycle_smoke_exit_status(void);

G_END_DECLS

#endif
