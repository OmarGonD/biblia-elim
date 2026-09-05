/*
 * Biblia Elim
 * pulpito.cc - la vista de púlpito: entregar el sermón, no estudiarlo
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <swmgr.h>
#include <swmodule.h>
#include <listkey.h>
#include <treekeyidx.h>
#include <versekey.h>

#include "main/pulpito.h"
#include "main/lists.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/xml.h"
#include "backend/sword_main.hh"

#include "gui/debug_glib_null.h"

using namespace sword;

#define PU_SECCION "pulpito"

/* Un rango de púlpito es un texto base, no un capítulo entero de
 * estudio: con este tope, un "Salmos 119" mal escrito no deja la
 * pantalla en blanco durante medio minuto. */
#define PU_MAX_VERSOS 200

/* --------------------------------------------------------------------
 * Las referencias
 * ------------------------------------------------------------------ */

static SWModule *
biblia(const char *version)
{
	if (!version || !*version || !backend)
		return NULL;
	return backend->get_SWModule(version);
}

/* La clave del módulo, para copiarla: trae la versificación y los
 * nombres de libro del idioma en que se está trabajando. */
static gboolean
clave_de(const char *version, VerseKey *destino)
{
	SWModule *mod = biblia(version);
	VerseKey *clave;

	if (!mod)
		return FALSE;
	clave = dynamic_cast<VerseKey *>((SWKey *)(*mod));
	if (!clave)
		return FALSE;	/* no es una Biblia: no hay versículos */
	*destino = *clave;
	return TRUE;
}

/* Los números de versículo, en volado, cuando la cita es un rango. */
static void
numero_volado(GString *out, int verso)
{
	static const char *volados[] = {"⁰", "¹", "²", "³", "⁴",
					"⁵", "⁶", "⁷", "⁸", "⁹"};
	gchar *digitos = g_strdup_printf("%d", verso);
	int i;

	for (i = 0; digitos[i]; ++i)
		g_string_append(out, volados[digitos[i] - '0']);
	g_string_append_c(out, ' ');
	g_free(digitos);
}

/* Cómo se escribe la cita ya resuelta: "Efesios 2:5", "Efesios 2:4-5" o
 * "Efesios 2:20 - 3:2". */
static gchar *
etiqueta_de(ListKey &lista)
{
	VerseKey *primero = dynamic_cast<VerseKey *>(lista.getElement(0));
	VerseKey lo, hi;

	if (!primero)
		return g_strdup(lista.getElement(0)
				    ? (const char *)lista.getElement(0)->getText()
				    : "");
	lo = primero->getLowerBound();
	hi = primero->getUpperBound();
	if (!g_strcmp0((const char *)lo.getText(), (const char *)hi.getText()))
		return g_strdup((const char *)lo.getText());
	if (lo.getBook() == hi.getBook() && lo.getChapter() == hi.getChapter())
		return g_strdup_printf("%s-%d", (const char *)lo.getText(),
				       hi.getVerse());
	return g_strdup_printf("%s - %s", (const char *)lo.getText(),
			       (const char *)hi.getText());
}

/* Una línea que es solo una referencia bíblica y nada más. Se le
 * pregunta al motor en vez de adivinar con expresiones: él sabe los
 * nombres de libro en el idioma del módulo, y nosotros no. Devuelve la
 * cita ya escrita como se va a enseñar, o NULL. */
static gchar *
referencia_sola(const char *linea, const char *version)
{
	VerseKey clave;
	int palabras = 0;
	const char *p;
	gboolean dentro = FALSE;

	if (!linea || !*linea)
		return NULL;

	/* Una referencia no pasa de cuatro palabras ("1 Juan 4:7-10").
	 * Sin este corte, una frase que lleve dentro una cita se colaría
	 * entera como versículo: el motor busca referencias en medio del
	 * texto, y aquí solo valen las líneas que son la cita y nada
	 * más. */
	for (p = linea; *p; ++p) {
		if (g_ascii_isspace((unsigned char)*p))
			dentro = FALSE;
		else if (!dentro) {
			dentro = TRUE;
			if (++palabras > 4)
				return NULL;
		}
	}
	if (!palabras)
		return NULL;
	/* Y lleva un número: "Amor" no es una referencia. */
	if (!strpbrk(linea, "0123456789"))
		return NULL;

	if (!clave_de(version, &clave))
		return NULL;

	/* El motor deja el error puesto cuando no reconoce el libro; sin
	 * mirarlo, "Punto 1" se convertiría en Apocalipsis 1:1. */
	clave.setText(linea);
	if (clave.popError())
		return NULL;

	{
		VerseKey base;
		ListKey lista;

		if (!clave_de(version, &base))
			return NULL;
		lista = base.parseVerseList(linea, base, true);
		if (!lista.getCount())
			return NULL;
		return etiqueta_de(lista);
	}
}

