/*
 * Biblia Elim
 * nube_palabras.cc - conteo de palabras por libro bíblico
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

#include <string.h>
#include <swmgr.h>
#include <swmodule.h>
#include <versekey.h>
#include <swbuf.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "backend/sword_main.hh"
#include "main/nube_palabras.h"
#include "main/sword.h"

#include "gui/debug_glib_null.h"

using namespace sword;

/* Palabras vacías ES + EN (incl. formas clásicas de Reina-Valera y KJV). */
static const char *const STOPWORDS[] = {
    /* español */
    "el", "la", "los", "las", "un", "una", "unos", "unas",
    "de", "del", "al", "y", "e", "o", "u", "que", "qué",
    "en", "a", "por", "para", "con", "sin", "sobre", "entre",
    "hacia", "hasta", "desde", "como", "más", "mas", "menos",
    "muy", "ya", "no", "ni", "sí", "si", "se", "su", "sus",
    "mi", "mis", "tu", "tus", "le", "les", "lo", "me", "te",
    "nos", "os", "es", "son", "ser", "sido", "fue", "fueron",
    "era", "eran", "está", "están", "estaba", "estaban",
    "ha", "han", "he", "hemos", "hay", "haber", "había", "habían",
    "este", "esta", "estos", "estas", "ese", "esa", "esos", "esas",
    "aquel", "aquella", "aquellos", "aquellas", "esto", "eso", "aquello",
    "yo", "tú", "el", "él", "ella", "nosotros", "nosotras",
    "vosotros", "vosotras", "ellos", "ellas",
    "quien", "quién", "quienes", "cual", "cuál", "cuales",
    "cuando", "cuándo", "donde", "dónde", "porque", "porqué",
    "pero", "sino", "aunque", "también", "así", "bien",
    "todo", "toda", "todos", "todas", "otro", "otra", "otros", "otras",
    "mismo", "misma", "mismos", "mismas", "cada",
    "algún", "alguna", "algunos", "algunas",
    "ningún", "ninguna", "ninguno",
    "pues", "entonces", "ahora", "después", "antes",
    "aquí", "allí", "ahí", "allá", "acá",
    "oh", "eh", "ay", "pues", "hecho", "hacer", "hace", "hacía",
    "tiene", "tienen", "tener", "tenía", "tenían",
    "dice", "dijo", "dicen", "decir", "dicho",
    "puede", "pueden", "poder", "podía",
    "va", "van", "voy", "vamos", "ir", "iba", "fueron",
    "muy", "tan", "tanto", "tanta", "tantos", "tantas",
    "poco", "poca", "pocos", "pocas",
    "mucho", "mucha", "muchos", "muchas",
    "cómo", "cual", "cuanto", "cuánto",
    "vos", "os", "le", "les",
    /* inglés */
    "the", "a", "an", "of", "and", "or", "to", "in", "for",
    "on", "with", "at", "by", "from", "as", "is", "was",
    "are", "were", "be", "been", "being", "that", "this",
    "these", "those", "it", "its", "he", "she", "they",
    "them", "his", "her", "their", "not", "no", "but",
    "if", "so", "than", "then", "there", "here",
    "have", "has", "had", "do", "does", "did",
    "will", "would", "shall", "should", "can", "could",
    "may", "might", "must", "into", "onto", "upon",
    "out", "up", "down", "over", "under", "again",
    "about", "against", "between", "through",
    "who", "whom", "which", "what", "when", "where", "why", "how",
    "all", "each", "every", "both", "few", "more", "most",
    "other", "some", "such", "only", "own", "same",
    "also", "just", "even", "still", "yet",
    /* formas clásicas */
    "unto", "thee", "thou", "thy", "thine", "ye", "hath",
    "doth", "saith", "shalt", "art", "wast", "wilt",
    "hast", "hadst", "dost", "didst", "thereof", "therein",
    "wherein", "whereby", "therefore", "nevertheless",
    "behold", "verily",
    NULL};

struct AliasLibro {
	const char *osis;
	const char *formas[12];
};

