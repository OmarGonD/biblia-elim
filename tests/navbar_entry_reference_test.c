#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "navbar_entry_reference.h"

static int failures;

#define CHECK(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "check failed at line %d: %s\n", __LINE__, \
			#condition); \
		failures++; \
	} \
} while (0)

static void
check_parse(const gchar *input, const gchar *expected_key,
	    const gchar *expected_anchor)
{
	gchar *entry_storage = g_strdup(input);
	gchar *original = g_strdup(input);
	NavbarEntryReference reference =
	    navbar_entry_reference_parse(entry_storage);

	CHECK(reference.key != entry_storage);
	CHECK(reference.key != NULL);
	CHECK(strcmp(entry_storage, original) == 0);
	/* The parse owns both results; replacing or releasing the GtkEntry's
	 * backing storage cannot invalidate navigation state. */
	g_free(entry_storage);
	entry_storage = NULL;
	CHECK(reference.key && strcmp(reference.key, expected_key) == 0);
	if (expected_anchor) {
		CHECK(reference.anchor != NULL);
		CHECK(reference.anchor &&
		      strcmp(reference.anchor, expected_anchor) == 0);
	} else {
		CHECK(reference.anchor == NULL);
	}

	navbar_entry_reference_clear(&reference);
	CHECK(reference.key == NULL);
	CHECK(reference.anchor == NULL);
	g_free(original);
}

int
main(void)
{
	check_parse("John 3:16", "John 3:16", NULL);
	check_parse("John 3:16#note-4", "John 3:16", "#note-4");
	check_parse("John 3:16!osis-ref", "John 3:16", "!osis-ref");
	check_parse("John 3:16!first#second", "John 3:16", "!first#second");

	NavbarEntryReference empty = navbar_entry_reference_parse(NULL);
	CHECK(empty.key == NULL);
	CHECK(empty.anchor == NULL);
	navbar_entry_reference_clear(&empty);

	printf("navbar_entry_reference_failures=%d\n", failures);
	return failures ? 1 : 0;
}
