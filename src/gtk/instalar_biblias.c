/*
 * Biblia Elim — diálogo simple para descargar e instalar Biblias.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "gui/instalar_biblias.h"
#include "gui/dialog.h"
#include "gui/lectura_sync.h"
#include "gui/mod_mgr.h"
#include "gui/sidebar.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/mod_mgr.h"
#include "main/settings.h"
#include "main/sidebar.h"
#include "main/sword.h"

#include "gui/debug_glib_null.h"

enum {
	COL_CHECK,
	COL_DESC,
	COL_NAME,
	COL_LANG,
	COL_TYPE,
	COL_SIZE,
	COL_STATUS,
	COL_INSTALLED,
	N_COLS
};

static GtkWidget *dlg = NULL;
static GtkWidget *combo_src = NULL;
static GtkWidget *combo_lang = NULL;
static GtkWidget *search_entry = NULL;
static GtkWidget *chk_bibles = NULL;
static GtkWidget *tree = NULL;
static GtkListStore *store = NULL;
static GtkWidget *progress = NULL;
static GtkWidget *status_lbl = NULL;
static GtkWidget *btn_refresh = NULL;
static GtkWidget *btn_install = NULL;
static GtkWidget *btn_advanced = NULL;
static GtkWidget *btn_close = NULL;
static GtkWidget *count_lbl = NULL;

static GPtrArray *mods = NULL;
static gchar *dest_dir = NULL;
static gboolean busy = FALSE;
static gboolean filling = FALSE;
static gboolean inited = FALSE;

static void load_catalog(gboolean force_refresh);
static void refill_view(void);
static void set_status(const char *msg);

static void
free_mod(gpointer data)
{
	MOD_MGR *m = (MOD_MGR *)data;
	if (!m)
		return;
	g_free(m->name);
	g_free(m->abbreviation);
	g_free(m->type);
	g_free(m->old_version);
	g_free(m->new_version);
	g_free(m->min_version);
	g_free(m->description);
	g_free(m->installsize);
	g_free(m->about);
	g_free(m);
}

static void
free_source(gpointer data)
{
	MOD_MGR_SOURCE *s = (MOD_MGR_SOURCE *)data;
	if (!s)
		return;
	g_free((gchar *)s->caption);
	g_free((gchar *)s->type);
	g_free((gchar *)s->source);
	g_free((gchar *)s->directory);
	g_free((gchar *)s->user);
	g_free((gchar *)s->pass);
	g_free((gchar *)s->uid);
	g_free(s);
}

static int
source_rank(const char *cap)
{
	if (!cap)
		return 99;
	if (!strcmp(cap, "CrossWire"))
		return 0;
	if (!strcmp(cap, "CrossWire HTTPS"))
		return 1;
	if (!strcmp(cap, "eBible.org"))
		return 2;
	if (!strcmp(cap, "eBible.org HTTPS"))
		return 3;
	if (!strcmp(cap, "CrossWire Attic"))
		return 4;
	if (!strcmp(cap, "STEP Bible"))
		return 5;
	if (!strcmp(cap, "IBT"))
		return 6;
	if (!strcmp(cap, "Bible.org"))
		return 7;
	if (!strcmp(cap, "Xiphos"))
		return 8;
	return 50;
}

static gboolean
lang_is(const char *lang, const char *a, const char *b)
{
	if (!lang || !*lang)
		return FALSE;
	if (a && strcmp(lang, a) == 0)
		return TRUE;
	if (b && strcmp(lang, b) == 0)
		return TRUE;
	if (a && !g_ascii_strcasecmp(lang, a))
		return TRUE;
	if (b && !g_ascii_strcasecmp(lang, b))
		return TRUE;
	return FALSE;
}

static gboolean
lang_is_spanish(const char *lang)
{
	return lang_is(lang, "Español", "Spanish");
}

static int
lang_rank(const char *lang)
{
	if (!lang || !*lang)
		return 20;
	if (lang_is_spanish(lang))
		return 0;
	if (lang_is(lang, "English", "Inglés") ||
	    g_str_has_prefix(lang, "English") ||
	    g_str_has_prefix(lang, "American English"))
		return 1;
	if (lang_is(lang, "עברית", "Hebrew") || lang_is(lang, "Hebreo", NULL))
		return 2;
	if ((lang && strstr(lang, "λληνικ")) ||
	    lang_is(lang, "Greek", "Griego"))
		return 3;
	if (lang_is(lang, "Latina", "Latin") || lang_is(lang, "Latín", NULL))
		return 4;
	return 10;
}

static const char *
lang_label(const char *lang)
{
	if (!lang || !*lang)
		return _("Desconocido");
	if (lang_is_spanish(lang))
		return _("Español");
	if (lang_is(lang, "English", "Inglés") ||
	    g_str_has_prefix(lang, "English") ||
	    g_str_has_prefix(lang, "American English"))
		return _("Inglés");
	if (lang_is(lang, "עברית", "Hebrew") || lang_is(lang, "Hebreo", NULL))
		return _("Hebreo");
	if ((lang && strstr(lang, "λληνικ")) ||
	    lang_is(lang, "Greek", "Griego"))
		return _("Griego");
	if (lang_is(lang, "Latina", "Latin") || lang_is(lang, "Latín", NULL))
		return _("Latín");
	if (lang_is(lang, "Français", "French") || lang_is(lang, "Francés", NULL))
		return _("Francés");
	if (lang_is(lang, "Deutsch", "German") || lang_is(lang, "Alemán", NULL))
		return _("Alemán");
	if (lang_is(lang, "Português", "Portuguese") ||
	    lang_is(lang, "Portugués", NULL))
		return _("Portugués");
	if (lang_is(lang, "Italiano", "Italian"))
		return _("Italiano");
	if (lang_is(lang, "Русский", "Russian") || lang_is(lang, "Ruso", NULL))
		return _("Ruso");
	if (lang_is(lang, "中文", "Chinese") || lang_is(lang, "Chino", NULL))
		return _("Chino");
	if (lang_is(lang, "العربية", "Arabic") || lang_is(lang, "Árabe", NULL))
		return _("Árabe");
	return lang;
}

static const char *
type_label(const MOD_MGR *m)
{
	if (!m || !m->type)
		return _("Otro");
	if (m->is_devotional)
		return _("Devocional");
	if (m->is_maps)
		return _("Mapa");
	if (m->is_glossary)
		return _("Glosario");
	switch (m->type[0]) {
	case 'B':
		return _("Biblia");
	case 'C':
		return _("Comentario");
	case 'L':
		return _("Diccionario");
	case 'G':
		return _("Libro");
	default:
		return m->type;
	}
}

static gboolean
is_bible(const MOD_MGR *m)
{
	return m && m->type && m->type[0] == 'B' &&
	       !m->is_devotional && !m->is_maps && !m->is_glossary;
}

static gint
cmp_mod(gconstpointer a, gconstpointer b)
{
	const MOD_MGR *ma = *(const MOD_MGR *const *)a;
	const MOD_MGR *mb = *(const MOD_MGR *const *)b;
	int ra, rb, c;
	const char *da, *db;

	ra = lang_rank(ma->language);
	rb = lang_rank(mb->language);
	if (ra != rb)
		return ra - rb;
	c = g_utf8_collate(ma->language ? ma->language : "",
			   mb->language ? mb->language : "");
	if (c)
		return c;
	da = ma->description && *ma->description ? ma->description : ma->name;
	db = mb->description && *mb->description ? mb->description : mb->name;
	return g_utf8_collate(da ? da : "", db ? db : "");
}

static gboolean
utf_contains(const char *hay, const char *needle_cf)
{
	gchar *hcf;
	gboolean hit;

	if (!needle_cf || !*needle_cf)
		return TRUE;
	if (!hay || !*hay)
		return FALSE;
	hcf = g_utf8_casefold(hay, -1);
	hit = strstr(hcf, needle_cf) != NULL;
	g_free(hcf);
	return hit;
}

static void
set_status(const char *msg)
{
	if (status_lbl)
		gtk_label_set_text(GTK_LABEL(status_lbl), msg ? msg : "");
	sync_windows();
}

static void
set_progress_text(const char *msg)
{
	if (progress)
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress),
					  msg ? msg : "");
	sync_windows();
}

static void
set_busy(gboolean on)
{
	GdkWindow *win;

	busy = on;
	gtk_widget_set_sensitive(combo_src, !on);
	gtk_widget_set_sensitive(combo_lang, !on);
	gtk_widget_set_sensitive(search_entry, !on);
	gtk_widget_set_sensitive(chk_bibles, !on);
	gtk_widget_set_sensitive(tree, !on);
	gtk_widget_set_sensitive(btn_refresh, !on);
	if (on)
		gtk_widget_set_sensitive(btn_install, FALSE);
	gtk_widget_set_sensitive(btn_advanced, !on);
	gtk_widget_set_sensitive(btn_close, !on);

	if (!dlg)
		return;
	win = gtk_widget_get_window(dlg);
	if (win) {
		GdkCursor *cur = NULL;
		if (on)
			cur = gdk_cursor_new_from_name(gdk_window_get_display(win),
						       "wait");
		gdk_window_set_cursor(win, cur);
		if (cur)
			g_object_unref(cur);
	}
	sync_windows();
}

static const char *
active_source(void)
{
	if (!combo_src)
		return NULL;
	return gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo_src));
}

static gint
cmp_str_ptr(gconstpointer a, gconstpointer b)
{
	const char *sa = *(const char *const *)a;
	const char *sb = *(const char *const *)b;
	return g_utf8_collate(sa ? sa : "", sb ? sb : "");
}

static void
update_install_sensitive(void)
{
	GtkTreeIter iter;
	gboolean valid;
	gboolean any = FALSE;

	if (!store || busy)
		return;
	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
	while (valid) {
		gboolean chk = FALSE, inst = FALSE;
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
				   COL_CHECK, &chk,
				   COL_INSTALLED, &inst, -1);
		if (chk && !inst) {
			any = TRUE;
			break;
		}
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
	}
	gtk_widget_set_sensitive(btn_install, any);
}

static void
on_toggle(GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
	GtkTreeIter iter;
	gboolean val, inst;

	(void)cell;
	(void)data;
	if (busy)
		return;
	if (!gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(store),
						 &iter, path_str))
		return;
	gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
			   COL_CHECK, &val,
			   COL_INSTALLED, &inst, -1);
	if (inst)
		return;
	gtk_list_store_set(store, &iter, COL_CHECK, !val, -1);
	update_install_sensitive();
}

static void
mark_row(const char *name, gboolean ok)
{
	GtkTreeIter iter;
	gboolean valid;

	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
	while (valid) {
		gchar *n = NULL;
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
				   COL_NAME, &n, -1);
		if (n && name && !strcmp(n, name)) {
			if (ok)
				gtk_list_store_set(store, &iter,
						   COL_CHECK, FALSE,
						   COL_INSTALLED, TRUE,
						   COL_STATUS, _("Instalada"),
						   -1);
			else
				gtk_list_store_set(store, &iter,
						   COL_STATUS, _("Error"),
						   -1);
			g_free(n);
			return;
		}
		g_free(n);
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
	}
}

static void
refill_langs(void)
{
	GHashTable *seen;
	GPtrArray *names;
	guint i;
	const gchar *keep;
	gchar *keep_copy;

	filling = TRUE;
	keep = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo_lang));
	keep_copy = g_strdup(keep);
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo_lang));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_lang),
				  "all", _("Todos los idiomas"));

	seen = g_hash_table_new(g_str_hash, g_str_equal);
	names = g_ptr_array_new();
	if (mods) {
		for (i = 0; i < mods->len; i++) {
			MOD_MGR *m = g_ptr_array_index(mods, i);
			const char *lang = m->language ? m->language : "";
			if (!*lang)
				continue;
			if (g_hash_table_contains(seen, lang))
				continue;
			g_hash_table_add(seen, (gpointer)lang);
			g_ptr_array_add(names, (gpointer)lang);
		}
	}
	g_ptr_array_sort(names, cmp_str_ptr);
	/* Spanish first if present. */
	for (i = 0; i < names->len; i++) {
		const char *lang = g_ptr_array_index(names, i);
		if (lang_is_spanish(lang)) {
			gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_lang),
						  lang, lang_label(lang));
			g_ptr_array_remove_index(names, i);
			break;
		}
	}
	for (i = 0; i < names->len; i++) {
		const char *lang = g_ptr_array_index(names, i);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_lang),
					  lang, lang_label(lang));
	}
	g_ptr_array_free(names, TRUE);
	g_hash_table_destroy(seen);

	if (keep_copy && gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo_lang),
						     keep_copy)) {
		/* kept */
	} else if (gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo_lang),
					       "Español") ||
		   gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo_lang),
					       "Spanish")) {
		/* prefer Spanish for this church app */
	} else {
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo_lang), "all");
	}
	g_free(keep_copy);
	filling = FALSE;
}