gchar *
main_pulpito_texto(const char *version, const char *ref)
{
	SWModule *mod = biblia(version);
	VerseKey base, donde_estaba;
	ListKey lista;
	GPtrArray *trozos;
	GArray *numeros;
	GString *out;
	guint i;

	if (!mod || !ref || !*ref)
		return NULL;
	if (!clave_de(version, &base))
		return NULL;
	/* Leer un pasaje mueve la clave del módulo, y ese módulo es el que
	 * el lector tiene abierto en la ventana principal: se apunta dónde
	 * estaba para dejarlo igual. */
	donde_estaba = base;

	lista = base.parseVerseList(ref, base, true);
	if (!lista.getCount())
		return NULL;

	trozos = g_ptr_array_new_with_free_func(g_free);
	numeros = g_array_new(FALSE, FALSE, sizeof(int));
	for (lista.setPosition(TOP); !lista.popError(); lista.increment()) {
		const char *trozo;
		VerseKey *aqui;
		int numero;

		if (trozos->len >= PU_MAX_VERSOS)
			break;
		mod->setKey(lista);
		trozo = mod->stripText();
		if (!trozo || !*trozo)
			continue;
		aqui = dynamic_cast<VerseKey *>((SWKey *)(*mod));
		numero = aqui ? aqui->getVerse() : 0;
		g_ptr_array_add(trozos, g_strstrip(g_strdup(trozo)));
		g_array_append_val(numeros, numero);
	}

	mod->setKey(donde_estaba);

	if (!trozos->len) {
		g_ptr_array_free(trozos, TRUE);
		g_array_free(numeros, TRUE);
		return NULL;
	}

	out = g_string_new(NULL);
	for (i = 0; i < trozos->len; ++i) {
		if (i)
			g_string_append_c(out, ' ');
		/* Con un solo versículo el número sobra: ya está en la cita,
		 * debajo. En un rango hace falta para seguir la lectura. */
		if (trozos->len > 1)
			numero_volado(out, g_array_index(numeros, int, i));
		g_string_append(out, (const char *)g_ptr_array_index(trozos, i));
	}
	g_ptr_array_free(trozos, TRUE);
	g_array_free(numeros, TRUE);

	{
		gchar *texto = g_string_free(out, FALSE);

		g_strstrip(texto);
		if (!*texto) {
			g_free(texto);
			return NULL;
		}
		return texto;
	}
}

gchar *
main_pulpito_capitulo(const char *version, const char *ref)
{
	VerseKey clave;

	if (!ref || !*ref)
		return NULL;
	if (!clave_de(version, &clave))
		return NULL;
	clave.setText(ref);
	if (clave.popError())
		return NULL;
	return g_strdup_printf("%s %d", (const char *)clave.getBookName(),
			       clave.getChapter());
}

/* --------------------------------------------------------------------
 * El resolutor que usa el reparto en pasos
 * ------------------------------------------------------------------ */

static gchar *
res_ref(const char *linea, gpointer datos)
{
	return referencia_sola(linea, (const char *)datos);
}

static gchar *
res_texto(const char *ref, gpointer datos)
{
	return main_pulpito_texto((const char *)datos, ref);
}

/* --------------------------------------------------------------------
 * Recorrer el bosquejo
 * ------------------------------------------------------------------ */

/* El árbol, en el orden en que se predica: cada punto y debajo lo suyo,
 * y luego sus hijos. */
