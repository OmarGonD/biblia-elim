/*
 * Biblia Elim — Biblia interlineal
 *
 * NT: Tisch (griego + Strong's + lema).
 * AT: WLC (hebreo) alineado con Strong's de KJV.
 * Léxico: ui/strongs-elim.xml (Strong 1890, openscriptures).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <swmodule.h>
#include <versekey.h>
#include <swbuf.h>
#include <swmodule.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "backend/sword_main.hh"
#include "gui/interlineal.h"
#include "gui/lectura_sync.h"
#include "gui/utilities.h"
#include "gui/widgets.h"
#include "gui/dialog.h"
#include "main/interlineal.h"
#include "main/morfologia.h"
#include "main/diccionario.h"
#include "main/lists.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/sidebar.h"
#include "main/xml.h"

using namespace sword;

static GHashTable *strongs = NULL;
static gboolean loaded = FALSE;
static GHashTable *occ_index = NULL;	/* "<mod>/<testament>" -> (strong -> GArray of verse indices) */
static gboolean il_reverse = FALSE;

static void
strong_free(gpointer p)
{
	InterlStrong *s = (InterlStrong *)p;
	if (!s)
		return;
	g_free(s->num);
	g_free(s->lema);
	g_free(s->translit);
	g_free(s->glosa);
	g_free(s->raiz);
	g_free(s->definicion);
	g_free(s);
}

static gchar *
xml_attr(xmlNodePtr n, const char *name)
{
	xmlChar *v = xmlGetProp(n, (const xmlChar *)name);
	if (!v)
		return g_strdup("");
	gchar *out = g_strdup((const char *)v);
	xmlFree(v);
	return out;
}

gchar *
main_interlineal_norm_strong(const char *s)
{
	const char *p, *end;
	gchar letter;

	if (!s)
		return NULL;
	/* G/H must be followed by a digit. Otherwise the 'g' in "strong"
	 * itself is taken as Strong's G0. */
	for (p = s; *p; p++) {
		if ((*p == 'G' || *p == 'g' || *p == 'H' || *p == 'h') &&
		    g_ascii_isdigit((guchar)p[1]))
			break;
	}
	if (!*p)
		return NULL;
	letter = g_ascii_toupper(*p);
	p++;
	while (*p == '0')
		p++;
	end = p;
	while (*end && g_ascii_isdigit(*end))
		end++;
	if (end == p)
		return NULL;
	return g_strdup_printf("%c%.*s", letter, (int)(end - p), p);
}

static void
load_strongs(void)
{
	if (loaded)
		return;
	loaded = TRUE;
	strongs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
					strong_free);

	GBytes *bytes = g_resources_lookup_data("/org/xiphos/ui/strongs-elim.xml",
						G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
	if (!bytes)
		return;
	gsize n = 0;
	const char *data = (const char *)g_bytes_get_data(bytes, &n);
	xmlDocPtr doc = xmlReadMemory(data, (int)n, "strongs-elim.xml", "UTF-8",
				      XML_PARSE_NOBLANKS);
	g_bytes_unref(bytes);
	if (!doc)
		return;
	xmlNodePtr root = xmlDocGetRootElement(doc);
	for (xmlNodePtr node = root ? root->children : NULL; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE || xmlStrcmp(node->name, (const xmlChar *)"s"))
			continue;
		InterlStrong *e = g_new0(InterlStrong, 1);
		e->num = xml_attr(node, "n");
		e->lema = xml_attr(node, "l");
		e->translit = xml_attr(node, "t");
		e->glosa = xml_attr(node, "g");
		e->raiz = xml_attr(node, "r");
		e->definicion = xml_attr(node, "d");
		gchar *key = main_interlineal_norm_strong(e->num);
		if (key)
			g_hash_table_replace(strongs, key, e);
		else
			strong_free(e);
	}
	xmlFreeDoc(doc);
}

void
main_interlineal_init(void)
{
	char *buf;

	load_strongs();
	buf = xml_get_value("misc", "interlineal_reverse");
	if (buf)
		il_reverse = atoi(buf) != 0;
}

void
main_interlineal_shutdown(void)
{
	if (occ_index)
		g_hash_table_destroy(occ_index);
	occ_index = NULL;
	if (strongs)
		g_hash_table_destroy(strongs);
	strongs = NULL;
	loaded = FALSE;
}

const InterlStrong *
main_interlineal_strong(const char *num)
{
	gchar *key;
	InterlStrong *e;

	load_strongs();
	key = main_interlineal_norm_strong(num);
	if (!key)
		return NULL;
	e = (InterlStrong *)g_hash_table_lookup(strongs, key);
	g_free(key);
	return e;
}

static void
tok_free(gpointer p)
{
	InterlTok *t = (InterlTok *)p;
	if (!t)
		return;
	g_free(t->forma);
	g_free(t->strong);
	g_free(t->strongs);
	g_free(t->raiz);
	g_free(t->glosa);
	g_free(t->translit);
	g_free(t->morph);
	g_free(t);
}

void
main_interlineal_tokens_free(GList *lista)
{
	g_list_free_full(lista, tok_free);
}

static gchar *
attr_val(const char *attrs, const char *name)
{
	gchar *pat = g_strdup_printf("%s=\"", name);
	const char *s = strstr(attrs, pat);
	g_free(pat);
	if (!s) {
		pat = g_strdup_printf("%s='", name);
		s = strstr(attrs, pat);
		g_free(pat);
	}
	if (!s)
		return NULL;
	s = strchr(s, '=');
	if (!s || !s[1])
		return NULL;
	s += 2;
	const char *e = strchr(s, * (s - 1) == '"' ? '"' : '\'');
	if (!e)
		e = strchr(s, ' ');
	if (!e)
		return g_strdup(s);
	return g_strndup(s, e - s);
}

static GList *
all_strongs_in(const char *blob)
{
	GList *out = NULL;
	const char *p;

	if (!blob)
		return NULL;
	p = blob;
	while (*p) {
		const char *s = strstr(p, "strong:");
		const char *S = strstr(p, "Strong:");
		gchar *n;

		if (!s || (S && S < s))
			s = S;
		if (!s)
			break;
		n = main_interlineal_norm_strong(s + 7);
		if (n && *n)
			out = g_list_append(out, n);
		else
			g_free(n);
		p = s + 7;
	}
	return out;
}

static gchar *
join_strongs(GList *nums)
{
	GString *s;
	GList *l;

	if (!nums)
		return NULL;
	s = g_string_new(NULL);
	for (l = nums; l; l = l->next) {
		if (s->len)
			g_string_append_c(s, ' ');
		g_string_append(s, (const char *)l->data);
	}
	return g_string_free(s, FALSE);
}

static gchar *
first_strong_in(const char *blob)
{
	GList *all = all_strongs_in(blob);
	gchar *first;

	if (!all)
		return main_interlineal_norm_strong(blob);
	first = g_strdup((const char *)all->data);
	g_list_free_full(all, g_free);
	return first;
}

static gchar *
lemma_in(const char *blob)
{
	const char *keys[] = { "lemma.Strong:", "lemma.TR:", "lemma.ANLEX:", NULL };
	int i;
	if (!blob)
		return NULL;
	for (i = 0; keys[i]; i++) {
		const char *s = strstr(blob, keys[i]);
		if (!s)
			continue;
		s += strlen(keys[i]);
		const char *e = s;
		while (*e && *e != ' ' && *e != '"' && *e != '\'')
			e++;
		if (e > s)
			return g_strndup(s, e - s);
	}
	return NULL;
}

static gchar *
morph_in(const char *blob)
{
	const char *s;
	if (!blob)
		return NULL;
	s = strstr(blob, "morph=\"");
	if (!s)
		s = strstr(blob, "robinson:");
	if (!s)
		return NULL;
	if (!strncmp(s, "morph=\"", 7))
		s += 7;
	else
		s += 9;
	const char *e = s;
	while (*e && *e != ' ' && *e != '"' && *e != '\'')
		e++;
	return g_strndup(s, e - s);
}