static void
refill_view(void)
{
	const gchar *lang_id;
	const gchar *needle;
	gchar *needle_cf;
	gboolean only_bibles;
	guint i, vis = 0, inst = 0;
	gchar *count;

	gtk_list_store_clear(store);
	if (!mods)
		return;

	lang_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo_lang));
	needle = gtk_entry_get_text(GTK_ENTRY(search_entry));
	needle_cf = (needle && *needle) ? g_utf8_casefold(needle, -1) : NULL;
	only_bibles = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_bibles));

	for (i = 0; i < mods->len; i++) {
		MOD_MGR *m = g_ptr_array_index(mods, i);
		const char *desc;
		GtkTreeIter iter;
		gboolean hit;

		if (only_bibles && !is_bible(m))
			continue;
		if (lang_id && strcmp(lang_id, "all") != 0) {
			if (!m->language || strcmp(m->language, lang_id) != 0)
				continue;
		}
		desc = (m->description && *m->description) ? m->description
							   : m->name;
		if (needle_cf) {
			hit = utf_contains(desc, needle_cf) ||
			      utf_contains(m->name, needle_cf) ||
			      utf_contains(m->abbreviation, needle_cf) ||
			      utf_contains(lang_label(m->language), needle_cf) ||
			      utf_contains(m->language, needle_cf);
			if (!hit)
				continue;
		}
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   COL_CHECK, FALSE,
				   COL_DESC, desc,
				   COL_NAME, m->name,
				   COL_LANG, lang_label(m->language),
				   COL_TYPE, type_label(m),
				   COL_SIZE, m->installsize ? m->installsize : "",
				   COL_STATUS, m->installed ? _("Instalada")
							    : _("Disponible"),
				   COL_INSTALLED, m->installed ? TRUE : FALSE,
				   -1);
		vis++;
		if (m->installed)
			inst++;
	}
	g_free(needle_cf);

	count = g_strdup_printf(_("%u módulos visibles · %u ya instalados"),
				vis, inst);
	gtk_label_set_text(GTK_LABEL(count_lbl), count);
	g_free(count);
	update_install_sensitive();

	set_progress_text("");
	if (vis == 0)
		set_status(_("Ningún módulo coincide. Cambie el idioma, la búsqueda o pulse Actualizar catálogo."));
	else
		set_status(_("Marque las Biblias y pulse Instalar. Se descargan e instalan en un paso."));
}

