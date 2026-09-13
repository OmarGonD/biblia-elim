/*
 * The chapter/verse number field in the picker must receive focus after the
 * popover maps. Grabbing the navbar toggle instead leaves the entry looking
 * locked: keys go to the button, the placeholder "Capítulo 1–N" stays.
 */
#include <glib.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__,          \
				__LINE__, #cond);                              \
			failures++;                                            \
		}                                                              \
	} while (0)

int
main(void)
{
	gchar *src = NULL;
	gchar *gtk_src = NULL;

	CHECK(g_file_get_contents(SRCDIR "/src/main/navbar_versekey.cc", &src,
				  NULL, NULL));
	CHECK(src && strstr(src, "picker_focus_entry_later") != NULL);
	CHECK(src && strstr(src, "g_signal_connect(popover, \"map\"") != NULL);
	CHECK(src && strstr(src, "picker_focus_entry_later(p->popover, p->entry)") !=
			  NULL);
	g_free(src);

	CHECK(g_file_get_contents(SRCDIR "/src/gtk/navbar_versekey.c", &gtk_src,
				  NULL, NULL));
	CHECK(gtk_src &&
	      strstr(gtk_src, "select_chapter_button_press_callback") != NULL);
	/* The chapter/verse/book press handlers must not steal focus onto the
	 * toggle before the popover entry can take it. */
	{
		const char *fn = strstr(gtk_src,
					"select_chapter_button_press_callback");
		const char *end = fn ? strstr(fn, "select_verse_button_press_callback")
				     : NULL;
		gchar *body = NULL;

		CHECK(fn != NULL && end != NULL && end > fn);
		if (fn && end && end > fn) {
			body = g_strndup(fn, (gsize)(end - fn));
			CHECK(strstr(body, "gtk_widget_grab_focus(widget)") ==
			      NULL);
			g_free(body);
		}
	}
	g_free(gtk_src);

	printf("navbar_picker_focus_failures=%d\n", failures);
	return failures ? 1 : 0;
}