static void
recorrer(PU_SERMON *sermon, SWModule *mod, TreeKeyIdx nodo, int nivel,
	 const PU_RESOLUTOR *res)
{
	do {
		unsigned long offset = nodo.getOffset();
		const char *nombre;
		int titulo_de;

		if (offset == 0) {
			/* La raíz no es un punto: solo se baja por ella. */
			if (nodo.hasChildren()) {
				TreeKeyIdx hijo = nodo;

				if (hijo.firstChild())
					recorrer(sermon, mod, hijo, nivel, res);
			}
			continue;
		}

		nombre = nodo.getLocalName();
		titulo_de = pu_sermon_titulo(sermon, nombre, nivel);

		{
			TreeKeyIdx aqui = nodo;
			const char *crudo;

			mod->setKey(aqui);
			crudo = mod->getRawEntry();
			if (crudo && *crudo)
				pu_sermon_contenido(sermon, crudo, nivel,
						    titulo_de, res);
		}

		if (nodo.hasChildren()) {
			TreeKeyIdx hijo = nodo;

			if (hijo.firstChild())
				recorrer(sermon, mod, hijo, nivel + 1, res);
		}
	} while (nodo.nextSibling());
}

/* --------------------------------------------------------------------
 * Qué módulos son bosquejos
 *
 * Los esquemas que crea la aplicación se guardan como genbooks con
 * GSType=PrayerList, así que están en la lista de listas de oración y no
 * en la de libros. Se miran las dos: al púlpito puede subir tanto el
 * esquema propio como un libro con árbol de puntos.
 * ------------------------------------------------------------------ */

static void
juntar(GList **destino, gint tipo)
{
	GList *l;

	for (l = get_list(tipo); l; l = l->next) {
		const char *nombre = (const char *)l->data;
		GList *ya;

		if (!nombre || !*nombre)
			continue;
		for (ya = *destino; ya; ya = ya->next)
			if (!g_strcmp0((const char *)ya->data, nombre))
				break;
		if (!ya)
			*destino = g_list_append(*destino, g_strdup(nombre));
	}
}

GList *
main_pulpito_sermones(void)
{
	GList *lista = NULL;

	juntar(&lista, PRAYER_LIST);
	juntar(&lista, GBS_LIST);
	return lista;
}

gboolean
main_pulpito_es_sermon(const char *modulo)
{
	GList *lista, *l;
	gboolean si = FALSE;

	if (!modulo || !*modulo)
		return FALSE;
	lista = main_pulpito_sermones();
	for (l = lista; l && !si; l = l->next)
		si = !g_strcmp0((const char *)l->data, modulo);
	g_list_free_full(lista, g_free);
	return si;
}

/* --------------------------------------------------------------------
 * Abrir y cerrar
 * ------------------------------------------------------------------ */

static gboolean
es_biblia(const char *modulo)
{
	GList *l;

	if (!modulo || !*modulo)
		return FALSE;
	for (l = get_list(TEXT_LIST); l; l = l->next)
		if (!g_strcmp0((const char *)l->data, modulo))
			return TRUE;
	return FALSE;
}

/* La versión con la que se predica. Por este orden: la que se eligió
 * para este sermón, la Reina-Valera 1960 si está instalada, la del
 * lector si es una Biblia (puede estar en el interlineal griego, que no
 * es para leer en voz alta) y, si no, la primera que haya. */
static gchar *
version_de(const char *modulo)
{
	char *guardada = xml_get_list_from_label(PU_SECCION, "version",
						 (char *)modulo);
	GList *l;

	if (guardada && *guardada && es_biblia(guardada)) {
		gchar *r = g_strdup(guardada);

		g_free(guardada);
		return r;
	}
	g_free(guardada);

	for (l = get_list(TEXT_LIST); l; l = l->next) {
		const char *nombre = (const char *)l->data;
		gchar *bajo = g_ascii_strdown(nombre, -1);
		gboolean rvr = (strstr(bajo, "rvr1960") ||
				strstr(bajo, "rvr60") ||
				strstr(bajo, "reinavalera1960"));

		g_free(bajo);
		if (rvr)
			return g_strdup(nombre);
	}

	if (es_biblia(settings.MainWindowModule))
		return g_strdup(settings.MainWindowModule);

	l = get_list(TEXT_LIST);
	return l ? g_strdup((const char *)l->data) : NULL;
}