static void
clear_mods(void)
{
	if (!mods)
		return;
	g_ptr_array_set_free_func(mods, free_mod);
	g_ptr_array_free(mods, TRUE);
	mods = NULL;
}

static void
load_catalog(gboolean force_refresh)
{
	const char *src;
	GList *list, *t;
	gchar *msg;

	src = active_source();
	if (!src || !*src) {
		set_status(_("No hay fuentes remotas configuradas."));
		return;
	}

	set_busy(TRUE);
	clear_mods();
	gtk_list_store_clear(store);

	if (force_refresh) {
		msg = g_strdup_printf(_("Descargando catálogo de %s…"), src);
		set_status(msg);
		set_progress_text(msg);
		gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress));
		g_free(msg);
		sync_windows();
		if (mod_mgr_refresh_remote_source(src) != 0) {
			set_busy(FALSE);
			set_status(_("No se pudo actualizar el catálogo. Pruebe otra fuente o CrossWire HTTPS."));
			gui_generic_warning(_("No se pudo descargar el catálogo.\n"
					      "Compruebe la red o elija otra fuente (por ejemplo CrossWire HTTPS o eBible.org HTTPS)."));
			return;
		}
	}

	msg = g_strdup_printf(_("Leyendo módulos de %s…"), src);
	set_status(msg);
	set_progress_text(msg);
	g_free(msg);
	sync_windows();

	list = mod_mgr_remote_list_modules(src);
	mods = g_ptr_array_new();
	for (t = list; t; t = t->next)
		g_ptr_array_add(mods, t->data);
	g_list_free(list);
	g_ptr_array_sort(mods, cmp_mod);

	if (mods->len == 0 && !force_refresh) {
		load_catalog(TRUE);
		return;
	}

	refill_langs();
	refill_view();
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 0);
	set_busy(FALSE);
	update_install_sensitive();
	if (mods->len == 0)
		set_status(_("Este repositorio no devolvió módulos."));
}

