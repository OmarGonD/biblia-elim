/*
 * Contract of the "Comentarios del autor" pane.
 *
 * These are wiring invariants, checked against the sources the way
 * comm_panel_close_test.c already does for this same panel: the state
 * they describe lives in GTK widgets built by the running application,
 * so what is pinned here is that the code keeps exactly one way to open
 * the pane and one reliable condition for refreshing it.  The routing
 * decision itself is tested for real in note_action_test.c, and the
 * commentary content in author_commentary_content_test.cc.
 */
#include <glib.h>

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                     \
	do {                                                                 \
		if (!(condition)) {                                           \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__,         \
				__LINE__, #condition);                        \
			failures++;                                           \
		}                                                             \
	} while (0)

static gchar *
load(const char *relative)
{
	gchar *path = g_build_filename(SRCDIR, relative, NULL);
	gchar *text = NULL;
	if (!g_file_get_contents(path, &text, NULL, NULL)) {
		fprintf(stderr, "FAIL cannot read %s\n", path);
		failures++;
	}
	g_free(path);
	return text;
}

/* Occurrences of `needle` in `haystack`. */
static int
count(const char *haystack, const char *needle)
{
	int n = 0;
	const char *p = haystack;
	while (p && (p = strstr(p, needle)) != NULL) {
		n++;
		p += strlen(needle);
	}
	return n;
}

/* The slice of `haystack` from `open` up to the next `close`, so a check
 * can say "not anywhere in this block" rather than "not anywhere in this
 * file" -- settings.currentverse is perfectly legitimate elsewhere. */
static gchar *
region(const char *haystack, const char *open, const char *close)
{
	const char *start = haystack ? strstr(haystack, open) : NULL;
	const char *end = start ? strstr(start + strlen(open), close) : NULL;

	if (!start || !end) {
		fprintf(stderr, "FAIL no region %s .. %s\n", open, close);
		failures++;
		return NULL;
	}
	return g_strndup(start, (gsize)(end - start));
}