static gchar *
strip_punct(const char *in)
{
	gsize len;
	gchar *out;
	const gchar *p, *end;
	gchar *w;

	if (!in)
		return g_strdup("");
	len = strlen(in);
	out = (gchar *)g_malloc(len + 1);
	w = out;
	for (p = in, end = in + len; p < end; ) {
		gunichar c = g_utf8_get_char(p);
		if (g_unichar_isalnum(c) || c == 0x05BE /* maqaf */) {
			gint n = g_unichar_to_utf8(c, w);
			w += n;
		} else if (c == ' ' || c == 0x05C3) {
			/* skip sof pasuq / spaces in word */
		}
		p = g_utf8_next_char(p);
	}
	*w = '\0';
	return out;
}

static InterlTok *
tok_from_w(const char *attrs, const char *text)
{
	InterlTok *t = g_new0(InterlTok, 1);
	gchar *savlm = attr_val(attrs, "savlm");
	gchar *lemma = attr_val(attrs, "lemma");
	gchar *blob = g_strdup_printf("%s %s", savlm ? savlm : "", lemma ? lemma : "");
	const InterlStrong *info;

	{
		gchar *rawf = g_strdup(text ? text : "");
		g_strstrip(rawf);
		if (strchr(rawf, ' ')) {
			t->forma = rawf;
		} else {
			t->forma = strip_punct(rawf);
			g_free(rawf);
			if (!t->forma || !*t->forma) {
				g_free(t->forma);
				t->forma = g_strdup(text ? text : "");
			}
		}
	}
	{
		GList *all = all_strongs_in(blob);
		t->strong = all ? g_strdup((const char *)all->data)
				: first_strong_in(blob);
		t->strongs = join_strongs(all);
		g_list_free_full(all, g_free);
	}
	t->raiz = lemma_in(blob);
	t->morph = morph_in(attrs);
	info = t->strong ? main_interlineal_strong(t->strong) : NULL;
	if (info) {
		if (!t->raiz || !*t->raiz)
			t->raiz = g_strdup(info->lema);
		if (info->translit && *info->translit)
			t->translit = g_strdup(info->translit);
		if (info->glosa && *info->glosa)
			t->glosa = g_strdup(info->glosa);
		else if (info->lema && *info->lema)
			t->glosa = g_strdup(info->lema);
	}
	if (!t->glosa)
		t->glosa = g_strdup("");
	g_free(savlm);
	g_free(lemma);
	g_free(blob);
	return t;
}

static GList *
parse_w_tags(const char *raw)
{
	GList *out = NULL;
	const char *p;

	if (!raw)
		return NULL;
	p = raw;
	while ((p = strstr(p, "<w"))) {
		const char *gt = strchr(p, '>');
		const char *end;
		gchar *attrs, *text;
		if (!gt)
			break;
		end = strstr(gt, "</w>");
		if (!end)
			break;
		attrs = g_strndup(p, gt - p);
		text = g_strndup(gt + 1, end - (gt + 1));
		g_strstrip(text);
		if (*text)
			out = g_list_append(out, tok_from_w(attrs, text));
		g_free(attrs);
		g_free(text);
		p = end + 4;
	}
	return out;
}

static gboolean
mod_ok(const char *name)
{
	return name && backend && backend->is_module(name);
}

static GList *
tokens_nt(const char *key)
{
	char *raw = NULL;
	GList *toks;

	if (mod_ok("Tisch"))
		raw = backend->get_raw_text("Tisch", key);
	if ((!raw || !strstr(raw, "strong:")) && mod_ok("KJV")) {
		g_free(raw);
		raw = backend->get_raw_text("KJV", key);
	}
	toks = parse_w_tags(raw);
	g_free(raw);
	return toks;
}

static GList *
tokens_ot(const char *key)
{
	GList *kjv = NULL;
	GList *out = NULL;
	char *raw = NULL;
	char *heb = NULL;

	/* Con el Antiguo Testamento de Open Scriptures (OSHB) no hay nada
	 * que adivinar: cada palabra hebrea trae ya su número de Strong y
	 * su morfología, pegados a ella. Lo de abajo -- alinear el KJV
	 * inglés con el WLC contando palabras -- es de cuando no estaba
	 * instalado, y sigue ahí para quien no lo tenga. */
	if (mod_ok("OSHB")) {
		char *oshb = backend->get_raw_text("OSHB", key);
		GList *toks = parse_w_tags(oshb);

		g_free(oshb);
		if (toks) {
			for (GList *l = toks; l; l = l->next) {
				InterlTok *t = (InterlTok *)l->data;
				const InterlStrong *info =
				    t->strong ? main_interlineal_strong(t->strong)
					      : NULL;

				if (!info)
					continue;
				if ((!t->raiz || !*t->raiz) && info->lema)
					t->raiz = g_strdup(info->lema);
				if ((!t->translit || !*t->translit) &&
				    info->translit)
					t->translit = g_strdup(info->translit);
				if ((!t->glosa || !*t->glosa) && info->glosa)
					t->glosa = g_strdup(info->glosa);
			}
			return toks;
		}
	}

	if (mod_ok("KJV"))
		raw = backend->get_raw_text("KJV", key);
	kjv = parse_w_tags(raw);
	g_free(raw);

	if (mod_ok("WLC"))
		heb = backend->get_strip_text("WLC", key);

	if (heb && *heb && kjv) {
		gchar **parts = g_strsplit_set(heb, " \t\n\r", -1);
		gint nh = 0, nk = (gint)g_list_length(kjv), i;
		for (i = 0; parts[i]; i++) {
			g_strstrip(parts[i]);
			if (parts[i][0])
				nh++;
		}
		if (nh > 0 && abs(nh - nk) <= 3) {
			GList *k = kjv;
			for (i = 0; parts[i] && k; i++) {
				gchar *form;
				InterlTok *src, *t;
				if (!parts[i][0])
					continue;
				form = strip_punct(parts[i]);
				if (!form || !*form) {
					g_free(form);
					continue;
				}
				src = (InterlTok *)k->data;
				t = g_new0(InterlTok, 1);
				t->forma = form;
				t->strong = g_strdup(src->strong);
				t->strongs = g_strdup(src->strongs);
				t->raiz = g_strdup(src->raiz);
				t->glosa = g_strdup(src->glosa);
				t->translit = g_strdup(src->translit);
				t->morph = g_strdup(src->morph);
				if (t->strong) {
					const InterlStrong *info = main_interlineal_strong(t->strong);
					if (info) {
						if ((!t->raiz || !*t->raiz) && info->lema)
							t->raiz = g_strdup(info->lema);
						if ((!t->translit || !*t->translit) &&
						    info->translit)
							t->translit = g_strdup(info->translit);
					}
				}
				out = g_list_append(out, t);
				k = k->next;
			}
			g_strfreev(parts);
			main_interlineal_tokens_free(kjv);
			g_free(heb);
			return out;
		}
		g_strfreev(parts);
	}
	g_free(heb);
	/* Fallback: Strong's of KJV with dictionary Hebrew lemma as form. */
	for (GList *l = kjv; l; l = l->next) {
		InterlTok *t = (InterlTok *)l->data;
		const InterlStrong *info = t->strong ? main_interlineal_strong(t->strong) : NULL;
		if (info && info->lema && *info->lema) {
			g_free(t->forma);
			t->forma = g_strdup(info->lema);
		}
	}
	return kjv;
}

GList *
main_interlineal_versiculo(const char *key)
{
	int test = 2;

	if (!backend || !key)
		return NULL;
	load_strongs();
	if (mod_ok("KJV"))
		test = backend->get_key_testament("KJV", key);
	else if (settings.MainWindowModule)
		test = backend->get_key_testament(settings.MainWindowModule, key);
	if (test == 1)
		return tokens_ot(key);
	return tokens_nt(key);
}

static void
occ_free_postings(gpointer p)
{
	g_array_free((GArray *)p, TRUE);
}

static void
occ_free_index(gpointer p)
{
	g_hash_table_destroy((GHashTable *)p);
}

static const char *
occ_pick_module(char letter)
{
	const char *cur = settings.MainWindowModule;

	if (cur && mod_ok(cur) &&
	    (!g_ascii_strcasecmp(cur, "KJV") ||
	     !g_ascii_strcasecmp(cur, "Tisch") ||
	     (letter == 'G' && strstr(cur, "Tisch")) ||
	     (letter == 'H' && strstr(cur, "WLC"))))
		return cur;
	if (letter == 'H')
		return mod_ok("KJV") ? "KJV" : NULL;
	if (mod_ok("Tisch"))
		return "Tisch";
	return mod_ok("KJV") ? "KJV" : NULL;
}