static void
on_src_changed(GtkComboBox *combo, gpointer data)
{
	(void)combo;
	(void)data;
	if (filling || busy)
		return;
	load_catalog(FALSE);
}

static void
on_filter_changed(GtkWidget *w, gpointer data)
{
	(void)w;
	(void)data;
	if (filling || busy)
		return;
	refill_view();
}

static void
on_ib_refresh_clicked(GtkButton *b, gpointer data)
{
	(void)b;
	(void)data;
	load_catalog(TRUE);
}

static void
on_ib_install_clicked(GtkButton *b, gpointer data)
{
	GtkTreeIter iter;
	gboolean valid;
	GPtrArray *todo;
	const char *src;
	guint i;
	int ok = 0, fail = 0;
	gchar *summary;

	(void)b;
	(void)data;
	if (busy)
		return;

	src = active_source();
	if (!src || !*src)
		return;

	todo = g_ptr_array_new_with_free_func(g_free);
	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
	while (valid) {
		gboolean chk = FALSE, inst = FALSE;
		gchar *name = NULL;
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
				   COL_CHECK, &chk,
				   COL_INSTALLED, &inst,
				   COL_NAME, &name, -1);
		if (chk && !inst && name)
			g_ptr_array_add(todo, name);
		else
			g_free(name);
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
	}

	if (todo->len == 0) {
		gui_generic_warning(_("Marque al menos una Biblia para instalar."));
		g_ptr_array_free(todo, TRUE);
		return;
	}

	set_busy(TRUE);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), 0);
	for (i = 0; i < todo->len; i++) {
		const char *name = (const char *)g_ptr_array_index(todo, i);
		gchar *msg;
		int rc;

		msg = g_strdup_printf(_("Descargando e instalando %s (%u de %u)…"),
				      name, i + 1, todo->len);
		set_status(msg);
		set_progress_text(msg);
		g_free(msg);
		gui_mod_mgr_set_current_mod(name);
		sync_windows();

		rc = mod_mgr_remote_install(dest_dir, src, name);
		if (rc == 0) {
			ok++;
			mark_row(name, TRUE);
		} else {
			fail++;
			mark_row(name, FALSE);
		}
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress),
					      (gdouble)(i + 1) / (gdouble)todo->len);
		sync_windows();
	}
	gui_mod_mgr_set_current_mod(NULL);

	main_update_module_lists();
	if (sidebar.module_list)
		main_load_module_tree(sidebar.module_list);
	gui_lectura_sync_rellenar_combo();

	/* installed flags in the cached catalog */
	if (mods) {
		for (i = 0; i < mods->len; i++) {
			MOD_MGR *m = g_ptr_array_index(mods, i);
			if (m->name && main_is_module((char *)m->name))
				m->installed = 1;
		}
	}

	g_ptr_array_free(todo, TRUE);
	set_busy(FALSE);
	update_install_sensitive();

	if (fail && ok)
		summary = g_strdup_printf(_("%d instaladas, %d con error. Las nuevas ya aparecen en Comparar y en el selector."),
					  ok, fail);
	else if (fail)
		summary = g_strdup_printf(_("No se pudieron instalar %d módulos. Pruebe otra fuente o Actualizar catálogo."),
					  fail);
	else
		summary = g_strdup_printf(_("%d Biblias instaladas. Ya aparecen en Comparar y en el selector."),
					  ok);
	set_status(summary);
	set_progress_text(summary);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress), fail ? 0 : 1);
	g_free(summary);
}

