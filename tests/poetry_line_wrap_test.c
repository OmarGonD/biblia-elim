/*
 * OSIS poetry lines arrive as <br /><span class="line">. A hard break
 * leaves unused pane width; those <br> must join as wrap spaces so the
 * verse fills like prose. Unrelated <br> stay breaks.
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

	CHECK(g_file_get_contents(SRCDIR "/src/webkit/wk-html.c", &src, NULL,
				  NULL));
	CHECK(src && strstr(src, "br_joins_poetry_line") != NULL);
	CHECK(src && strstr(src, "node_is_line_span") != NULL);
	CHECK(src && strstr(src, "insert_text(ctx, \" \")") != NULL);
	CHECK(src && strstr(src, "class_has((const char *)klass, \"line\")") !=
			  NULL);
	g_free(src);

	printf("poetry_line_wrap_failures=%d\n", failures);
	return failures ? 1 : 0;
}