static const AliasLibro ALIAS_LIBROS[] = {
    {"Gen", {"genesis", "génesis", "gn", "gen", NULL}},
    {"Exod", {"exodo", "éxodo", "ex", "exo", NULL}},
    {"Lev", {"levitico", "levítico", "lv", "lev", NULL}},
    {"Num", {"numeros", "números", "nm", "num", "nu", NULL}},
    {"Deut", {"deuteronomio", "dt", "deu", "deut", NULL}},
    {"Josh", {"josue", "josué", "joshua", "jos", NULL}},
    {"Judg", {"jueces", "judges", "jue", "jdc", NULL}},
    {"Ruth", {"rut", "ruth", "rt", NULL}},
    {"1Sam", {"1 samuel", "1samuel", "1 sam", "1s", "1sa", NULL}},
    {"2Sam", {"2 samuel", "2samuel", "2 sam", "2s", "2sa", NULL}},
    {"1Kgs", {"1 reyes", "1reyes", "1 kings", "1r", "1re", "1ki", NULL}},
    {"2Kgs", {"2 reyes", "2reyes", "2 kings", "2r", "2re", "2ki", NULL}},
    {"1Chr", {"1 cronicas", "1 crónicas", "1 chronicles", "1cr", "1ch", NULL}},
    {"2Chr", {"2 cronicas", "2 crónicas", "2 chronicles", "2cr", "2ch", NULL}},
    {"Ezra", {"esdras", "ezra", "esd", "ezr", NULL}},
    {"Neh", {"nehemias", "nehemías", "nehemiah", "neh", NULL}},
    {"Esth", {"ester", "esther", "est", NULL}},
    {"Job", {"job", "jb", NULL}},
    {"Ps", {"salmos", "salmo", "psalms", "psalm", "sal", "ps", NULL}},
    {"Prov", {"proverbios", "proverbs", "prv", "prov", "pr", NULL}},
    {"Eccl", {"eclesiastes", "eclesiastés", "ecclesiastes", "ecl", "ec", NULL}},
    {"Song", {"cantares", "cantar", "song", "cnt", "can", "so", NULL}},
    {"Isa", {"isaias", "isaías", "isaiah", "is", "isa", NULL}},
    {"Jer", {"jeremias", "jeremías", "jeremiah", "jer", NULL}},
    {"Lam", {"lamentaciones", "lamentations", "lm", "lam", NULL}},
    {"Ezek", {"ezequiel", "ezekiel", "ez", "eze", NULL}},
    {"Dan", {"daniel", "dn", "dan", NULL}},
    {"Hos", {"oseas", "óseas", "hosea", "os", "hos", NULL}},
    {"Joel", {"joel", "jl", NULL}},
    {"Amos", {"amos", "amós", "am", NULL}},
    {"Obad", {"abdias", "abdías", "obadiah", "abd", "ob", NULL}},
    {"Jonah", {"jonas", "jonás", "jonah", "jon", NULL}},
    {"Mic", {"miqueas", "micah", "miq", "mi", NULL}},
    {"Nah", {"nahum", "nahúm", "nah", NULL}},
    {"Hab", {"habacuc", "habakkuk", "hab", NULL}},
    {"Zeph", {"sofonias", "sofonías", "zephaniah", "sof", "zep", NULL}},
    {"Hag", {"hageo", "haggai", "hag", NULL}},
    {"Zech", {"zacarias", "zacarías", "zechariah", "zac", "zec", NULL}},
    {"Mal", {"malaquias", "malaquías", "malachi", "mal", NULL}},
    {"Matt", {"mateo", "matthew", "mt", "mat", NULL}},
    {"Mark", {"marcos", "mark", "mc", "mr", "mk", NULL}},
    {"Luke", {"lucas", "luke", "lc", "lu", "lk", NULL}},
    {"John", {"juan", "john", "jn", "joh", NULL}},
    {"Acts", {"hechos", "acts", "hch", "hc", "ac", NULL}},
    {"Rom", {"romanos", "romans", "ro", "rm", "rom", NULL}},
    {"1Cor", {"1 corintios", "1corintios", "1 corinthians", "1co", "1cor", NULL}},
    {"2Cor", {"2 corintios", "2corintios", "2 corinthians", "2co", "2cor", NULL}},
    {"Gal", {"galatas", "gálatas", "galatians", "ga", "gal", NULL}},
    {"Eph", {"efesios", "ephesians", "ef", "eph", NULL}},
    {"Phil", {"filipenses", "philippians", "flp", "fil", "php", NULL}},
    {"Col", {"colosenses", "colossians", "col", NULL}},
    {"1Thess", {"1 tesalonicenses", "1tesalonicenses", "1 thessalonians", "1ts", "1th", NULL}},
    {"2Thess", {"2 tesalonicenses", "2tesalonicenses", "2 thessalonians", "2ts", "2th", NULL}},
    {"1Tim", {"1 timoteo", "1timoteo", "1 timothy", "1tm", "1ti", NULL}},
    {"2Tim", {"2 timoteo", "2timoteo", "2 timothy", "2tm", "2ti", NULL}},
    {"Titus", {"tito", "titus", "tit", "ti", NULL}},
    {"Phlm", {"filemon", "filemón", "philemon", "flm", "flm", "phm", NULL}},
    {"Heb", {"hebreos", "hebrews", "he", "heb", NULL}},
    {"Jas", {"santiago", "james", "stg", "st", "jas", NULL}},
    {"1Pet", {"1 pedro", "1pedro", "1 peter", "1p", "1pe", "1pt", NULL}},
    {"2Pet", {"2 pedro", "2pedro", "2 peter", "2p", "2pe", "2pt", NULL}},
    {"1John", {"1 juan", "1juan", "1 john", "1jn", "1jo", NULL}},
    {"2John", {"2 juan", "2juan", "2 john", "2jn", "2jo", NULL}},
    {"3John", {"3 juan", "3juan", "3 john", "3jn", "3jo", NULL}},
    {"Jude", {"judas", "jude", "jud", NULL}},
    {"Rev", {"apocalipsis", "revelation", "ap", "apo", "rev", NULL}},
    {"Tob", {"tobias", "tobías", "tobit", "tob", NULL}},
    {"Jdt", {"judit", "judith", "jdt", NULL}},
    {"Wis", {"sabiduria", "sabiduría", "wisdom", "sab", "wis", NULL}},
    {"Sir", {"eclesiastico", "eclesiástico", "sirach", "sir", "ecs", NULL}},
    {"Bar", {"baruc", "baruch", "bar", NULL}},
    {"1Macc", {"1 macabeos", "1macabeos", "1 maccabees", "1mac", "1ma", NULL}},
    {"2Macc", {"2 macabeos", "2macabeos", "2 maccabees", "2mac", "2ma", NULL}},
    {NULL, {NULL}}};

