/*
 * Biblia Elim — scripted reproduction of the clicked-note panel bug.
 *
 * The "Comentarios del autor" heading and body are produced together by
 * one GTKEntryDisp::display() pass, but they used to read two different
 * references: the body took the key the pane had just set, the heading
 * took settings.currentverse, i.e. wherever the Bible pane happened to
 * be looking.  Clicking a note of Matthew 11:11 while focused on 11:7
 * therefore painted 11:11's comment under an "Mt 11:7" heading -- in a
 * single render, with no second refresh involved.
 *
 * Source-level checks cannot see that: the defect is which value one
 * expression reads at runtime.  So this drives the real application --
 * real modules, real click routing through main_url_handler() -- and
 * asserts against the HTML the panel is actually handed.  It is inert
 * unless BIBLIA_ELIM_AC_PROBE=1, exactly like the GTK lifecycle smoke
 * harness next door.
 */
#ifndef XIPHOS_AUTHOR_COMMENTARY_PROBE_H
#define XIPHOS_AUTHOR_COMMENTARY_PROBE_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Records `html` when it is being painted into the author commentary
 * pane. A no-op unless the probe is running. */
void author_commentary_probe_capture(GtkWidget *widget, const char *html);

/* Queues the scripted run, if the probe is requested. */
void author_commentary_probe_schedule(void);

/* 0 when every check passed (or the probe never ran). */
int author_commentary_probe_exit_status(void);

#ifdef __cplusplus
}
#endif

#endif
