/*
 * Biblia Elim
 * pulpito_pasos.h - el bosquejo convertido en pasos de entrega
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __PULPITO_PASOS_H__
#define __PULPITO_PASOS_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Esta parte no sabe nada de Sword ni de GTK a propósito: es la que
 * decide qué se ve y en qué orden, y por eso es la que se prueba sola.
 * Quien la usa le pasa un resolutor con las dos cosas que aquí no se
 * pueden saber: si una línea es una referencia y qué dice ese texto. */

typedef enum {
	PU_TITULO = 0,	/* el enunciado de un punto */
	PU_VERSO,	/* el texto base del punto */
	PU_VINETAS,	/* dos a cinco frases cortas */
	PU_APOYO,	/* un versículo de apoyo, de uno en uno */
	PU_ILUSTRACION	/* una línea, visualmente secundaria */
} PU_TIPO;

typedef struct {
	PU_TIPO tipo;
	int nivel;		/* profundidad en el bosquejo, 1 arriba */
	int titulo_de;		/* índice del paso PU_TITULO al que pertenece */
	gchar *titulo;		/* PU_TITULO */
	gchar *ref;		/* PU_VERSO y PU_APOYO: "Efesios 2:5" */
	gchar *texto;		/* el texto resuelto, o NULL si no se pudo */
	GPtrArray *vinetas;	/* PU_VINETAS: gchar* */
	gchar *ilustracion;	/* PU_ILUSTRACION */
	/* Lo que el bosquejo apuntó con "Nota:" para este paso. Lo ve
	 * quien predica y no sale nunca a la pantalla de la congregación.
	 * Varias notas seguidas van en renglones, separadas por \n. */
	gchar *nota;
} PU_PASO;

typedef struct {
	gchar *modulo;		/* el genbook del bosquejo */
	gchar *titulo;
	gchar *ref_base;	/* la referencia del sermón, si se pudo deducir */
	gchar *version;		/* módulo bíblico con el que se predica */
	GPtrArray *pasos;	/* PU_PASO* en el orden en que se entregan */
} PU_SERMON;

/* Lo que el constructor de pasos no puede saber por su cuenta. Ambas
 * devuelven memoria de g_malloc, o NULL. */
typedef struct {
	gchar *(*ref_valida)(const char *linea, gpointer datos);
	gchar *(*texto_de)(const char *ref, gpointer datos);
	gpointer datos;
} PU_RESOLUTOR;

/* --- construir --- */

PU_SERMON *pu_sermon_nuevo(const char *modulo, const char *titulo,
			   const char *version);
void pu_sermon_libre(PU_SERMON *sermon);

/* Añade el enunciado de un punto y devuelve su índice. */
int pu_sermon_titulo(PU_SERMON *sermon, const char *enunciado, int nivel);

/* Reparte el contenido de ese punto en los pasos que le siguen. El
 * convenio es el que sale solo de escribir un bosquejo:
 *
 *   - una línea que es solo una referencia -> el texto base del punto
 *   - "Apoyo: <ref>"                       -> versículo de apoyo
 *   - "Ilustración: <frase>"               -> la ilustración
 *   - "Nota: <lo que sea>"                 -> nota del predicador
 *   - cualquier otra línea                 -> una viñeta
 *
 * La nota no es un paso: se pega al paso que viene detrás, que es donde
 * uno la quiere ver ("Nota: aquí despacio" delante de las viñetas sale
 * con las viñetas). Si detrás no queda nada, se pega al último paso del
 * punto, y si el punto no tuvo ninguno, a su enunciado.
 */
void pu_sermon_contenido(PU_SERMON *sermon, const char *html, int nivel,
			 int titulo_de, const PU_RESOLUTOR *res);

/* Quita las etiquetas del editor y deja los saltos de línea. */
gchar *pu_sin_etiquetas(const char *html);

/* --- recorrer --- */

int pu_total(PU_SERMON *sermon);
PU_PASO *pu_paso(PU_SERMON *sermon, int i);

/* Todas devuelven el índice al que hay que ir; nunca se salen del
 * bosquejo ni devuelven -1, para que el púlpito no tenga que pensar. */
int pu_siguiente(PU_SERMON *sermon, int i);
int pu_anterior(PU_SERMON *sermon, int i);
int pu_primero(PU_SERMON *sermon);
int pu_ultimo(PU_SERMON *sermon);
/* El siguiente (1) o anterior (-1) enunciado de punto. */
int pu_titulo_cercano(PU_SERMON *sermon, int i, int direccion);

/* El último versículo visto desde ese paso hacia atrás, o -1. */
int pu_verso_visible(PU_SERMON *sermon, int i);
/* Los pasos de apoyo del punto en el que está ese paso. GArray de int. */
GArray *pu_apoyos_de(PU_SERMON *sermon, int i);
/* La ilustración de ese punto, o -1. */
int pu_ilustracion_de(PU_SERMON *sermon, int i);

/* Un renglón que resume el paso: para el pie y para el bosquejo. */
gchar *pu_resumen(PU_PASO *paso);

/* --- por dónde se iba --- */

/* El paso guardado, ya dentro del bosquejo. */
int pu_paso_guardado(PU_SERMON *sermon, int guardado);
/* Solo se pregunta si de verdad se quedó a medias. */
gboolean pu_preguntar_continuar(PU_SERMON *sermon, int guardado);

#ifdef __cplusplus
}
#endif
#endif