/* One pass over a testament, recording every Strong's number against
 * every verse it appears in.
 *
 * This replaces a pair of scanners that walked the testament looking
 * for one Strong's number at a time and stopped early once `max` hits
 * were in hand. That was cheap for a common word and pathological for
 * a rare one: anything occurring fewer than `max` times could not stop
 * early and paid for the whole testament, every time it was clicked.
 * Measured on this machine: 238 ms per lookup for a rare Hebrew number
 * (23 145 verses), 126 ms for a rare Greek one.
 *
 * Building the entire table costs about the same as a single one of
 * those misses (274 ms for the OT, 150 ms for the NT) and answers
 * every subsequent lookup, for any Strong's number, from memory. Verse
 * positions are stored as VerseKey indices rather than key strings --
 * 4 bytes each, so ~1.2 MB for the OT and ~680 KB for the NT. */
static GHashTable *
occ_build_index(SWModule *mod, int testament)
{
	GHashTable *ht = g_hash_table_new_full(g_str_hash, g_str_equal,
					       g_free, occ_free_postings);
	SWBuf oldkey = mod->getKey()->getText();
	VerseKey vk;

	vk.setAutoNormalize(1);
	vk.setText(testament == 1 ? "Genesis 1:1" : "Matthew 1:1");

	for (; !vk.popError(); vk++) {
		const char *raw, *p;
		guint32 idx;

		if (vk.getTestament() != testament)
			break;
		mod->setKey(vk);
		raw = mod->getRawEntry();
		if (!raw)
			continue;
		idx = (guint32)vk.getIndex();

		for (p = strstr(raw, "strong:"); p; p = strstr(p, "strong:")) {
			const char *d;
			char key[16], letter;
			int len;
			GArray *a;

			p += 7;
			letter = g_ascii_toupper(*p);
			if (letter != 'G' && letter != 'H')
				continue;
			p++;
			while (*p == '0')
				p++;
			d = p;
			while (g_ascii_isdigit(*p))
				p++;
			if (p == d)
				continue;

			/* same normalisation main_interlineal_norm_strong()
			 * produces, so lookups can use its output directly:
			 * letter + digits, leading zeros stripped. */
			len = MIN((int)(p - d), (int)sizeof(key) - 2);
			key[0] = letter;
			memcpy(key + 1, d, len);
			key[1 + len] = '\0';

			a = (GArray *)g_hash_table_lookup(ht, key);
			if (!a) {
				a = g_array_new(FALSE, FALSE, sizeof(guint32));
				g_hash_table_insert(ht, g_strdup(key), a);
			}
			/* verses are visited in order, so a repeat of the
			 * same number within one verse is always the tail. */
			if (a->len == 0 ||
			    g_array_index(a, guint32, a->len - 1) != idx)
				g_array_append_val(a, idx);
		}
	}

	mod->setKey(oldkey.c_str());
	return ht;
}

/* The built index for one (module, testament), building it on first
 * use. NULL only if the module is unavailable. */
static GHashTable *
occ_get_index(const char *modname, int testament)
{
	gchar *cachekey;
	GHashTable *ht;
	SWModule *mod;

	if (!modname)
		return NULL;
	if (!occ_index)
		occ_index = g_hash_table_new_full(g_str_hash, g_str_equal,
						  g_free, occ_free_index);

	cachekey = g_strdup_printf("%s/%d", modname, testament);
	ht = (GHashTable *)g_hash_table_lookup(occ_index, cachekey);
	if (ht) {
		g_free(cachekey);
		return ht;
	}

	mod = backend->get_SWModule(modname);
	if (!mod) {
		g_free(cachekey);
		return NULL;
	}
	ht = occ_build_index(mod, testament);
	g_hash_table_insert(occ_index, cachekey, ht);	/* takes cachekey */
	return ht;
}

/* Called when the interlinear is switched on. Building the index here
 * means the cost lands on a deliberate user action rather than on the
 * first Strong's number they happen to click. */
void
main_interlineal_empezar_indice(void)
{
	VerseKey here;
	int testament;

	if (!backend)
		return;
	here.setAutoNormalize(1);
	here.setText(settings.currentverse ? settings.currentverse : "Gen.1.1");
	testament = here.getTestament();
	occ_get_index(occ_pick_module(testament == 1 ? 'H' : 'G'), testament);
}

gboolean
main_interlineal_indice_listo(void)
{
	return TRUE;
}

GList *
main_interlineal_ocurrencias(const char *strong, int max)
{
	GList *out = NULL;
	gchar *norm;
	const char *modname;
	GHashTable *ht;
	GArray *postings;
	VerseKey here, vk;
	char letter;
	int testament, book, i, n = 0;

	if (!strong || max <= 0 || !backend)
		return NULL;
	norm = main_interlineal_norm_strong(strong);
	if (!norm || !norm[0]) {
		g_free(norm);
		return NULL;
	}

	letter = norm[0];
	testament = (letter == 'H') ? 1 : 2;
	modname = occ_pick_module(letter);
	ht = occ_get_index(modname, testament);
	if (!ht) {
		g_free(norm);
		return NULL;
	}
	postings = (GArray *)g_hash_table_lookup(ht, norm);
	g_free(norm);
	if (!postings || postings->len == 0)
		return NULL;

	/* Same ordering the old two-phase scan produced: hits in the book
	 * being read come first, the rest after, each in canonical order.
	 * Two passes over the (integer) postings rather than one, so a
	 * common word does not materialise thousands of key strings only
	 * to throw all but `max` of them away. */
	here.setAutoNormalize(1);
	here.setText(settings.currentverse ? settings.currentverse : "Gen.1.1");
	book = (here.getTestament() == testament) ? here.getBook() : -1;

	vk.setAutoNormalize(1);
	for (i = 0; i < (int)postings->len && n < max; i++) {
		vk.setTestament(testament);
		vk.setIndex((long)g_array_index(postings, guint32, i));
		if (book > 0 && vk.getBook() != book)
			continue;
		out = g_list_prepend(out, g_strdup(vk.getText()));
		n++;
	}
	if (book > 0) {
		for (i = 0; i < (int)postings->len && n < max; i++) {
			vk.setTestament(testament);
			vk.setIndex((long)g_array_index(postings, guint32, i));
			if (vk.getBook() == book)
				continue;
			out = g_list_prepend(out, g_strdup(vk.getText()));
			n++;
		}
	}

	return g_list_reverse(out);
}

static gchar *
css_color(const char *c, const char *fallback)
{
	if (!c || !*c)
		return g_strdup(fallback);
	if (c[0] == '#')
		return g_strdup(c);
	if (c[0] == '0' && (c[1] == 'x' || c[1] == 'X'))
		return g_strdup_printf("#%s", c + 2);
	return g_strdup(c);
}

