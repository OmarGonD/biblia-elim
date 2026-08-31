/*
 * Biblia Elim — Biblia interlineal (griego / hebreo)
 */

#ifndef __INTERLINEAL_H__
#define __INTERLINEAL_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _interl_strong {
	gchar *num;
	gchar *lema;
	gchar *translit;
	gchar *glosa;
	gchar *raiz;
	gchar *definicion;
} InterlStrong;

typedef struct _interl_tok {
	gchar *forma;
	gchar *strong;
	gchar *strongs;
	gchar *raiz;
	gchar *glosa;
	gchar *translit;
	gchar *morph;
} InterlTok;

/* Una fila del aparato interlineal (Forward u Reverse). */
typedef struct _interl_fila {
	gchar *es;
	gchar *strong;
	gchar *strongs;
	gchar *forma;
	gchar *raiz;
	gchar *translit;
	gchar *morph;
	gchar *morph_es;
	gboolean phrase;
	gboolean hebrew;
} InterlFila;

void main_interlineal_init(void);
void main_interlineal_shutdown(void);

const InterlStrong *main_interlineal_strong(const char *num);

/* Tokens del versículo (InterlTok*). Liberar con main_interlineal_tokens_free. */
GList *main_interlineal_versiculo(const char *key);
void main_interlineal_tokens_free(GList *lista);

/* Hasta max claves (gchar*) en un solo módulo (G→Tisch, H→KJV).
 * Libro actual primero. Caller g_list_free_full(..., g_free). */
GList *main_interlineal_ocurrencias(const char *strong, int max);
gboolean main_interlineal_indice_listo(void);
void main_interlineal_empezar_indice(void);
/* Cita corta en español: "1 Ti 1:1". Caller g_free. */
gchar *main_interlineal_cita_es(const char *key);

void main_interlineal_actualizar(void);
void main_verse_tools_xrefs(const char *key);
void main_interlineal_abrir_verso(const char *key);
void main_interlineal_cerrar_verso(void);
const char *main_interlineal_verso_abierto(void);
gboolean main_interlineal_bloquea_navegacion(void);
/* Si el interlineal abierto queda a 10+ versículos, lo pliega.
 * TRUE si lo cerró (hay que redibujar el capítulo). La lectura sigue. */
gboolean main_interlineal_quizas_plegar(const char *key);
/* HTML del original (griego/hebreo) si este versículo está abierto; si no, NULL. */
gchar *main_interlineal_html_original(const char *key);

/* Filas palabra-por-palabra. reverse=FALSE: orden del original;
 * reverse=TRUE: orden del español. Liberar con main_interlineal_filas_free. */
GList *main_interlineal_filas(const char *key, gboolean reverse);
void main_interlineal_filas_free(GList *filas);
gboolean main_interlineal_modo_reverse(void);
void main_interlineal_set_modo_reverse(gboolean reverse);
gchar *main_interlineal_norm_strong(const char *s);
/* Pie de la tabla: NULL si el español es el de la Biblia abierta. */
const char *main_interlineal_pie_es(void);

#ifdef __cplusplus
}
#endif
#endif
