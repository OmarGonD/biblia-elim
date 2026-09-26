/*
 * Biblia Elim
 * nube_mayusculas.c - palabras del texto y cómo las escribe (ver .h)
 */
#include "main/nube_mayusculas.h"

#include <string.h>

typedef struct {
	gint mayuscula; /* dentro de la frase, con mayúscula */
	gint minuscula; /* en cualquier sitio, en minúscula */
} Caso;

struct _NubeMayusculas {
	GHashTable *casos; /* minúsculas -> Caso* */
};

static gboolean
es_letra_de_palabra(gunichar c)
{
	return g_unichar_isalpha(c) || c == '\'' || c == 0x2019;
}

static gboolean
cierra_frase(gunichar c)
{
	return c == '.' || c == '!' || c == '?' || c == ':' || c == 0xBF /* ¿ */ ||
	       c == 0xA1 /* ¡ */;
}

static gboolean
todo_mayusculas(const gchar *palabra)
{
	for (const gchar *p = palabra; *p; p = g_utf8_next_char(p))
		if (g_unichar_islower(g_utf8_get_char(p)))
			return FALSE;
	return TRUE;
}

void
nube_recorrer_palabras(const gchar *texto, NubePalabraVista cb, gpointer datos)
{
	GString *cur;
	gboolean abre = TRUE, abre_esta = TRUE;
	guint indice = 0;

	if (!texto || !g_utf8_validate(texto, -1, NULL))
		return;
	cur = g_string_new(NULL);
	for (const gchar *p = texto;; p = g_utf8_next_char(p)) {
		gunichar c = *p ? g_utf8_get_char(p) : 0;
		if (c && es_letra_de_palabra(c)) {
			if (cur->len == 0)
				abre_esta = abre;
			g_string_append_unichar(cur, c);
			continue;
		}
		if (cur->len > 0) {
			/* Los capítulos abren en versalitas: «Y ACONTECIÓ».
			 * Esas mayúsculas son de la página, no de la palabra. */
			if (indice < 2 && todo_mayusculas(cur->str))
				abre_esta = TRUE;
			cb(cur->str, abre_esta, datos);
			g_string_truncate(cur, 0);
			abre = FALSE;
			indice++;
		}
		if (!c)
			break;
		if (cierra_frase(c))
			abre = TRUE;
	}
	g_string_free(cur, TRUE);
}

NubeMayusculas *
nube_mayusculas_nueva(void)
{
	NubeMayusculas *m = g_new0(NubeMayusculas, 1);
	m->casos = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	return m;
}

void
nube_mayusculas_libre(NubeMayusculas *m)
{
	if (!m)
		return;
	g_hash_table_destroy(m->casos);
	g_free(m);
}

void
nube_mayusculas_anotar(NubeMayusculas *m, const gchar *palabra,
		       gboolean abre_frase)
{
	gunichar primera;
	gchar *clave;
	Caso *caso;

	if (!m || !palabra || !*palabra)
		return;
	primera = g_utf8_get_char(palabra);
	if (g_unichar_isupper(primera) || g_unichar_istitle(primera)) {
		/* «Noemí» o «JEHOVÁ»: dice algo solo dentro de la frase. */
		if (abre_frase)
			return;
	} else if (!g_unichar_islower(primera)) {
		return;
	}
	clave = g_utf8_strdown(palabra, -1);
	caso = g_hash_table_lookup(m->casos, clave);
	if (!caso) {
		caso = g_new0(Caso, 1);
		g_hash_table_insert(m->casos, clave, caso);
	} else {
		g_free(clave);
	}
	if (g_unichar_islower(primera))
		caso->minuscula++;
	else
		caso->mayuscula++;
}

gchar *
nube_mayusculas_forma(NubeMayusculas *m, const gchar *minusculas)
{
	Caso *caso;
	gchar primera[8] = {0};

	if (!minusculas)
		return NULL;
	caso = m ? g_hash_table_lookup(m->casos, minusculas) : NULL;
	/* Un nombre propio puede salir alguna vez en minúscula (una errata,
	 * un uso común: «el señor» frente a «el Señor»); lo que decide es
	 * qué hace el texto la mayoría de las veces. */
	if (!caso || caso->mayuscula <= caso->minuscula)
		return g_strdup(minusculas);
	g_unichar_to_utf8(g_unichar_totitle(g_utf8_get_char(minusculas)), primera);
	return g_strconcat(primera, g_utf8_next_char(minusculas), NULL);
}