static void
on_advanced_clicked(GtkButton *b, gpointer data)
{
	(void)b;
	(void)data;
	if (busy)
		return;
	gtk_widget_destroy(dlg);
	gui_open_mod_mgr();
}

static void
on_close_clicked(GtkButton *b, gpointer data)
{
	(void)b;
	(void)data;
	if (busy)
		return;
	gtk_widget_destroy(dlg);
}

static gboolean
on_delete(GtkWidget *w, GdkEvent *e, gpointer data)
{
	(void)w;
	(void)e;
	(void)data;
	return busy;
}

static gboolean
on_key_press(GtkWidget *w, GdkEventKey *e, gpointer data)
{
	(void)w;
	(void)data;
	if (busy)
		return TRUE;
	if (e->keyval == GDK_KEY_Escape) {
		gtk_widget_destroy(dlg);
		return TRUE;
	}
	return FALSE;
}

static void
on_destroy(GtkWidget *w, gpointer data)
{
	(void)w;
	(void)data;
	gui_mod_mgr_bind_progress(NULL);
	gui_mod_mgr_set_current_mod(NULL);
	if (inited) {
		mod_mgr_shut_down();
		inited = FALSE;
	}
	clear_mods();
	g_free(dest_dir);
	dest_dir = NULL;
	dlg = NULL;
	combo_src = NULL;
	combo_lang = NULL;
	search_entry = NULL;
	chk_bibles = NULL;
	tree = NULL;
	store = NULL;
	progress = NULL;
	status_lbl = NULL;
	btn_refresh = NULL;
	btn_install = NULL;
	btn_advanced = NULL;
	btn_close = NULL;
	count_lbl = NULL;
	busy = FALSE;
}