static gchar *
html_to_plain(const char *in)
{
	GString *out;
	const char *p;
	gboolean intag = FALSE, space = TRUE;

	if (!in)
		return g_strdup("");
	out = g_string_new(NULL);
	for (p = in; *p;) {
		if (!intag && *p == '<') {
			if (!g_ascii_strncasecmp(p, "<br", 3) && !space) {
				g_string_append_c(out, ' ');
				space = TRUE;
			}
			intag = TRUE;
			p++;
			continue;
		}
		if (intag) {
			if (*p == '>')
				intag = FALSE;
			p++;
			continue;
		}
		if (*p == '&') {
			gunichar uc = 0;
			if (!g_ascii_strncasecmp(p, "&lt;", 4)) {
				g_string_append_c(out, '<');
				p += 4;
				space = FALSE;
				continue;
			}
			if (!g_ascii_strncasecmp(p, "&gt;", 4)) {
				g_string_append_c(out, '>');
				p += 4;
				space = FALSE;
				continue;
			}
			if (!g_ascii_strncasecmp(p, "&amp;", 5)) {
				g_string_append_c(out, '&');
				p += 5;
				space = FALSE;
				continue;
			}
			if (!g_ascii_strncasecmp(p, "&nbsp;", 6) ||
			    !g_ascii_strncasecmp(p, "&#160;", 6)) {
				if (!space) {
					g_string_append_c(out, ' ');
					space = TRUE;
				}
				p += 6;
				continue;
			}
			if (!g_ascii_strncasecmp(p, "&quot;", 6)) {
				g_string_append_c(out, '"');
				p += 6;
				space = FALSE;
				continue;
			}
			if (p[1] == '#') {
				gchar *end = NULL;
				if (p[2] == 'x' || p[2] == 'X')
					uc = (gunichar)g_ascii_strtoull(p + 3, &end, 16);
				else
					uc = (gunichar)g_ascii_strtoull(p + 2, &end, 10);
				if (end && *end == ';' && uc) {
					gchar buf[8];
					gint n = g_unichar_to_utf8(uc, buf);
					g_string_append_len(out, buf, n);
					p = end + 1;
					space = FALSE;
					continue;
				}
			}
		}
		if (g_unichar_isspace(g_utf8_get_char(p))) {
			if (!space) {
				g_string_append_c(out, ' ');
				space = TRUE;
			}
			p = g_utf8_next_char(p);
			continue;
		}
		{
			const char *n = g_utf8_next_char(p);
			g_string_append_len(out, p, n - p);
			p = n;
			space = FALSE;
		}
	}
	g_strstrip(out->str);
	return g_string_free(out, FALSE);
}

static gchar *
spanish_line(const char *key)
{
	const char *mod = settings.MainWindowModule;
	char *t, *plain;

	if (mod_ok(mod)) {
		t = backend->get_strip_text(mod, key);
		if (t && *t) {
			plain = html_to_plain(t);
			g_free(t);
			return plain;
		}
		g_free(t);
	}
	if (mod_ok("SpaRV")) {
		t = backend->get_strip_text("SpaRV", key);
		plain = html_to_plain(t);
		g_free(t);
		return plain;
	}
	if (mod_ok("SpaRVG")) {
		t = backend->get_strip_text("SpaRVG", key);
		plain = html_to_plain(t);
		g_free(t);
		return plain;
	}
	return g_strdup("");
}

static void
peel_word(const char *w, gchar **lead, gchar **core, gchar **trail)
{
	const gchar *p, *start, *end;

	if (!w || !*w) {
		*lead = g_strdup("");
		*core = g_strdup("");
		*trail = g_strdup("");
		return;
	}
	p = w;
	while (*p) {
		gunichar c = g_utf8_get_char(p);
		if (!g_unichar_ispunct(c) && !g_unichar_isspace(c))
			break;
		p = g_utf8_next_char(p);
	}
	start = p;
	end = p;
	while (*p) {
		gunichar c = g_utf8_get_char(p);
		const gchar *n = g_utf8_next_char(p);
		if (!g_unichar_ispunct(c))
			end = n;
		p = n;
	}
	*lead = g_strndup(w, start - w);
	*core = g_strndup(start, end - start);
	*trail = g_strdup(end);
}

static void
append_strong_sup(GString *out, InterlTok *tok)
{
	gchar *st, *tip;

	if (!tok || !tok->strong || !*tok->strong)
		return;
	st = g_markup_escape_text(tok->strong, -1);
	tip = g_markup_escape_text(tok->forma ? tok->forma : "", -1);
	g_string_append_printf(out,
			       "<sup class=\"st\"><a href=\"passagestudy.jsp?"
			       "action=showInterlineal&amp;value=%s\" title=\"%s\">%s</a></sup>",
			       st, tip, st);
	g_free(st);
	g_free(tip);
}

typedef struct {
	gchar *lead;
	gchar *core;
	gchar *trail;
	gboolean nota;
} EsPal;

static void
espal_free(gpointer p)
{
	EsPal *w = (EsPal *)p;
	if (!w)
		return;
	g_free(w->lead);
	g_free(w->core);
	g_free(w->trail);
	g_free(w);
}

static GList *
parse_es_palabras(const char *es)
{
	GList *out = NULL;
	const gchar *p;

	if (!es)
		return NULL;
	p = es;
	while (*p) {
		EsPal *w;
		gunichar c;

		while (*p && g_unichar_isspace(g_utf8_get_char(p)))
			p = g_utf8_next_char(p);
		if (!*p)
			break;
		c = g_utf8_get_char(p);
		if (c == '[') {
			const gchar *q = g_utf8_strchr(p, -1, (gunichar)']');
			w = g_new0(EsPal, 1);
			w->nota = TRUE;
			w->lead = g_strdup("");
			w->trail = g_strdup("");
			if (q) {
				w->core = g_strndup(p, (q - p) + 1);
				p = q + 1;
			} else {
				w->core = g_strdup(p);
				p += strlen(p);
			}
			out = g_list_append(out, w);
			continue;
		}
		{
			const gchar *start = p;
			gchar *tok;

			while (*p) {
				gunichar u = g_utf8_get_char(p);
				if (g_unichar_isspace(u) || u == '[')
					break;
				p = g_utf8_next_char(p);
			}
			tok = g_strndup(start, p - start);
			w = g_new0(EsPal, 1);
			peel_word(tok, &w->lead, &w->core, &w->trail);
			g_free(tok);
			out = g_list_append(out, w);
		}
	}
	return out;
}

static gboolean
es_stopword(const char *core)
{
	static const char *sw[] = {
		"a", "al", "como", "con", "de", "del", "e", "el", "en",
		"la", "las", "le", "lo", "los", "me", "mi", "ni", "no",
		"o", "os", "para", "pero", "por", "que", "se", "si",
		"su", "sus", "te", "un", "una", "y", "ya", NULL
	};
	gchar *fold;
	int i;

	if (!core || !*core)
		return FALSE;
	fold = g_utf8_casefold(core, -1);
	for (i = 0; sw[i]; i++) {
		if (!strcmp(fold, sw[i])) {
			g_free(fold);
			return TRUE;
		}
	}
	g_free(fold);
	return FALSE;
}

static GList *
toks_con_strong(GList *toks)
{
	GList *out = NULL, *l;

	for (l = toks; l; l = l->next) {
		InterlTok *t = (InterlTok *)l->data;
		if (t && t->strong && *t->strong)
			out = g_list_append(out, t);
	}
	return out;
}

static gboolean
alineacion_ok(int n_es, int n_st)
{
	int d, m;

	if (n_st <= 0)
		return TRUE;
	if (n_es <= 0)
		return FALSE;
	d = ABS(n_es - n_st);
	m = MAX(n_es, n_st);
	if (d <= 1)
		return TRUE;
	if (d <= 2 && (double)d / (double)m <= 0.18)
		return TRUE;
	return FALSE;
}

static gchar *
verse_con_strongs(const char *es, GList *toks)
{
	GString *out = g_string_new(NULL);
	GList *pal, *st, *l, *t;
	gint n_es = 0, n_st = 0, n_stop = 0;
	gboolean fiable, saltar_vacias = FALSE;

	if (!es)
		es = "";
	pal = parse_es_palabras(es);
	st = toks_con_strong(toks);
	n_st = (gint)g_list_length(st);
	for (l = pal; l; l = l->next) {
		EsPal *w = (EsPal *)l->data;
		if (!w->nota && w->core && *w->core) {
			n_es++;
			if (es_stopword(w->core))
				n_stop++;
		}
	}
	fiable = alineacion_ok(n_es, n_st);
	if (!fiable && n_es > n_st && alineacion_ok(n_es - n_stop, n_st)) {
		fiable = TRUE;
		saltar_vacias = TRUE;
	}

	t = st;
	for (l = pal; l; l = l->next) {
		EsPal *w = (EsPal *)l->data;
		gchar *e_lead = g_markup_escape_text(w->lead ? w->lead : "", -1);
		gchar *e_core = g_markup_escape_text(w->core ? w->core : "", -1);
		gchar *e_trail = g_markup_escape_text(w->trail ? w->trail : "", -1);
		gboolean poner_codigo = FALSE;

		if (w->nota)
			g_string_append(out, "<span class=\"nota\">");
		g_string_append(out, e_lead);
		g_string_append(out, e_core);
		if (fiable && !w->nota && w->core && *w->core && t) {
			if (saltar_vacias && es_stopword(w->core) && n_es > n_st)
				n_es--;
			else
				poner_codigo = TRUE;
		}
		if (poner_codigo) {
			append_strong_sup(out, (InterlTok *)t->data);
			t = t->next;
			n_st--;
			n_es--;
		}
		g_string_append(out, e_trail);
		if (w->nota)
			g_string_append(out, "</span>");
		g_string_append_c(out, ' ');
		g_free(e_lead);
		g_free(e_core);
		g_free(e_trail);
	}
	if (!fiable && n_st > 0)
		g_string_append(out,
				"<span class=\"aviso\">Alineación incierta: "
				"números Strong ocultos en este versículo.</span>");
	g_list_free(st);
	g_list_free_full(pal, espal_free);
	return g_string_free(out, FALSE);
}

