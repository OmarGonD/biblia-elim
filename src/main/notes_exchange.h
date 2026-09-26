/*
 * Biblia Elim
 * notes_exchange.h - exportar e importar las notas (NOTES-EXPORT-101)
 *
 * Dos formas de sacar las notas de la aplicación:
 *
 *  - JSON: copia completa del almacén (notas, subrayados y enlaces, con
 *    sus fechas). Cada entrada lleva su etiqueta y su valor tal cual, que
 *    es lo que se importa, y además sus campos ya legibles (versión,
 *    pasaje, frase, nota, color) para quien la abra con otro programa.
 *  - Markdown: para leer e imprimir, por libros y en orden bíblico.
 *
 * No sabe de Sword ni de GTK: el pasaje legible y las fechas en texto los
 * pone quien llama. Por eso se prueba solo.
 */
#ifndef BIBLIA_ELIM_NOTES_EXCHANGE_H
#define BIBLIA_ELIM_NOTES_EXCHANGE_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NOTES_EXCHANGE_FORMAT "biblia-elim-notas"
#define NOTES_EXCHANGE_VERSION 1

/* El almacén abierto, en JSON. El llamador libera. */
gchar *notes_export_json(void);

typedef struct {
	guint added;	  /* notas que no estaban */
	guint identical;  /* ya estaban igual: no se toca nada */
	guint extra;	  /* otra nota del mismo versículo: se añade junto a
			   * la que ya había, no en su lugar */
	guint conflicts;  /* subrayado o enlace distinto con la misma
			   * identidad: se queda el que había */
	guint invalid;	  /* entradas que no tienen la forma de una nota */
	guint links_added;
} NotesImportResult;

/* Lee `json` entero antes de tocar nada: si no es una copia de notas
 * válida, devuelve FALSE con el motivo y el almacén queda igual. Si lo
 * es, añade lo que falta (nunca borra ni sustituye una nota) y cuenta en
 * `result` qué hizo. No guarda: eso lo decide quien llama. */
gboolean notes_import_json(const gchar *json, NotesImportResult *result,
			   GError **error);

/* Una nota tal como se imprime. */
typedef struct {
	const gchar *libro;	/* «Salmos»: encabezado de sección */
	const gchar *pasaje;	/* «Salmos 23:1» */
	const gchar *modulo;
	const gchar *frase;	/* frase subrayada, o NULL */
	const gchar *nota;
	const gchar *fechas;	/* «Escrita el …», o NULL */
} NotesMdItem;

/* `items`: GList de NotesMdItem*, ya en orden bíblico. `fecha`: el día
 * de la exportación, en texto. El llamador libera. */
gchar *notes_export_markdown(GList *items, const gchar *fecha);

#ifdef __cplusplus
}
#endif

#endif /* BIBLIA_ELIM_NOTES_EXCHANGE_H */