static GtkWidget *
add_col(GtkTreeView *tv, const char *title, int col, int min_w, gboolean expand)
{
	GtkCellRenderer *r;
	GtkTreeViewColumn *c;

	r = gtk_cell_renderer_text_new();
	g_object_set(r, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	c = gtk_tree_view_column_new_with_attributes(title, r, "text", col, NULL);
	gtk_tree_view_column_set_resizable(c, TRUE);
	gtk_tree_view_column_set_min_width(c, min_w);
	gtk_tree_view_column_set_expand(c, expand);
	gtk_tree_view_append_column(tv, c);
	return (GtkWidget *)c;
}

static void
fill_sources(void)
{
	GList *have, *t;
	GPtrArray *caps;
	guint i;
	gboolean got_cw = FALSE;

	filling = TRUE;
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo_src));
	have = mod_mgr_list_remote_sources();
	caps = g_ptr_array_new();
	for (t = have; t; t = t->next) {
		MOD_MGR_SOURCE *s = (MOD_MGR_SOURCE *)t->data;
		if (s->caption && *s->caption)
			g_ptr_array_add(caps, (gpointer)s->caption);
	}

	/* Preferred order, then the rest. */
	for (i = 0; i < 9; i++) {
		guint j;
		for (j = 0; j < caps->len; j++) {
			const char *cap = g_ptr_array_index(caps, j);
			if (source_rank(cap) == (int)i) {
				gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_src),
							  cap, cap);
				if (i == 0)
					got_cw = TRUE;
				g_ptr_array_remove_index(caps, j);
				break;
			}
		}
	}
	for (i = 0; i < caps->len; i++) {
		const char *cap = g_ptr_array_index(caps, i);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_src),
					  cap, cap);
	}
	g_ptr_array_free(caps, TRUE);
	g_list_free_full(have, free_source);

	if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo_src), "CrossWire") &&
	    !got_cw)
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo_src), 0);
	filling = FALSE;
}