int
main(void)
{
	gchar *sword = load("src/main/sword.cc");
	gchar *url = load("src/main/url.cc");
	gchar *sword_h = load("src/main/sword.h");
	gchar *window = load("src/gtk/main_window.c");
	gchar *display = load("src/main/display.cc");

	if (sword) {
		/* B. State: the refresh condition is the pane's real
		 * visibility, never settings.showcomms, which is a request
		 * to open and drifts from the widget. */
		CHECK(strstr(sword, "author_commentary_pane_is_live") != NULL);
		CHECK(strstr(sword,
			     "gtk_widget_get_visible(widgets.notebook_comm_book)") !=
		      NULL);
		CHECK(strstr(sword, "gtk_notebook_get_current_page") != NULL);
		/* the old gate must be gone from this pane's path */
		CHECK(strstr(sword,
			     "if (!settings.showcomms || !settings.comm_showing)") ==
		      NULL);

		/* C. Navigation: exactly one call site keeps an open pane in
		 * step, and it never opens the pane by itself. */
		CHECK(count(sword, "main_display_author_commentary(") == 2);
		CHECK(strstr(sword,
			     "if (!author_commentary_pane_is_live())\n\t\treturn;") !=
		      NULL);

		/* The one way in. It must go through gui_show_hide_comms(),
		 * which is the only writer of settings.showcomms, rather
		 * than showing the widget behind the flags' back. */
		CHECK(strstr(sword, "gboolean main_show_author_commentary(") !=
		      NULL);
		CHECK(strstr(sword, "gui_show_hide_comms(TRUE)") != NULL);
		CHECK(strstr(sword, "settings.comm_showing = TRUE;") != NULL);
		CHECK(count(sword, "gtk_widget_show(widgets.notebook_comm_book)") ==
		      0);

		/* Module and reference come from the existing neutral
		 * infrastructure, not from a name written into a handler. */
		CHECK(strstr(sword, "author_commentary_for_bible(bible)") != NULL);
		CHECK(strstr(sword,
			     "main_reference_for_module(bible, key, commentary)") !=
		      NULL);

		/* A verse the author never commented says so explicitly
		 * instead of leaving a heading over an empty body. */
		CHECK(strstr(sword, "author_commentary_has_text") != NULL);
		CHECK(strstr(sword,
			     "no tiene comentario del autor") != NULL);

		/* Neither does the pane itself: author_commentary_render()
		 * and the one way in leave settings.currentverse alone. */
		{
			gchar *open = region(sword,
					     "gboolean main_show_author_commentary(",
					     "void main_display_dictionary(");
			if (open) {
				CHECK(strstr(open, "settings.currentverse") ==
				      NULL);
				g_free(open);
			}
		}
	}

	if (display) {
		/* F. The heading names the verse it is showing.
		 *
		 * The pane's body comes from the module key that
		 * main_display_commentary() set; the heading used to come
		 * from settings.currentverse instead, so a clicked note of
		 * Matthew 11:6 rendered its comment under "Mt 11:5" while
		 * the Bible pane still focused 11:5.  Both halves now read
		 * the one key, off the module itself -- no new global, and
		 * no reason to move the Bible pane to fix a title. */
		gchar *heading = region(display, "if (author_commentary) {",
					"\n\t} else {");

		CHECK(strstr(display,
			     "static gchar *author_commentary_heading_key(SWModule &imodule)") !=
		      NULL);
		CHECK(strstr(display, "key->getOSISRef()") != NULL);

		if (heading) {
			CHECK(strstr(heading,
				     "author_commentary_heading_key(imodule)") !=
			      NULL);
			CHECK(strstr(heading,
				     "main_interlineal_cita_es(heading_key)") !=
			      NULL);
			/* the bug itself: the focused verse has no say here */
			CHECK(strstr(heading, "settings.currentverse") == NULL);
			g_free(heading);
		}
	}

	if (sword_h) {
		CHECK(strstr(sword_h,
			     "gboolean main_show_author_commentary(const char *bible, const char *key);") !=
		      NULL);
	}

	if (url) {
		/* A. The clicked editorial marker routes to the pane. */
		CHECK(strstr(url, "main_note_action_for(stype, clicked)") != NULL);
		CHECK(strstr(url, "case NOTE_ACTION_AUTHOR_COMMENTARY:") != NULL);
		CHECK(strstr(url, "main_show_author_commentary(module, passage)") !=
		      NULL);
		/* and it opens the pane on the clicked reference without
		 * navigating the Bible pane there: the clicked marker is an
		 * aside, so nothing on this path writes the focus. */
		{
			gchar *clicked = region(url,
						"case NOTE_ACTION_AUTHOR_COMMENTARY:",
						"case NOTE_ACTION_NOTE_PREVIEW:");
			if (clicked) {
				CHECK(strstr(clicked,
					     "settings.currentverse = ") == NULL);
				g_free(clicked);
			}
		}

		/* The commentary module is never hardcoded in the router. */
		CHECK(strstr(url, "SpaPlatenseComentarios") == NULL);
		CHECK(strstr(url, "NacarColungaNotas") == NULL);
		CHECK(strstr(url, "TorresAmatNotas") == NULL);

		/* E. The other link types keep their own dispatch. */
		CHECK(strstr(url, "case NOTE_ACTION_CROSSREF_LIST:") != NULL);
		CHECK(strstr(url, "main_display_verse_list_in_sidebar") != NULL);
		CHECK(strstr(url, "case NOTE_ACTION_NOTE_PREVIEW:") != NULL);
		CHECK(strstr(url, "main_information_viewer") != NULL);
		CHECK(strstr(url, "showNeutralWord") != NULL);
		CHECK(strstr(url, "showNeutralStrong") != NULL);
		CHECK(strstr(url, "showStrongs") != NULL);
		CHECK(strstr(url, "showMorph") != NULL);
		CHECK(strstr(url, "showRef") != NULL);
		/* and the pane is not opened from any of them */
		CHECK(count(url, "main_show_author_commentary") == 1);
	}

	if (window) {
		/* D. Closing goes through the same single writer, so the
		 * widget and settings.showcomms end up agreeing and the
		 * pane cannot reappear on the next navigation. */
		CHECK(strstr(window, "void gui_close_comms_panel(void)") != NULL);
		CHECK(strstr(window, "gui_show_hide_comms(FALSE)") != NULL);
		CHECK(strstr(window, "settings.showcomms = choice;") != NULL);
		/* gui_show_hide_comms() remains the only place in the GTK
		 * layer that assigns the flag. (settings.c also forces it
		 * to 0 once at startup, on purpose and with its reason
		 * written there; the trailing " = " keeps this from
		 * matching the "== FALSE" comparisons.) */
		CHECK(count(window, "settings.showcomms = ") == 1);
	}

	g_free(sword);
	g_free(url);
	g_free(sword_h);
	g_free(window);
	g_free(display);
	printf("author_commentary_panel_failures=%d\n", failures);
	return failures ? 1 : 0;
}
