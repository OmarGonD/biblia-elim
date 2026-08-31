/*
 * Biblia Elim — diccionario / léxico offline
 */

#ifndef __DICCIONARIO_H__
#define __DICCIONARIO_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _dicc_estudio {
	gchar *autor;
	gchar *titulo;
	gchar *texto;
} DiccEstudio;

typedef struct _dicc_entrada {
	gchar *titulo;
	gchar *definicion;
	gchar *referencias;
	gchar **claves; /* NULL-terminated */
	GList *estudios; /* DiccEstudio* */
} DiccEntrada;

typedef struct _dicc_comentario {
	gchar *autor;
	gchar *modulo;
	gchar *descripcion;
	gchar *extracto;
} DiccComentario;

void main_diccionario_init(void);
void main_diccionario_shutdown(void);

/* Lista de títulos (gchar*) que coinciden con el texto tecleado. */
GList *main_diccionario_sugerencias(const char *texto);

/* Busca por clave o título. Caller no libera la entrada. */
const DiccEntrada *main_diccionario_buscar(const char *palabra);

/* Comentarios Sword del pasaje actual, agrupables por autor. Caller libera con main_diccionario_comentarios_free. */
GList *main_diccionario_comentarios(const char *clave_versiculo);
void main_diccionario_comentarios_free(GList *lista);

#ifdef __cplusplus
}
#endif
#endif
