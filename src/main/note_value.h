/*
 * Biblia Elim
 * note_value.h - la forma en que se guarda cada nota
 *
 * Etiqueta: "<módulo> <osisref>" o "<módulo> <osisref>#<id>".
 * Valor:    "color|<texto escapado>|<nota escapada>|<pos o vacío>".
 *
 * '|' es un separador seguro: g_uri_escape_string() codifica todo lo que
 * no es «unreserved» de RFC 3986. Texto vacío = nota del versículo
 * entero; pos < 0 = sin posición (versículo entero, o subrayados de antes
 * de guardarla). Lo comparten display.cc y la exportación.
 */
#ifndef BIBLIA_ELIM_NOTE_VALUE_H
#define BIBLIA_ELIM_NOTE_VALUE_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

gchar *encode_note_value(const gchar *color, const gchar *text,
			 const gchar *note, gint pos);
gboolean decode_note_value(const gchar *value, gchar **color, gchar **text,
			   gchar **note, gint *pos);

/* "<módulo> <osisref>[#<id>]" en sus partes (cada una se libera; `id`
 * queda NULL si no hay). FALSE si la etiqueta no tiene esa forma. */
gboolean note_label_split(const gchar *label, gchar **module,
			  gchar **osisref, gchar **id);

#ifdef __cplusplus
}
#endif

#endif /* BIBLIA_ELIM_NOTE_VALUE_H */
