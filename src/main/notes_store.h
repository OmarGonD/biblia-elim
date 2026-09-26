/*
 * Biblia Elim
 * notes_store.h - almacén propio de las notas del lector
 *
 * Las notas y sus enlaces vivían dentro de settings.xml, mezclados con la
 * configuración: cada autoguardado reescribía el archivo entero y un
 * reinicio de la configuración las borraba (NOTES-STORE-101). Aquí tienen
 * su propio archivo, notas.xml, junto a settings.xml, escrito de forma
 * atómica (temporal + fsync + rename) y con copias rotativas: una por
 * sesión, las NOTES_STORE_BACKUPS más recientes.
 *
 * Es un almacén de pares etiqueta -> valor por sección, en orden de
 * inserción, con la misma semántica que las secciones «osisrefnotes» y
 * «osisrefnotelinks» del XML de configuración: quien lo usa (display.cc)
 * no cambia de formato de etiqueta ni de valor.
 */
#ifndef BIBLIA_ELIM_NOTES_STORE_H
#define BIBLIA_ELIM_NOTES_STORE_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	NOTES_SECTION_NOTES,	/* «osisrefnotes»: nota o subrayado */
	NOTES_SECTION_LINKS,	/* «osisrefnotelinks»: enlaces entre notas */
} NotesSection;

#define NOTES_STORE_BACKUPS 5

typedef struct {
	gchar *label;
	gchar *value;
	/* Segundos Unix (UTC) en que se escribió y en que cambió por última
	 * vez su valor; 0 si no se sabe (notas anteriores a NOTES-DATES-101).
	 * Los pone el almacén: quien escribe no tiene que acordarse. */
	gint64 created;
	gint64 modified;
} NotesEntry;

/* Abre el almacén en `path`. Devuelve TRUE si el archivo ya existía (se
 * ha cargado); FALSE si no existe o no se pudo leer, en cuyo caso el
 * almacén queda vacío y el llamador decide si importar datos antiguos.
 * Un archivo que existe pero no se puede leer NO se sobrescribe: se
 * aparta como «.ilegible-<fecha>» antes de nada. */
gboolean notes_store_open(const gchar *path);
gboolean notes_store_is_open(void);
const gchar *notes_store_path(void);
void notes_store_close(void);

/* Valor (el llamador libera) o NULL. */
gchar *notes_store_get(NotesSection section, const gchar *label);
/* Una etiqueta nueva recibe fecha de creación y de modificación; una
 * existente cuyo valor cambia, fecha de modificación. Escribir el mismo
 * valor no cambia nada. */
void notes_store_set(NotesSection section, const gchar *label,
		     const gchar *value);
/* Igual, con las fechas dadas (importar una copia conserva las suyas). */
void notes_store_set_full(NotesSection section, const gchar *label,
			  const gchar *value, gint64 created,
			  gint64 modified);
/* La entrada (el llamador la libera con notes_entry_free) o NULL. */
NotesEntry *notes_store_lookup(NotesSection section, const gchar *label);
void notes_entry_free(NotesEntry *entry);
/* Cambia cada vez que cambia el contenido: quien guarda algo calculado a
 * partir de las notas sabe así cuándo rehacerlo. */
guint64 notes_store_generation(void);
/* Fechas del almacén en texto: ISO 8601 en UTC («2026-09-25T18:04:05Z»).
 * 0 <-> NULL/vacío o ilegible. */
gchar *notes_time_to_iso(gint64 t);
gint64 notes_time_from_iso(const gchar *text);
/* Copia exacta del archivo actual en `dest` (antes de importar). */
gboolean notes_store_copy_to(const gchar *dest);
void notes_store_remove(NotesSection section, const gchar *label);

/* Copia de las entradas en orden: se puede modificar el almacén mientras
 * se recorre. Liberar con g_ptr_array_unref(). */
GPtrArray *notes_store_entries(NotesSection section);
guint notes_store_count(NotesSection section);

/* Escribe el archivo de forma atómica. En la primera escritura de la
 * sesión, antes, rota las copias (.bak.1 la más reciente). FALSE si no se
 * pudo escribir; los datos siguen en memoria. */
gboolean notes_store_save(void);

#ifdef __cplusplus
}
#endif

#endif /* BIBLIA_ELIM_NOTES_STORE_H */
