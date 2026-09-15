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
	CHECK(src && strstr(src, "picker_entry_focus_on_open(popover, entry,") != NULL);
	CHECK(src && strstr(src, "picker_focus_entry_later(p->popover, p->entry)") !=
			  NULL);
	/* the number field keeps the system input method */
	CHECK(src && strstr(src, "GTK_INPUT_PURPOSE_DIGITS") != NULL);
	CHECK(src && strstr(src, "gtk-im-context-simple") == NULL);
	CHECK(src && strstr(src, "g_object_set(p->entry, \"im-module\"") == NULL);
	/* GtkPopover must record a real widget to give the focus back to,
	 * before both number and book pickers pop up */
	{
		const char *first = src ? strstr(src, "picker_entry_settle_window_focus(") : NULL;
		CHECK(first != NULL);
		CHECK(first && strstr(first + 1, "picker_entry_settle_window_focus(") != NULL);
	}
	/* no default-priority idle left to ask for the focus */
	CHECK(src && strstr(src, "picker_focus_entry_idle") == NULL);
	g_free(src);

	CHECK(g_file_get_contents(SRCDIR "/src/main/picker_entry.c", &src, NULL,
				  NULL));
	CHECK(src && strstr(src, "\"map\"") != NULL);
	/* GtkWindow as its own focus is repaired as it happens; the handler is
	 * tied to the entry, never to the popover (GtkPopover disconnects
	 * everything on the window whose data is the popover) */
	CHECK(src && strstr(src, "\"set-focus\"") != NULL);
	CHECK(src && strstr(src, "G_CONNECT_AFTER") != NULL);
	CHECK(src && strstr(src, "on_window_set_focus), entry,") != NULL);
	/* one request at map: no retry loop, no stored handler ids */
	CHECK(src && strstr(src, "gtk_widget_add_tick_callback") == NULL);
	CHECK(src && strstr(src, "g_signal_handler_disconnect") == NULL);
	CHECK(src && strstr(src, "im-module") == NULL);
	CHECK(src && strstr(src, "g_idle_add") == NULL);
	CHECK(src && strstr(src, "g_timeout_add") == NULL);
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