gchar *
main_interlineal_cita_es(const char *key)
{
	static const struct {
		const char *osis;
		const char *es;
	} lib[] = {
		{"Gen", "Gn"}, {"Exod", "Ex"}, {"Lev", "Lv"}, {"Num", "Nm"},
		{"Deut", "Dt"}, {"Josh", "Jos"}, {"Judg", "Jue"}, {"Ruth", "Rt"},
		{"1Sam", "1 S"}, {"2Sam", "2 S"}, {"1Kgs", "1 R"}, {"2Kgs", "2 R"},
		{"1Chr", "1 Cr"}, {"2Chr", "2 Cr"}, {"Ezra", "Esd"}, {"Neh", "Neh"},
		{"Esth", "Est"}, {"Job", "Job"}, {"Ps", "Sal"}, {"Prov", "Pr"},
		{"Eccl", "Ec"}, {"Song", "Cnt"}, {"Isa", "Is"}, {"Jer", "Jer"},
		{"Lam", "Lm"}, {"Ezek", "Ez"}, {"Dan", "Dn"}, {"Hos", "Os"},
		{"Joel", "Jl"}, {"Amos", "Am"}, {"Obad", "Abd"}, {"Jonah", "Jon"},
		{"Mic", "Mi"}, {"Nah", "Nah"}, {"Hab", "Hab"}, {"Zeph", "Sof"},
		{"Hag", "Hag"}, {"Zech", "Zac"}, {"Mal", "Mal"},
		{"Matt", "Mt"}, {"Mark", "Mc"}, {"Luke", "Lc"}, {"John", "Jn"},
		{"Acts", "Hch"}, {"Rom", "Ro"}, {"1Cor", "1 Co"}, {"2Cor", "2 Co"},
		{"Gal", "Ga"}, {"Eph", "Ef"}, {"Phil", "Fil"}, {"Col", "Col"},
		{"1Thess", "1 Ts"}, {"2Thess", "2 Ts"}, {"1Tim", "1 Ti"},
		{"2Tim", "2 Ti"}, {"Titus", "Tit"}, {"Phlm", "Flm"},
		{"Heb", "He"}, {"Jas", "Stg"}, {"1Pet", "1 P"}, {"2Pet", "2 P"},
		{"1John", "1 Jn"}, {"2John", "2 Jn"}, {"3John", "3 Jn"},
		{"Jude", "Jud"}, {"Rev", "Ap"},
		{NULL, NULL}
	};
	VerseKey vk;
	SWBuf osis;
	gchar *book, *rest, *out;
	const char *es = NULL;
	int i, ch = 0, vs = 0;

	if (!key || !*key)
		return g_strdup("");
	vk.setAutoNormalize(1);
	vk.setText(key);
	osis = vk.getOSISRef();
	if (osis.length() < 3)
		return g_strdup(key);
	book = g_strdup(osis.c_str());
	rest = strchr(book, '.');
	if (rest) {
		*rest++ = '\0';
		sscanf(rest, "%d.%d", &ch, &vs);
	}
	for (i = 0; lib[i].osis; i++) {
		if (!g_ascii_strcasecmp(book, lib[i].osis)) {
			es = lib[i].es;
			break;
		}
	}
	if (es && ch > 0)
		out = g_strdup_printf("%s %d:%d", es, ch, vs);
	else
		out = g_strdup(key);
	g_free(book);
	return out;
}

static gchar *il_verse = NULL;

static gboolean
verse_keys_match(const char *a, const char *b)
{
	VerseKey ka, kb;

	if (!a || !b || !*a || !*b)
		return FALSE;
	if (!g_ascii_strcasecmp(a, b))
		return TRUE;
	ka.setAutoNormalize(1);
	kb.setAutoNormalize(1);
	ka.setText(a);
	kb.setText(b);
	return ka.getBook() == kb.getBook() &&
	       ka.getChapter() == kb.getChapter() &&
	       ka.getVerse() == kb.getVerse();
}

void
main_interlineal_abrir_verso(const char *key)
{
	g_free(il_verse);
	il_verse = (key && *key) ? g_strdup(key) : NULL;
}

void
main_interlineal_cerrar_verso(void)
{
	g_free(il_verse);
	il_verse = NULL;
}

const char *
main_interlineal_verso_abierto(void)
{
	return il_verse;
}

gboolean
main_interlineal_bloquea_navegacion(void)
{
	/* El aparato se ancla al versículo; el scroll y la barra siguen. */
	return FALSE;
}

#define IL_CIERRA_A_VERSICULOS 10

static int
verse_gap(const char *a, const char *b)
{
	VerseKey ka, kb;
	int n = 0;

	if (!a || !b)
		return IL_CIERRA_A_VERSICULOS;
	ka.setAutoNormalize(1);
	kb.setAutoNormalize(1);
	ka.setText(a);
	kb.setText(b);
	if (ka.getBook() != kb.getBook())
		return IL_CIERRA_A_VERSICULOS;
	if (kb.compare(ka) < 0) {
		VerseKey tmp = ka;
		ka = kb;
		kb = tmp;
	}
	while (ka.compare(kb) < 0 && n < IL_CIERRA_A_VERSICULOS) {
		ka++;
		n++;
	}
	return n;
}

gboolean
main_interlineal_quizas_plegar(const char *key)
{
	if (!il_verse || !key || !*key)
		return FALSE;
	if (verse_keys_match(il_verse, key))
		return FALSE;
	if (verse_gap(il_verse, key) < IL_CIERRA_A_VERSICULOS)
		return FALSE;
	main_interlineal_cerrar_verso();
	settings.show_interlineal = 0;
	xml_set_or_create_value("misc", "show_interlineal", "0");
	gui_interlineal_rellenar();
	gui_lectura_sync_ficha_clear();
	return TRUE;
}

static gchar *
suavizar_es(const char *s)
{
	const gchar *p;
	gboolean all_upper = TRUE, cap = TRUE;
	GString *out;
	gunichar c;

	if (!s || !*s)
		return g_strdup("");
	for (p = s; *p; p = g_utf8_next_char(p)) {
		c = g_utf8_get_char(p);
		if (g_unichar_isalpha(c) && g_unichar_islower(c)) {
			all_upper = FALSE;
			break;
		}
	}
	if (!all_upper)
		return g_strdup(s);
	out = g_string_new(NULL);
	for (p = s; *p; p = g_utf8_next_char(p)) {
		gchar buf[8];
		gint n;
		c = g_utf8_get_char(p);
		if (g_unichar_isalpha(c)) {
			c = cap ? g_unichar_totitle(c) : g_unichar_tolower(c);
			cap = FALSE;
		} else if (g_unichar_isspace(c) || c == '-' || c == 0x2014)
			cap = TRUE;
		n = g_unichar_to_utf8(c, buf);
		g_string_append_len(out, buf, n);
	}
	return g_string_free(out, FALSE);
}

static gboolean
es_frase(const char *s)
{
	const gchar *p;
	if (!s)
		return FALSE;
	for (p = s; *p; p = g_utf8_next_char(p)) {
		if (g_unichar_isspace(g_utf8_get_char(p)))
			return TRUE;
	}
	return FALSE;
}