static GHashTable *stop_table = NULL;

static void
ensure_stopwords(void)
{
	if (stop_table)
		return;
	stop_table = g_hash_table_new(g_str_hash, g_str_equal);
	for (int i = 0; STOPWORDS[i]; i++)
		g_hash_table_add(stop_table, (gpointer)STOPWORDS[i]);
}

gchar *
main_nube_normalizar(const gchar *s)
{
	if (!s || !*s)
		return g_strdup("");

	gchar *down = g_utf8_strdown(s, -1);
	gchar *norm = g_utf8_normalize(down, -1, G_NORMALIZE_NFD);
	g_free(down);
	if (!norm)
		return g_strdup(s);

	GString *out = g_string_new(NULL);
	for (const gchar *p = norm; *p; p = g_utf8_next_char(p)) {
		gunichar c = g_utf8_get_char(p);
		if (g_unichar_combining_class(c) != 0)
			continue;
		if (g_unichar_isspace(c)) {
			if (out->len > 0 && out->str[out->len - 1] != ' ')
				g_string_append_c(out, ' ');
			continue;
		}
		g_string_append_unichar(out, c);
	}
	g_free(norm);
	gchar *res = g_string_free(out, FALSE);
	return g_strstrip(res);
}

gboolean
main_nube_texto_coincide(const gchar *haystack, const gchar *needle)
{
	if (!haystack || !needle || !*needle)
		return TRUE;
	gchar *h = main_nube_normalizar(haystack);
	gchar *n = main_nube_normalizar(needle);
	gboolean ok = (n[0] == '\0') || (strstr(h, n) != NULL);
	g_free(h);
	g_free(n);
	return ok;
}

void
main_nube_libro_free(NUBE_LIBRO *libro)
{
	if (!libro)
		return;
	g_free(libro->nombre);
	g_free(libro->abrev);
	g_free(libro->osis);
	g_free(libro);
}

void
main_nube_lista_libros_free(GList *lista)
{
	g_list_free_full(lista, (GDestroyNotify)main_nube_libro_free);
}

static char *
osis_from_ref(const char *osisref)
{
	if (!osisref || !*osisref)
		return g_strdup("");
	const char *dot = strchr(osisref, '.');
	if (dot)
		return g_strndup(osisref, dot - osisref);
	return g_strdup(osisref);
}