PU_SERMON *
main_pulpito_abrir(const char *modulo)
{
	SWMgr *mgr;
	SWModule *mod;
	PU_SERMON *sermon;
	PU_RESOLUTOR res;
	gchar *version;

	if (!modulo || !*modulo || !backend)
		return NULL;
	mgr = backend->get_mgr();
	if (!mgr)
		return NULL;
	mod = mgr->getModule(modulo);
	if (!mod)
		return NULL;

	version = version_de(modulo);
	sermon = pu_sermon_nuevo(modulo,
				 (mod->getDescription() &&
				  *mod->getDescription())
				     ? mod->getDescription()
				     : modulo,
				 version);

	res.ref_valida = res_ref;
	res.texto_de = res_texto;
	res.datos = sermon->version;

	{
		SWKey *clave = mod->createKey();
		TreeKeyIdx *arbol = dynamic_cast<TreeKeyIdx *>(clave);

		if (arbol) {
			TreeKeyIdx raiz = *arbol;

			raiz.root();
			recorrer(sermon, mod, raiz, 1, &res);
		}
		delete clave;
	}

	g_free(version);
	return sermon;
}

void
main_pulpito_cerrar(PU_SERMON *sermon)
{
	pu_sermon_libre(sermon);
}

/* --------------------------------------------------------------------
 * Estado de la entrega
 * ------------------------------------------------------------------ */

int
main_pulpito_ultimo_paso(const char *modulo)
{
	char *val;
	int n = 0;

	if (!modulo)
		return 0;
	val = xml_get_list_from_label(PU_SECCION, "paso", (char *)modulo);
	if (val && *val)
		n = atoi(val);
	g_free(val);
	return (n < 0) ? 0 : n;
}

void
main_pulpito_guardar_paso(const char *modulo, int paso)
{
	gchar *val;
	gchar *cuando;
	GDateTime *ahora;

	if (!modulo)
		return;
	val = g_strdup_printf("%d", (paso < 0) ? 0 : paso);
	xml_set_list_item(PU_SECCION, "paso", (char *)modulo, val);
	g_free(val);

	ahora = g_date_time_new_now_local();
	cuando = g_date_time_format(ahora, "%Y-%m-%d %H:%M");
	g_date_time_unref(ahora);
	xml_set_list_item(PU_SECCION, "abierto", (char *)modulo, cuando);
	g_free(cuando);
}

gboolean
main_pulpito_predicado(const char *modulo)
{
	char *val;
	gboolean si;

	if (!modulo)
		return FALSE;
	val = xml_get_list_from_label(PU_SECCION, "predicado", (char *)modulo);
	si = (val && *val == '1');
	g_free(val);
	return si;
}

void
main_pulpito_marcar_predicado(const char *modulo, gboolean predicado)
{
	if (!modulo)
		return;
	xml_set_list_item(PU_SECCION, "predicado", (char *)modulo,
			  predicado ? (char *)"1" : (char *)"0");
}

PU_SEGUNDA
main_pulpito_segunda(void)
{
	char *val = xml_get_list_from_label(PU_SECCION, "estado", "segunda");
	int que = PU2_VERSO;

	if (val && *val)
		que = atoi(val);
	g_free(val);
	return (PU_SEGUNDA)CLAMP(que, PU2_NADA, PU2_AMBOS);
}

void
main_pulpito_segunda_poner(PU_SEGUNDA que)
{
	gchar *val = g_strdup_printf("%d", CLAMP((int)que, PU2_NADA,
						 PU2_AMBOS));

	xml_set_list_item(PU_SECCION, "estado", "segunda", val);
	g_free(val);
}

int
main_pulpito_objetivo(void)
{
	char *val = xml_get_list_from_label(PU_SECCION, "estado", "objetivo");
	int m = 0;

	if (val && *val)
		m = atoi(val);
	g_free(val);
	return CLAMP(m, 0, 240);
}

void
main_pulpito_objetivo_poner(int minutos)
{
	gchar *val = g_strdup_printf("%d", CLAMP(minutos, 0, 240));

	xml_set_list_item(PU_SECCION, "estado", "objetivo", val);
	g_free(val);
}

int
main_pulpito_zoom(void)
{
	char *val = xml_get_list_from_label(PU_SECCION, "estado", "zoom");
	int z = 100;

	if (val && *val)
		z = atoi(val);
	g_free(val);
	return CLAMP(z, 50, 300);
}

void
main_pulpito_zoom_poner(int porciento)
{
	gchar *val = g_strdup_printf("%d", CLAMP(porciento, 50, 300));

	xml_set_list_item(PU_SECCION, "estado", "zoom", val);
	g_free(val);
}