/* En los módulos conviven dos convenios de numeración Strong:
 *
 *   - Tischendorf numera por LEMA: toda forma de εἰμί es G1510.
 *   - La Reina-Valera 1909, como toda la tradición KJV / Textus
 *     Receptus, da número propio a ciertas formas flexionadas: ἔστω
 *     es G2077, ἐστίν G2076, ὑμῶν G5216.
 *
 * Emparejar por número falla entonces justo en las palabras más
 * frecuentes -- el verbo ser y los pronombres -- y esas celdas caen a
 * la glosa del lema. Esta tabla lleva la forma a su lema para poder
 * compararlas.
 *
 * Solo entran equivalencias que Tischendorf no emite nunca por su
 * cuenta: si usara el número de la forma, fundirlo con el del lema
 * emparejaría palabras distintas. Por eso quedan fuera ἐρέω (G2046),
 * φημί (G5346) y εἴδω (G1492), que Tischendorf sí distingue de λέγω y
 * de ὁράω. Y por eso el singular no se funde con el plural: σύ (G4771)
 * y ὑμεῖς (G5210) ya van separados en el original, y fundirlos dejaría
 * «vosotros» colgando de un σοι.
 *
 * Medido sobre 110.000 palabras del NT griego, los emparejamientos por
 * número suben del 66% al 72%. */
static const struct {
	const char *forma;
	const char *lema;
} STRONG_TR_LEMA[] = {
	/* εἰμί -- ser */
	{ "G1488", "G1510" },	/* εἶ       */
	{ "G1498", "G1510" },	/* εἴην     */
	{ "G1511", "G1510" },	/* εἶναι    */
	{ "G1526", "G1510" },	/* εἰσί     */
	{ "G2070", "G1510" },	/* ἐσμέν    */
	{ "G2071", "G1510" },	/* ἔσομαι   */
	{ "G2075", "G1510" },	/* ἐστέ     */
	{ "G2076", "G1510" },	/* ἐστί     */
	{ "G2077", "G1510" },	/* ἔστω     */
	{ "G2258", "G1510" },	/* ἦν       */
	{ "G2277", "G1510" },	/* ἤτω      */
	{ "G2468", "G1510" },	/* ἴσθι     */
	{ "G5600", "G1510" },	/* ὦ        */
	{ "G5607", "G1510" },	/* ὤν       */
	/* σύ -- tú (singular) */
	{ "G4571", "G4771" },	/* σέ       */
	{ "G4671", "G4771" },	/* σοί      */
	{ "G4675", "G4771" },	/* σοῦ      */
	/* ὑμεῖς -- vosotros (plural) */
	{ "G5209", "G5210" },	/* ὑμᾶς     */
	{ "G5213", "G5210" },	/* ὑμῖν     */
	{ "G5216", "G5210" },	/* ὑμῶν     */
	/* ἐγώ -- yo (singular) */
	{ "G1691", "G1473" },	/* ἐμέ      */
	{ "G1698", "G1473" },	/* ἐμοί     */
	{ "G1700", "G1473" },	/* ἐμοῦ     */
	{ "G3165", "G1473" },	/* μέ       */
	{ "G3427", "G1473" },	/* μοί      */
	{ "G3450", "G1473" },	/* μοῦ      */
	/* ἡμεῖς -- nosotros (plural) */
	{ "G2248", "G2249" },	/* ἡμᾶς     */
	{ "G2254", "G2249" },	/* ἡμῖν     */
	{ "G2257", "G2249" },	/* ἡμῶν     */
	/* οὗτος -- este */
	{ "G5023", "G3778" },	/* ταῦτα    */
	{ "G5025", "G3778" },	/* ταύταις  */
	{ "G5026", "G3778" },	/* ταύτῃ    */
	{ "G5124", "G3778" },	/* τοῦτο    */
	{ "G5125", "G3778" },	/* τούτοις  */
	{ "G5126", "G3778" },	/* τοῦτον   */
	{ "G5127", "G3778" },	/* τούτου   */
	{ "G5128", "G3778" },	/* τούτους  */
	{ "G5129", "G3778" },	/* τούτῳ    */
	{ "G5130", "G3778" },	/* τούτων   */
	/* λέγω -- decir: el aoristo va aparte en la numeración TR */
	{ "G2036", "G3004" },	/* ἔπω      */
	{ "G4483", "G3004" },	/* ῥέω      */
	/* ὁράω -- ver */
	{ "G3700", "G3708" },	/* ὀπτάνομαι */
};

/* El número con el que comparar: el del lema si es una forma del
 * convenio TR, y el mismo que entró en cualquier otro caso. */
static const char *
strong_canonico(const char *num)
{
	gsize i;

	if (!num || !*num)
		return num;
	for (i = 0; i < G_N_ELEMENTS(STRONG_TR_LEMA); i++)
		if (!strcmp(num, STRONG_TR_LEMA[i].forma))
			return STRONG_TR_LEMA[i].lema;
	return num;
}

static gboolean
token_has_strong(const InterlTok *t, const char *num)
{
	gchar *n, **v;
	const char *busca;
	int i;
	gboolean ok = FALSE;

	if (!t || !num || !*num)
		return FALSE;
	/* Por el lema en los dos lados: el original y la versión española
	 * pueden venir de convenios de numeración distintos. */
	busca = strong_canonico(num);
	n = main_interlineal_norm_strong(t->strong);
	if (n && !strcmp(strong_canonico(n), busca))
		ok = TRUE;
	g_free(n);
	if (ok)
		return TRUE;
	if (!t->strongs)
		return FALSE;
	v = g_strsplit(t->strongs, " ", -1);
	for (i = 0; v[i]; i++) {
		n = main_interlineal_norm_strong(v[i]);
		if (n && !strcmp(strong_canonico(n), busca))
			ok = TRUE;
		g_free(n);
		if (ok)
			break;
	}
	g_strfreev(v);
	return ok;
}

static GList *
strongs_of_tok(const InterlTok *t)
{
	GList *out = NULL;
	gchar **v;
	int i;

	if (!t)
		return NULL;
	if (t->strongs && *t->strongs) {
		v = g_strsplit(t->strongs, " ", -1);
		for (i = 0; v[i]; i++) {
			gchar *n = main_interlineal_norm_strong(v[i]);
			if (n && *n)
				out = g_list_append(out, n);
			else
				g_free(n);
		}
		g_strfreev(v);
		return out;
	}
	if (t->strong && *t->strong)
		out = g_list_append(out, g_strdup(t->strong));
	return out;
}

static InterlTok *
find_unused_strong(GList *list, gboolean *used, const char *num)
{
	int i = 0;
	GList *l;

	if (!num)
		return NULL;
	for (l = list; l; l = l->next, i++) {
		InterlTok *t = (InterlTok *)l->data;
		if (used[i])
			continue;
		if (token_has_strong(t, num)) {
			used[i] = TRUE;
			return t;
		}
	}
	return NULL;
}

static gchar *
join_nonempty(const char *a, const char *b, const char *sep)
{
	if (a && *a && b && *b)
		return g_strdup_printf("%s%s%s", a, sep, b);
	if (a && *a)
		return g_strdup(a);
	if (b && *b)
		return g_strdup(b);
	return g_strdup("");
}

static InterlFila *
fila_new(void)
{
	return g_new0(InterlFila, 1);
}

static void
fila_fill_orig(InterlFila *f, const InterlTok *t)
{
	const InterlStrong *info;

	if (!f || !t)
		return;
	if (t->forma && *t->forma) {
		gchar *prev = f->forma;
		f->forma = join_nonempty(prev, t->forma, "  ");
		g_free(prev);
	}
	if (t->raiz && *t->raiz && (!f->raiz || !*f->raiz))
		f->raiz = g_strdup(t->raiz);
	if (t->translit && *t->translit && (!f->translit || !*f->translit))
		f->translit = g_strdup(t->translit);
	if (t->morph && *t->morph && (!f->morph || !*f->morph)) {
		g_free(f->morph);
		g_free(f->morph_es);
		f->morph = main_morf_codigo(t->morph);
		f->morph_es = main_morf_es(t->morph);
		f->morph_corto = main_morf_corto(t->morph);
	}
	if (t->strong && *t->strong) {
		gchar *n = main_interlineal_norm_strong(t->strong);
		if (n && n[0] == 'H')
			f->hebrew = TRUE;
		if (!f->strong)
			f->strong = n;
		else
			g_free(n);
	}
	info = t->strong ? main_interlineal_strong(t->strong) : NULL;
	if (info) {
		if ((!f->raiz || !*f->raiz) && info->lema)
			f->raiz = g_strdup(info->lema);
		if ((!f->translit || !*f->translit) && info->translit)
			f->translit = g_strdup(info->translit);
		if ((!f->forma || !*f->forma) && info->lema)
			f->forma = g_strdup(info->lema);
	}
}