static GList *
append_testament_books(GList *lista, SWModule *mod, VerseKey *key, int testament)
{
	if (!backend->module_has_testament(mod->getName(), testament))
		return lista;

	int max = key->BMAX[testament - 1];
	for (int i = 0; i < max; i++) {
		key->setTestament(testament);
		key->setBook(i + 1);
		key->setChapter(1);
		key->setVerse(1);
		NUBE_LIBRO *libro = g_new0(NUBE_LIBRO, 1);
		libro->nombre = g_strdup(key->getBookName());
		libro->abrev = g_strdup(key->getBookAbbrev());
		libro->osis = osis_from_ref(key->getOSISRef());
		lista = g_list_append(lista, libro);
	}
	return lista;
}

GList *
main_nube_lista_libros(const char *module)
{
	GList *lista = NULL;
	if (!module || !*module)
		return NULL;

	SWModule *mod = backend->get_SWModule(module);
	if (!mod)
		return NULL;

	VerseKey *key = (VerseKey *)mod->createKey();
	key->setAutoNormalize(1);
	lista = append_testament_books(lista, mod, key, 1);
	lista = append_testament_books(lista, mod, key, 2);
	delete key;
	return lista;
}

char *
main_nube_libro_de_clave(const char *module, const char *key)
{
	if (!module || !key)
		return NULL;
	SWModule *mod = backend->get_SWModule(module);
	if (!mod)
		return NULL;
	VerseKey *vk = (VerseKey *)mod->createKey();
	vk->setAutoNormalize(1);
	vk->setText(key);
	char *s = g_strdup(vk->getBookName());
	delete vk;
	return s;
}

static gint
puntaje_coincidencia(const gchar *norm_input,
		     const NUBE_LIBRO *libro)
{
	gchar *n = main_nube_normalizar(libro->nombre);
	gchar *a = main_nube_normalizar(libro->abrev);
	gchar *o = main_nube_normalizar(libro->osis);
	gint score = 0;

	if (n[0] && strcmp(n, norm_input) == 0)
		score = 100;
	else if (a[0] && strcmp(a, norm_input) == 0)
		score = 90;
	else if (o[0] && strcmp(o, norm_input) == 0)
		score = 88;
	else if (n[0] && g_str_has_prefix(n, norm_input))
		score = 70 - (gint)strlen(n) + (gint)strlen(norm_input);
	else if (n[0] && strstr(n, norm_input))
		score = 50;

	g_free(n);
	g_free(a);
	g_free(o);
	return score;
}

static const char *
osis_de_alias(const gchar *norm_input)
{
	for (int i = 0; ALIAS_LIBROS[i].osis; i++) {
		for (int j = 0; ALIAS_LIBROS[i].formas[j]; j++) {
			gchar *f = main_nube_normalizar(ALIAS_LIBROS[i].formas[j]);
			gboolean eq = (strcmp(f, norm_input) == 0);
			g_free(f);
			if (eq)
				return ALIAS_LIBROS[i].osis;
		}
	}
	return NULL;
}

char *
main_nube_resolver_libro(const char *module, const char *texto)
{
	if (!texto || !*texto)
		return NULL;

	gchar *norm = main_nube_normalizar(texto);
	if (!norm[0]) {
		g_free(norm);
		return NULL;
	}

	GList *libros = main_nube_lista_libros(module);
	const NUBE_LIBRO *mejor = NULL;
	gint mejor_score = 0;

	for (GList *l = libros; l; l = l->next) {
		NUBE_LIBRO *libro = (NUBE_LIBRO *)l->data;
		gint s = puntaje_coincidencia(norm, libro);
		if (s > mejor_score) {
			mejor_score = s;
			mejor = libro;
		}
	}

	if (mejor_score < 88) {
		const char *osis = osis_de_alias(norm);
		if (osis) {
			gchar *osis_n = main_nube_normalizar(osis);
			for (GList *l = libros; l; l = l->next) {
				NUBE_LIBRO *libro = (NUBE_LIBRO *)l->data;
				gchar *o = main_nube_normalizar(libro->osis);
				gboolean eq = (strcmp(o, osis_n) == 0);
				g_free(o);
				if (eq) {
					mejor = libro;
					mejor_score = 95;
					break;
				}
			}
			g_free(osis_n);
		}
	}

	char *res = NULL;
	if (mejor && mejor_score >= 50)
		res = g_strdup(mejor->nombre);

	main_nube_lista_libros_free(libros);
	g_free(norm);
	return res;
}