static GtkWidget *
build_dialog(void)
{
	GtkWidget *win, *hb, *outer, *row, *lbl, *scroller, *bbox;
	GtkWidget *hint;
	GtkCellRenderer *tog;
	GtkTreeViewColumn *chk_col;
	GtkStyleContext *ctx;

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(win), _("Instalar Biblias"));
	gtk_window_set_default_size(GTK_WINDOW(win), 820, 560);
	gui_prepare_floating_dialog(GTK_WINDOW(win),
				    widgets.app ? GTK_WINDOW(widgets.app) : NULL);

	hb = gtk_header_bar_new();
	gtk_header_bar_set_title(GTK_HEADER_BAR(hb), _("Instalar Biblias"));
	gtk_header_bar_set_subtitle(GTK_HEADER_BAR(hb),
				    _("Descargar e instalar desde fuentes de CrossWire"));
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(hb), TRUE);
	gtk_window_set_titlebar(GTK_WINDOW(win), hb);

	outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_set_border_width(GTK_CONTAINER(outer), 12);
	gtk_container_add(GTK_CONTAINER(win), outer);

	hint = gtk_label_new(_("Elija una fuente, filtre por idioma o nombre, marque las Biblias y pulse Instalar. Se descargan y quedan listas para leer y comparar."));
	gtk_label_set_line_wrap(GTK_LABEL(hint), TRUE);
	gtk_label_set_xalign(GTK_LABEL(hint), 0);
	gtk_widget_set_opacity(hint, 0.85);
	gtk_box_pack_start(GTK_BOX(outer), hint, FALSE, FALSE, 0);

	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	lbl = gtk_label_new(_("Fuente"));
	gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
	combo_src = gtk_combo_box_text_new();
	gtk_widget_set_hexpand(combo_src, TRUE);
	gtk_widget_set_tooltip_text(combo_src,
				    _("CrossWire es el catálogo principal. eBible.org tiene muchas lenguas vernáculas. HTTPS ayuda si el FTP está bloqueado."));
	btn_refresh = gtk_button_new_with_label(_("Actualizar catálogo"));
	gtk_button_set_image(GTK_BUTTON(btn_refresh),
			     gtk_image_new_from_icon_name("view-refresh-symbolic",
							  GTK_ICON_SIZE_BUTTON));
	gtk_button_set_always_show_image(GTK_BUTTON(btn_refresh), TRUE);
	gtk_widget_set_tooltip_text(btn_refresh,
				    _("Descargar la lista de módulos de esta fuente"));
	gtk_box_pack_start(GTK_BOX(row), lbl, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(row), combo_src, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(row), btn_refresh, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(outer), row, FALSE, FALSE, 0);

	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	lbl = gtk_label_new(_("Idioma"));
	gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
	combo_lang = gtk_combo_box_text_new();
	gtk_widget_set_size_request(combo_lang, 180, -1);
	search_entry = gtk_search_entry_new();
	gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry),
				       _("Buscar por nombre o módulo…"));
	gtk_widget_set_hexpand(search_entry, TRUE);
	chk_bibles = gtk_check_button_new_with_label(_("Solo Biblias"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_bibles), TRUE);
	gtk_box_pack_start(GTK_BOX(row), lbl, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(row), combo_lang, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(row), search_entry, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(row), chk_bibles, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(outer), row, FALSE, FALSE, 0);

	store = gtk_list_store_new(N_COLS,
				   G_TYPE_BOOLEAN,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_BOOLEAN);
	tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(tree), COL_DESC);
	gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(tree), COL_DESC);

	tog = gtk_cell_renderer_toggle_new();
	g_signal_connect(tog, "toggled", G_CALLBACK(on_toggle), NULL);
	chk_col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(chk_col, " ");
	gtk_tree_view_column_pack_start(chk_col, tog, FALSE);
	gtk_tree_view_column_add_attribute(chk_col, tog, "active", COL_CHECK);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), chk_col);

	add_col(GTK_TREE_VIEW(tree), _("Nombre"), COL_DESC, 220, TRUE);
	add_col(GTK_TREE_VIEW(tree), _("Módulo"), COL_NAME, 90, FALSE);
	add_col(GTK_TREE_VIEW(tree), _("Idioma"), COL_LANG, 90, FALSE);
	add_col(GTK_TREE_VIEW(tree), _("Tipo"), COL_TYPE, 90, FALSE);
	add_col(GTK_TREE_VIEW(tree), _("Tamaño"), COL_SIZE, 70, FALSE);
	add_col(GTK_TREE_VIEW(tree), _("Estado"), COL_STATUS, 90, FALSE);

	scroller = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroller),
					    GTK_SHADOW_IN);
	gtk_widget_set_vexpand(scroller, TRUE);
	gtk_container_add(GTK_CONTAINER(scroller), tree);
	gtk_box_pack_start(GTK_BOX(outer), scroller, TRUE, TRUE, 0);

	count_lbl = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(count_lbl), 0);
	gtk_widget_set_opacity(count_lbl, 0.75);
	gtk_box_pack_start(GTK_BOX(outer), count_lbl, FALSE, FALSE, 0);

	progress = gtk_progress_bar_new();
	gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progress), TRUE);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress), "");
	gtk_box_pack_start(GTK_BOX(outer), progress, FALSE, FALSE, 0);

	status_lbl = gtk_label_new("");
	gtk_label_set_xalign(GTK_LABEL(status_lbl), 0);
	gtk_label_set_line_wrap(GTK_LABEL(status_lbl), TRUE);
	gtk_box_pack_start(GTK_BOX(outer), status_lbl, FALSE, FALSE, 0);

	bbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	btn_advanced = gtk_button_new_with_label(_("Avanzado…"));
	gtk_widget_set_tooltip_text(btn_advanced,
				    _("Gestor completo de módulos (comentaristas, diccionarios, fuentes locales)"));
	btn_close = gtk_button_new_with_label(_("Cerrar"));
	btn_install = gtk_button_new_with_label(_("Instalar"));
	gtk_widget_set_sensitive(btn_install, FALSE);
	gtk_widget_set_tooltip_text(btn_install,
				    _("Descargar las Biblias marcadas e instalarlas en este equipo"));
	ctx = gtk_widget_get_style_context(btn_install);
	gtk_style_context_add_class(ctx, "suggested-action");
	gtk_box_pack_start(GTK_BOX(bbox), btn_advanced, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(bbox), btn_install, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(bbox), btn_close, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(outer), bbox, FALSE, FALSE, 0);

	g_signal_connect(combo_src, "changed", G_CALLBACK(on_src_changed), NULL);
	g_signal_connect(combo_lang, "changed", G_CALLBACK(on_filter_changed), NULL);
	g_signal_connect(search_entry, "search-changed",
			 G_CALLBACK(on_filter_changed), NULL);
	g_signal_connect(chk_bibles, "toggled", G_CALLBACK(on_filter_changed), NULL);
	g_signal_connect(btn_refresh, "clicked", G_CALLBACK(on_ib_refresh_clicked), NULL);
	g_signal_connect(btn_install, "clicked", G_CALLBACK(on_ib_install_clicked), NULL);
	g_signal_connect(btn_advanced, "clicked", G_CALLBACK(on_advanced_clicked), NULL);
	g_signal_connect(btn_close, "clicked", G_CALLBACK(on_close_clicked), NULL);
	g_signal_connect(win, "delete-event", G_CALLBACK(on_delete), NULL);
	g_signal_connect(win, "key-press-event", G_CALLBACK(on_key_press), NULL);
	g_signal_connect(win, "destroy", G_CALLBACK(on_destroy), NULL);

	return win;
}

void
gui_instalar_biblias(void)
{
	if (dlg) {
		gtk_window_present(GTK_WINDOW(dlg));
		return;
	}

	main_ensure_remote_sources();
	dest_dir = g_strdup_printf("%s/%s", settings.homedir, DOTSWORD);
	mod_mgr_init(dest_dir, TRUE, TRUE);
	inited = TRUE;

	dlg = build_dialog();
	gui_mod_mgr_bind_progress(progress);
	fill_sources();
	gtk_widget_show_all(dlg);
	load_catalog(FALSE);
}