static void
fila_add_strong_disp(InterlFila *f, const char *num)
{
	gchar *n = main_interlineal_norm_strong(num);
	if (!n)
		return;
	if (!f->strong)
		f->strong = g_strdup(n);
	if (f->strongs && *f->strongs) {
		if (!strstr(f->strongs, n)) {
			gchar *prev = f->strongs;
			f->strongs = g_strdup_printf("%s · %s", prev, n);
			g_free(prev);
		}
	} else {
		g_free(f->strongs);
		f->strongs = g_strdup(n);
	}
	if (n[0] == 'H')
		f->hebrew = TRUE;
	g_free(n);
}

static void
fila_free(gpointer p)
{
	InterlFila *f = (InterlFila *)p;
	if (!f)
		return;
	g_free(f->es);
	g_free(f->strong);
	g_free(f->strongs);
	g_free(f->forma);
	g_free(f->raiz);
	g_free(f->translit);
	g_free(f->morph);
	g_free(f->morph_es);
	g_free(f->morph_corto);
	g_free(f);
}

void
main_interlineal_filas_free(GList *filas)
{
	g_list_free_full(filas, fila_free);
}

static gchar *il_pie_es = NULL;

const char *
main_interlineal_pie_es(void)
{
	return il_pie_es;
}

static void
set_pie_es(const char *s)
{
	g_free(il_pie_es);
	il_pie_es = (s && *s) ? g_strdup(s) : NULL;
}

static gboolean
raw_tiene_strongs(const char *raw)
{
	return raw && (strstr(raw, "strong:") || strstr(raw, "Strong:"));
}

static GList *
tokens_es_from_mod(const char *mod, const char *key)
{
	char *raw;
	GList *toks;

	if (!mod_ok(mod) || !key)
		return NULL;
	raw = backend->get_raw_text(mod, key);
	if (!raw_tiene_strongs(raw)) {
		g_free(raw);
		return NULL;
	}
	toks = parse_w_tags(raw);
	g_free(raw);
	return toks;
}

/* Pospositivas griegas: partículas que en griego no abren nunca la
 * frase, pero cuya traducción al español sí la abre. «ἔστω δὲ ὁ λόγος»
 * se lee «Mas sea vuestro hablar»: la segunda del griego es la primera
 * del español, y de ahí en adelante todo va corrido un puesto.
 *
 * Importa porque emparejar por posición da por hecho que la palabra n
 * del original es la n del español, y el recuento de palabras -- lo
 * único que mira alineacion_ok() -- no lo desmiente: en Mt 5:37 salen
 * 18 palabras contra 17 tokens, diferencia de una, y la alineación se
 * da por buena aunque esté mal de la primera fila. No es un caso raro:
 * δέ, γάρ y οὖν abren miles de versículos. */
static gboolean
pospositiva_en_segunda(GList *orig)
{
	static const char *const POSPOS[] = {
		"G1161",	/* δέ     */
		"G1063",	/* γάρ    */
		"G3767",	/* οὖν    */
		"G3303",	/* μέν    */
		"G5037",	/* τε     */
		"G1065",	/* γε     */
		"G1211",	/* δή     */
		"G3305",	/* μέντοι */
		"G5106",	/* τοίνυν */
		NULL
	};
	GList *st = toks_con_strong(orig);
	gboolean hit = FALSE;

	if (st && st->next) {
		const InterlTok *t = (const InterlTok *)st->next->data;
		int i;

		for (i = 0; t && t->strong && POSPOS[i]; i++)
			if (!g_strcmp0(t->strong, POSPOS[i])) {
				hit = TRUE;
				break;
			}
	}
	g_list_free(st);
	return hit;
}

static GList *
filas_reverse(GList *orig, GList *es, const char *key)
{
	GList *out = NULL, *l;
	int norig = (int)g_list_length(orig);
	gboolean *used = norig ? g_new0(gboolean, norig) : NULL;

	(void)key;
	for (l = es; l; l = l->next) {
		InterlTok *et = (InterlTok *)l->data;
		InterlFila *f;
		GList *nums, *n;
		gboolean any = FALSE;

		if (!et || !et->forma || !*et->forma)
			continue;
		if (!et->strong && !et->strongs)
			continue;
		f = fila_new();
		f->es = suavizar_es(et->forma);
		f->phrase = es_frase(f->es);
		nums = strongs_of_tok(et);
		for (n = nums; n; n = n->next) {
			InterlTok *ot = find_unused_strong(orig, used,
							   (const char *)n->data);
			fila_add_strong_disp(f, (const char *)n->data);
			if (ot) {
				fila_fill_orig(f, ot);
				any = TRUE;
			} else {
				const InterlStrong *info =
				    main_interlineal_strong((const char *)n->data);
				if (info) {
					if (!f->forma && info->lema)
						f->forma = g_strdup(info->lema);
					if (!f->translit && info->translit)
						f->translit = g_strdup(info->translit);
					if (!f->raiz && info->lema)
						f->raiz = g_strdup(info->lema);
					any = TRUE;
				}
			}
		}
		g_list_free_full(nums, g_free);
		if (!any && !f->strong) {
			fila_free(f);
			continue;
		}
		out = g_list_append(out, f);
	}
	g_free(used);
	return out;
}

static GList *
filas_forward(GList *orig, GList *es)
{
	GList *out = NULL, *l;
	int nes = (int)g_list_length(es);
	gboolean *used = nes ? g_new0(gboolean, nes) : NULL;

	for (l = orig; l; l = l->next) {
		InterlTok *ot = (InterlTok *)l->data;
		InterlFila *f;
		InterlTok *et = NULL;
		GList *nums, *n;

		if (!ot || !ot->forma || !*ot->forma)
			continue;
		f = fila_new();
		fila_fill_orig(f, ot);
		nums = strongs_of_tok(ot);
		for (n = nums; n; n = n->next) {
			fila_add_strong_disp(f, (const char *)n->data);
			if (!et)
				et = find_unused_strong(es, used, (const char *)n->data);
		}
		g_list_free_full(nums, g_free);
		if (et && et->forma && *et->forma)
			f->es = suavizar_es(et->forma);
		else if (ot->glosa && *ot->glosa)
			f->es = suavizar_es(ot->glosa);
		else
			f->es = g_strdup("");
		f->phrase = es_frase(f->es);
		out = g_list_append(out, f);
	}
	g_free(used);
	return out;
}