static gboolean
es_palabra_util(const gchar *word)
{
	if (!word || !*word)
		return FALSE;
	glong len = g_utf8_strlen(word, -1);
	if (len < 2)
		return FALSE;

	gboolean tiene_letra = FALSE;
	for (const gchar *p = word; *p; p = g_utf8_next_char(p)) {
		if (g_unichar_isalpha(g_utf8_get_char(p))) {
			tiene_letra = TRUE;
			break;
		}
	}
	if (!tiene_letra)
		return FALSE;

	ensure_stopwords();
	return !g_hash_table_contains(stop_table, word);
}

static void
agregar_palabra(GHashTable *ht, const gchar *word)
{
	gpointer orig, val;
	if (g_hash_table_lookup_extended(ht, word, &orig, &val)) {
		g_hash_table_insert(ht, orig,
				    GINT_TO_POINTER(GPOINTER_TO_INT(val) + 1));
	} else {
		g_hash_table_insert(ht, g_strdup(word), GINT_TO_POINTER(1));
	}
}

static void
tokenizar(const char *texto, GHashTable *ht, gint *total)
{
	if (!texto)
		return;

	GString *cur = g_string_new(NULL);
	for (const gchar *p = texto; *p; p = g_utf8_next_char(p)) {
		gunichar c = g_utf8_get_char(p);
		if (g_unichar_isalpha(c) || c == '\'' || c == 0x2019) {
			g_string_append_unichar(cur, c);
			continue;
		}
		if (cur->len > 0) {
			gchar *down = g_utf8_strdown(cur->str, -1);
			g_strdelimit(down, "'", '\0'); /* corta posesivos ingleses */
			if (es_palabra_util(down)) {
				agregar_palabra(ht, down);
				(*total)++;
			}
			g_free(down);
			g_string_assign(cur, "");
		}
	}
	if (cur->len > 0) {
		gchar *down = g_utf8_strdown(cur->str, -1);
		g_strdelimit(down, "'", '\0');
		if (es_palabra_util(down)) {
			agregar_palabra(ht, down);
			(*total)++;
		}
		g_free(down);
	}
	g_string_free(cur, TRUE);
}

static GHashTable *
contar_libro(SWModule *mod, const char *libro, gint *total_out)
{
	GHashTable *ht = g_hash_table_new_full(g_str_hash, g_str_equal,
					       g_free, NULL);
	*total_out = 0;

	SWBuf original = mod->getKey()->getText();

	VerseKey *vk = (VerseKey *)mod->createKey();
	vk->setAutoNormalize(1);
	SWBuf start;
	start.append(libro);
	start.append(" 1:1");
	vk->setText(start.c_str());

	int book = vk->getBook();
	int testament = vk->getTestament();
	mod->setKey(*vk);

	while (!mod->popError()) {
		VerseKey *cur = (VerseKey *)(SWKey *)(*mod);
		if (cur->getBook() != book || cur->getTestament() != testament)
			break;
		tokenizar(mod->stripText(), ht, total_out);
		(*mod)++;
	}

	delete vk;
	mod->setKeyText(original.c_str());
	return ht;
}

typedef struct {
	gchar *palabra;
	gint cuenta;
} ParCuenta;

static gint
cmp_par_desc(gconstpointer a, gconstpointer b)
{
	const ParCuenta *pa = (const ParCuenta *)a;
	const ParCuenta *pb = (const ParCuenta *)b;
	if (pb->cuenta != pa->cuenta)
		return pb->cuenta - pa->cuenta;
	return g_utf8_collate(pa->palabra, pb->palabra);
}

static GPtrArray *
top_palabras(GHashTable *ht, int limite)
{
	GArray *arr = g_array_new(FALSE, FALSE, sizeof(ParCuenta));
	GHashTableIter iter;
	gpointer key, val;
	g_hash_table_iter_init(&iter, ht);
	while (g_hash_table_iter_next(&iter, &key, &val)) {
		ParCuenta p;
		p.palabra = (gchar *)key;
		p.cuenta = GPOINTER_TO_INT(val);
		g_array_append_val(arr, p);
	}
	g_array_sort(arr, cmp_par_desc);

	GPtrArray *out = g_ptr_array_new();
	int n = MIN(limite, (int)arr->len);
	for (int i = 0; i < n; i++) {
		ParCuenta *p = &g_array_index(arr, ParCuenta, i);
		g_ptr_array_add(out, p->palabra);
	}
	g_array_free(arr, TRUE);
	return out;
}

