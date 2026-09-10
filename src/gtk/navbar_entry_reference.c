#include "navbar_entry_reference.h"

#include <string.h>

NavbarEntryReference
navbar_entry_reference_parse(const gchar *entry_text)
{
	NavbarEntryReference reference = { NULL, NULL };
	gchar *delimiter;

	if (entry_text == NULL)
		return reference;

	reference.key = g_strdup(entry_text);
	delimiter = strpbrk(reference.key, "#!");
	if (delimiter != NULL) {
		reference.anchor = g_strdup(delimiter);
		*delimiter = '\0';
	}

	return reference;
}

void
navbar_entry_reference_clear(NavbarEntryReference *reference)
{
	if (reference == NULL)
		return;

	g_clear_pointer(&reference->key, g_free);
	g_clear_pointer(&reference->anchor, g_free);
}