static GList *
filas_secuencial(GList *orig, const char *key, gboolean reverse)
{
	GList *pal, *st, *out = NULL, *l, *t;
	gchar *esline;
	int n_es = 0, n_st, n_stop = 0;
	gboolean fiable, saltar = FALSE;

	esline = spanish_line(key);
	pal = parse_es_palabras(esline);
	st = toks_con_strong(orig);
	n_st = (int)g_list_length(st);
	for (l = pal; l; l = l->next) {
		EsPal *w = (EsPal *)l->data;
		if (!w->nota && w->core && *w->core) {
			n_es++;
			if (es_stopword(w->core))
				n_stop++;
		}
	}
	fiable = alineacion_ok(n_es, n_st);
	if (!fiable && n_es > n_st && alineacion_ok(n_es - n_stop, n_st)) {
		fiable = TRUE;
		saltar = TRUE;
	}
	/* Cuadren o no los recuentos, con una pospositiva en segundo lugar
	 * el orden de las dos lenguas ya no coincide: mejor dejar la
	 * columna del original vacía (reverse) o dar la glosa del lema
	 * (forward) que colgar a cada palabra la del vecino. */
	if (fiable && pospositiva_en_segunda(orig)) {
		fiable = FALSE;
		saltar = FALSE;
	}
	if (reverse) {
		t = st;
		for (l = pal; l; l = l->next) {
			EsPal *w = (EsPal *)l->data;
			InterlFila *f;
			if (w->nota || !w->core || !*w->core)
				continue;
			if (saltar && es_stopword(w->core) && n_es > n_st) {
				n_es--;
				continue;
			}
			f = fila_new();
			f->es = suavizar_es(w->core);
			f->phrase = es_frase(f->es);
			if (fiable && t) {
				fila_fill_orig(f, (InterlTok *)t->data);
				fila_add_strong_disp(f, ((InterlTok *)t->data)->strong);
				t = t->next;
			}
			out = g_list_append(out, f);
		}
	} else {
		GList *pl = pal;
		for (l = orig; l; l = l->next) {
			InterlTok *ot = (InterlTok *)l->data;
			InterlFila *f = fila_new();
			fila_fill_orig(f, ot);
			fila_add_strong_disp(f, ot->strong);
			if (fiable) {
				while (pl) {
					EsPal *w = (EsPal *)pl->data;
					pl = pl->next;
					if (w->nota || !w->core || !*w->core)
						continue;
					if (saltar && es_stopword(w->core))
						continue;
					f->es = suavizar_es(w->core);
					break;
				}
			}
			if (!f->es)
				f->es = suavizar_es(ot->glosa);
			f->phrase = es_frase(f->es);
			out = g_list_append(out, f);
		}
	}
	g_list_free(st);
	g_list_free_full(pal, espal_free);
	g_free(esline);
	return out;
}

GList *
main_interlineal_filas(const char *key, gboolean reverse)
{
	GList *orig, *es, *out = NULL;
	const char *cur;

	if (!key || !*key)
		return NULL;
	load_strongs();
	set_pie_es(NULL);
	orig = main_interlineal_versiculo(key);
	cur = settings.MainWindowModule;
	es = cur ? tokens_es_from_mod(cur, key) : NULL;
	if (es && orig)
		out = reverse ? filas_reverse(orig, es, key)
			      : filas_forward(orig, es);
	main_interlineal_tokens_free(es);
	es = NULL;
	if (out) {
		main_interlineal_tokens_free(orig);
		return out;
	}
	/* La versión en pantalla no viene etiquetada. Antes de emparejar
	 * por posición se prueba la Reina-Valera 1909, que sí lo está:
	 * casar por número de Strong es una correspondencia de verdad,
	 * mientras que la posición solo acierta cuando las dos lenguas
	 * ordenan igual, y el griego rara vez lo hace. Donde el número no
	 * aparezca en la RV1909 esa celda cae a la glosa del lema, que
	 * dice algo cierto de esa palabra griega; el desfase posicional,
	 * en cambio, le cuelga la palabra de la de al lado. */
	es = tokens_es_from_mod("SpaRV1909", key);
	if (!es)
		es = tokens_es_from_mod("SpaRV", key);
	if (es && orig)
		out = reverse ? filas_reverse(orig, es, key)
			      : filas_forward(orig, es);
	main_interlineal_tokens_free(es);
	es = NULL;
	if (out) {
		set_pie_es(_("Equivalencia RV1909 — esta versión no trae Strong's"));
		main_interlineal_tokens_free(orig);
		return out;
	}

	/* Sin ninguna versión etiquetada a mano queda la posición, que
	 * filas_secuencial() ya sabe descartar sola cuando no se sostiene. */
	out = filas_secuencial(orig, key, reverse);
	main_interlineal_tokens_free(orig);
	return out;
}

gboolean
main_interlineal_modo_reverse(void)
{
	return il_reverse;
}

void
main_interlineal_set_modo_reverse(gboolean reverse)
{
	il_reverse = reverse ? TRUE : FALSE;
	xml_set_or_create_value("misc", "interlineal_reverse",
				il_reverse ? "1" : "0");
}

gchar *
main_interlineal_html_original(const char *key)
{
	GList *toks, *l;
	GString *out;
	int test = 2;
	gboolean rtl;
	gboolean any = FALSE;

	if (!il_verse || !key || !verse_keys_match(il_verse, key))
		return NULL;

	load_strongs();
	if (mod_ok("KJV"))
		test = backend->get_key_testament("KJV", key);
	else if (settings.MainWindowModule)
		test = backend->get_key_testament(settings.MainWindowModule, key);
	rtl = (test == 1);
	toks = main_interlineal_versiculo(key);

	out = g_string_new(NULL);
	{
		const char *fg = settings.darktheme ? "#EDE3C8" : "#2A2218";
		const char *lg = settings.darktheme ? "#B9A882" : "#6B5A3C";
		g_string_append_printf(out,
				       "<p class=\"ilblock illabel\" style=\"color:%s\">%s</p>"
				       "<p class=\"ilblock ilorig\" style=\"color:%s\"%s>",
				       lg,
				       rtl ? _("Hebreo · WLC") : _("Griego · Tischendorf"),
				       fg,
				       rtl ? " dir=\"rtl\"" : "");
	}

	for (l = toks; l; l = l->next) {
		InterlTok *t = (InterlTok *)l->data;
		gchar *w, *st;
		if (!t || !t->forma || !*t->forma)
			continue;
		w = g_markup_escape_text(t->forma, -1);
		if (t->strong && *t->strong) {
			gchar *mo = NULL;

			st = g_markup_escape_text(t->strong, -1);
			/* La morfología viaja con el enlace, con '+' donde
			 * hay espacios: así la ficha puede decir cómo está
			 * la palabra en este versículo. */
			if (t->morph && *t->morph) {
				gchar *plano = g_strdelimit(g_strdup(t->morph),
							    " \t", '+');

				mo = g_markup_escape_text(plano, -1);
				g_free(plano);
			}
			g_string_append_printf(out,
					       "%s<a class=\"ilw\" href=\"passagestudy.jsp?"
					       "action=showInterlineal&amp;value=%s%s%s\">%s</a>",
					       any ? "\xE2\x80\x83" : "", st,
					       mo ? "&amp;morph=" : "",
					       mo ? mo : "", w);
			g_free(mo);
			g_free(st);
		} else {
			g_string_append_printf(out, "%s%s", any ? "\xE2\x80\x83" : "", w);
		}
		g_free(w);
		any = TRUE;
	}
	main_interlineal_tokens_free(toks);

	if (!any) {
		const char *mod = rtl ? "WLC" : "Tisch";
		char *plain = NULL;
		if (mod_ok(mod))
			plain = backend->get_strip_text(mod, key);
		if (plain && *plain) {
			gchar *esc = g_markup_escape_text(plain, -1);
			g_string_append(out, esc);
			g_free(esc);
			any = TRUE;
		}
		g_free(plain);
	}

	g_string_append(out, "</p>");
	if (!any) {
		g_string_free(out, TRUE);
		return NULL;
	}
	{
		gchar *esc_key = g_markup_escape_text(key, -1);
		g_string_append_printf(out,
				       "<span class=\"iltable\" data-key=\"%s\"> </span>",
				       esc_key);
		g_free(esc_key);
	}
	return g_string_free(out, FALSE);
}

void
main_interlineal_actualizar(void)
{
	gui_interlineal_rellenar();
}

void
main_verse_tools_xrefs(const char *key)
{
	gchar *mod;
	GString *refs;
	int i;

	if (!key || !*key || !backend)
		return;
	mod = settings.MainWindowModule;
	if (!mod || !*mod)
		return;
	backend->set_module_key(mod, (gchar *)key);
	refs = g_string_new(NULL);
	for (i = 0; i < 24; i++) {
		char idx[8];
		char *list;
		g_snprintf(idx, sizeof(idx), "%d", i);
		list = backend->get_entry_attribute("Footnote", idx, "refList");
		if (!list || !*list) {
			g_free(list);
			if (i == 0)
				continue;
			break;
		}
		if (refs->len)
			g_string_append_c(refs, ' ');
		g_string_append(refs, list);
		g_free(list);
	}
	if (!refs->len) {
		gui_generic_warning(_("Este versículo no tiene referencias cruzadas."));
		g_string_free(refs, TRUE);
		return;
	}
	main_display_verse_list_in_sidebar((gchar *)key, mod, refs->str);
	g_string_free(refs, TRUE);
}
