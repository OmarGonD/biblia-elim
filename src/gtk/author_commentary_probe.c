/*
 * Biblia Elim — scripted reproduction of the clicked-note panel bug.
 * See author_commentary_probe.h for why this exists.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <string.h>

#include "gtk/author_commentary_probe.h"
#include "gui/widgets.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/url.hh"

/* set by the webkit hover handler; a real click always has it set,
 * because the pointer is over the marker being clicked. */
extern gint in_url;

#define BIBLE "SpaPlatense"
#define COMMENTARY "SpaPlatenseComentarios"

static gboolean requested;
static gboolean requested_known;
static gchar *painted;
static int failures;

static gboolean
probe_requested(void)
{
	if (!requested_known) {
		const gchar *value = g_getenv("BIBLIA_ELIM_AC_PROBE");
		requested = value && !strcmp(value, "1");
		requested_known = TRUE;
	}
	return requested;
}

void
author_commentary_probe_capture(GtkWidget *widget, const char *html)
{
	if (!probe_requested() || !html || widget != widgets.html_comm)
		return;
	g_free(painted);
	painted = g_strdup(html);
}

int
author_commentary_probe_exit_status(void)
{
	return failures;
}

static void
check(gboolean condition, const char *what)
{
	if (condition)
		return;
	failures++;
	g_printerr("AC_PROBE_CHECK_FAILED %s\n", what);
}

/* Everything the panel was last handed, as one line, so a failure says
 * what was on screen rather than only which assertion tripped. */
static void
report(const char *stage)
{
	gchar *heading = NULL;
	const char *open = painted ? strstr(painted, "<b>") : NULL;
	const char *close = open ? strstr(open, "</b>") : NULL;

	if (open && close)
		heading = g_strndup(open + 3, (gsize)(close - (open + 3)));
	g_printerr("AC_PROBE stage=%s heading=[%s] focus=[%s] painted_bytes=%zu\n",
		   stage, heading ? heading : "(none)",
		   settings.currentverse ? settings.currentverse : "(null)",
		   painted ? strlen(painted) : 0);
	g_free(heading);
}

static gboolean
painted_has(const char *needle)
{
	return painted && strstr(painted, needle) != NULL;
}

static void
settle(int ms)
{
	gint64 end = g_get_monotonic_time() + (gint64)ms * 1000;

	while (g_get_monotonic_time() < end) {
		while (gtk_events_pending())
			gtk_main_iteration_do(FALSE);
		g_usleep(2000);
	}
}

/* The note anchors the Bible pane renders, e.g. for Matthew 11:11:
 *   passagestudy.jsp?action=showNote&type=n&value=23
 *     &module=SpaPlatense&passage=Matthew+11%3A11
 * Routing it through main_url_handler() is what the click does. */
static void
click_note(const char *passage, const char *value)
{
	gchar *url = g_strdup_printf(
	    "passagestudy.jsp?action=showNote&type=n&value=%s"
	    "&module=" BIBLE "&passage=%s",
	    value, passage);

	in_url = 1;
	main_url_handler(url, TRUE);
	g_free(url);
}

static gboolean
quit_now(gpointer unused)
{
	(void)unused;
	gtk_main_quit();
	return FALSE;
}

static gboolean
run(gpointer unused)
{
	(void)unused;
	settle(1800);

	/* 1. Focus the Bible pane on Matthew 11:7 -- a verse the author
	 * never commented. The pane is closed on a fresh profile and must
	 * stay closed: navigating never opens it by itself. */
	{
		gchar *before = g_strdup(painted ? painted : "");
		main_display_bible(BIBLE, "Matthew 11:7");
		settle(800);
		report("focus-11-7");
		check(!g_strcmp0(before, painted ? painted : ""),
		      "navigating must not paint a pane the user has not opened");
		g_free(before);
	}

	/* 2. Click the editorial marker of Matthew 11:11. Heading and
	 * body must both be the clicked verse, and the Bible pane must
	 * not follow. */
	click_note("Matthew+11%3A11", "23");
	settle(2000);
	report("clicked-11-11");
	check(painted_has("<b>Mt 11:11</b>"),
	      "clicked note must head the panel with Mt 11:11");
	check(!painted_has("Mt 11:7"),
	      "the focused verse must not head the clicked note's comment");
	check(painted_has("Es decir"),
	      "clicked note must show the body of Matthew 11:11");
	check(!g_strcmp0(settings.currentverse, "Matthew 11:7"),
	      "clicking a note must not move the Bible pane");

	/* 3. Ordinary navigation still titles itself with the verse it
	 * navigated to, heading and body together. */
	main_display_bible(BIBLE, "Matthew 12:4");
	settle(1200);
	report("navigated-12-4");
	check(painted_has("<b>Mt 12:4</b>"),
	      "navigation must head the panel with Mt 12:4");
	check(painted_has("panes de la proposici"),
	      "navigation must show the body of Matthew 12:4");
	check(!painted_has("Es decir"),
	      "the clicked note's body must not survive navigation");

	/* 4. And a verse with no comment says so, rather than leaving the
	 * previous reference's body under a new heading. */
	main_display_bible(BIBLE, "Matthew 11:8");
	settle(1200);
	report("navigated-11-8");
	check(painted_has("no tiene comentario del autor"),
	      "a verse with no comment must say so");
	check(!painted_has("panes de la proposici"),
	      "a verse with no comment must not keep the previous body");

	g_printerr("author_commentary_probe_failures=%d\n", failures);
	gtk_main_quit();
	return FALSE;
}

void
author_commentary_probe_schedule(void)
{
	if (!probe_requested())
		return;

	/* The probe asserts against this edition's real notes; without
	 * them there is nothing to reproduce. */
	if (!main_is_module((char *)BIBLE) ||
	    !main_is_module((char *)COMMENTARY)) {
		g_printerr("author_commentary_probe_skipped=no-" BIBLE "\n");
		g_printerr("author_commentary_probe_failures=0\n");
		g_idle_add(quit_now, NULL);
		return;
	}
	g_idle_add(run, NULL);
}
