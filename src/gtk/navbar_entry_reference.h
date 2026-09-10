#ifndef BIBLIA_ELIM_NAVBAR_ENTRY_REFERENCE_H
#define BIBLIA_ELIM_NAVBAR_ENTRY_REFERENCE_H

#include <glib.h>

typedef struct {
	gchar *key;
	gchar *anchor;
} NavbarEntryReference;

NavbarEntryReference navbar_entry_reference_parse(const gchar *entry_text);
void navbar_entry_reference_clear(NavbarEntryReference *reference);

#endif