static gint
cmp_nube_desc(gconstpointer a, gconstpointer b)
{
	const NUBE_PALABRA *pa = *(const NUBE_PALABRA *const *)a;
	const NUBE_PALABRA *pb = *(const NUBE_PALABRA *const *)b;
	gint ma = MAX(pa->cuenta, pa->cuenta_b);
	gint mb = MAX(pb->cuenta, pb->cuenta_b);
	if (mb != ma)
		return mb - ma;
	if (pb->cuenta != pa->cuenta)
		return pb->cuenta - pa->cuenta;
	return g_utf8_collate(pa->palabra, pb->palabra);
}

static void
nube_palabra_free(gpointer p)
{
	NUBE_PALABRA *w = (NUBE_PALABRA *)p;
	g_free(w->palabra);
	g_free(w);
}

void
main_nube_conteo_free(NUBE_CONTEO *conteo)
{
	if (!conteo)
		return;
	g_free(conteo->libro);
	g_free(conteo->libro_b);
	if (conteo->palabras)
		g_ptr_array_free(conteo->palabras, TRUE);
	g_free(conteo);
}

NUBE_CONTEO *
main_nube_contar(const char *module,
		 const char *libro_a,
		 const char *libro_b,
		 int limite)
{
	if (!module || !libro_a || !*libro_a)
		return NULL;
	if (limite < 10)
		limite = 80;

	SWModule *mod = backend->get_SWModule(module);
	if (!mod)
		return NULL;

	gint total_a = 0, total_b = 0;
	GHashTable *ht_a = contar_libro(mod, libro_a, &total_a);
	GHashTable *ht_b = NULL;
	if (libro_b && *libro_b)
		ht_b = contar_libro(mod, libro_b, &total_b);

	GHashTable *elegidas = g_hash_table_new(g_str_hash, g_str_equal);
	GPtrArray *top_a = top_palabras(ht_a, limite);
	for (guint i = 0; i < top_a->len; i++)
		g_hash_table_add(elegidas, g_ptr_array_index(top_a, i));
	g_ptr_array_free(top_a, TRUE);

	if (ht_b) {
		GPtrArray *top_b = top_palabras(ht_b, limite);
		for (guint i = 0; i < top_b->len; i++)
			g_hash_table_add(elegidas, g_ptr_array_index(top_b, i));
		g_ptr_array_free(top_b, TRUE);
	}

	NUBE_CONTEO *c = g_new0(NUBE_CONTEO, 1);
	c->libro = g_strdup(libro_a);
	c->libro_b = (ht_b) ? g_strdup(libro_b) : NULL;
	c->total = total_a;
	c->total_b = total_b;
	c->unicas = (gint)g_hash_table_size(ht_a);
	c->palabras = g_ptr_array_new_with_free_func(nube_palabra_free);

	GHashTableIter iter;
	gpointer key, unused;
	g_hash_table_iter_init(&iter, elegidas);
	while (g_hash_table_iter_next(&iter, &key, &unused)) {
		const gchar *palabra = (const gchar *)key;
		gint ca = GPOINTER_TO_INT(g_hash_table_lookup(ht_a, palabra));
		gint cb = ht_b ? GPOINTER_TO_INT(g_hash_table_lookup(ht_b, palabra)) : 0;
		NUBE_PALABRA *w = g_new0(NUBE_PALABRA, 1);
		w->palabra = g_strdup(palabra);
		w->cuenta = ca;
		w->cuenta_b = cb;
		w->diferencia = ca - cb;
		w->pct = (total_a > 0) ? (100.0 * ca / total_a) : 0.0;
		w->pct_b = (total_b > 0) ? (100.0 * cb / total_b) : 0.0;
		w->dif_pct = w->pct - w->pct_b;
		g_ptr_array_add(c->palabras, w);
	}

	g_ptr_array_sort(c->palabras, cmp_nube_desc);

	g_hash_table_destroy(elegidas);
	g_hash_table_destroy(ht_a);
	if (ht_b)
		g_hash_table_destroy(ht_b);
	return c;
}
