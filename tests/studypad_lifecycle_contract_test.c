#include <glib.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                       \
	do {                                                                     \
		if (!(condition)) {                                                \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,    \
				#condition);                                        \
			failures++;                                               \
		}                                                                \
	} while (0)

static gchar *
read_source(const gchar *path)
{
	gchar *contents = NULL;
	GError *error = NULL;
	if (!g_file_get_contents(path, &contents, NULL, &error)) {
		fprintf(stderr, "Unable to read %s: %s\n", path,
			error ? error->message : "unknown error");
		g_clear_error(&error);
		failures++;
	}
	return contents;
}

static const gchar *
function_body(const gchar *source, const gchar *signature,
	     const gchar *next_function)
{
	const gchar *start = strstr(source, signature);
	const gchar *end = start ? strstr(start, next_function) : NULL;
	if (!start || !end || end <= start)
		return NULL;
	return g_strndup(start, end - start);
}

static gboolean
appears_before(const gchar *source, const gchar *first, const gchar *second)
{
	const gchar *a = strstr(source, first);
	const gchar *b = strstr(source, second);
	return a && b && a < b;
}

int
main(void)
{
	gchar *editor = read_source(SRCDIR "/src/editor/gtktextview_editor.c");
	gchar *menu = read_source(SRCDIR "/src/gtk/main_menu.c");
	gchar *window = read_source(SRCDIR "/src/gtk/main_window.c");
	gchar *save, *load, *close_all, *open, *new_doc, *embedded, *hide;
	if (!editor || !menu || !window)
		goto done;

	save = (gchar *)function_body(editor,
		"static gboolean\n_save_file(EDITOR *e, gboolean force_dialog)",
		"static void\nstudy_organize_dialog");
	CHECK(save != NULL);
	if (save) {
		CHECK(strstr(save, "if (!chosen)") != NULL);
		CHECK(appears_before(save, "if (!chosen)",
				     "e->is_changed = FALSE;"));
		CHECK(strstr(save, "if (!saved)") != NULL);
		CHECK(strstr(save, "return FALSE;") != NULL);
		CHECK(strstr(save, "e->is_changed = FALSE;") != NULL);
		CHECK(appears_before(save, "if (!saved) {",
				     "e->is_changed = FALSE;"));
		g_free(save);
	}

	load = (gchar *)function_body(editor,
		"static gboolean\n_load_file(EDITOR *e, const gchar *filename)",
		"static gboolean\n_save_file");
	CHECK(load != NULL);
	if (load) {
		CHECK(strstr(load, "if (error)") != NULL);
		CHECK(strstr(load, "return FALSE;") != NULL);
		CHECK(strstr(load, "_load_text_into_buffer") != NULL);
		CHECK(appears_before(load, "if (error)",
				     "_load_text_into_buffer"));
		g_free(load);
	}

	close_all = (gchar *)function_body(editor,
		"gboolean\neditor_maybe_save_all(void)",
		"static gint\n_create_new");
	CHECK(close_all != NULL);
	if (close_all) {
		CHECK(strstr(close_all, "response == GS_CANCEL") != NULL);
		CHECK(strstr(close_all, "return FALSE;") != NULL);
		CHECK(strstr(close_all, "while (editors_all)") != NULL);
		CHECK(appears_before(close_all, "response == GS_CANCEL",
				     "while (editors_all)"));
		g_free(close_all);
	}

	CHECK(strstr(menu,
	      "if (!editor_maybe_save_all())\n\t\treturn;") != NULL);
	open = (gchar *)function_body(editor,
		"action_open_activate_cb(GtkWidget *widget, EDITOR *e)",
		"action_save_activate_cb(GtkWidget *widget, EDITOR *e)");
	CHECK(open != NULL);
	if (open) {
		CHECK(strstr(open, "ask_about_saving(e) == GS_CANCEL") != NULL);
		g_free(open);
	}
	new_doc = (gchar *)function_body(editor,
		"action_new_activate_cb(GtkWidget *widget, EDITOR *e)",
		"action_open_activate_cb(GtkWidget *widget, EDITOR *e)");
	CHECK(new_doc != NULL);
	if (new_doc) {
		CHECK(strstr(new_doc, "ask_about_saving(e) == GS_CANCEL") != NULL);
		g_free(new_doc);
	}
	embedded = (gchar *)function_body(editor,
		"editor_create_embedded_studypad(GtkWindow *parent, GtkWidget **title_label_out)",
		"/* ============================================================\n * Public API");
	CHECK(embedded != NULL);
	if (embedded) {
		CHECK(strstr(embedded, "editor->embedded = TRUE") != NULL);
		CHECK(strstr(embedded, "editors_all = g_list_append(editors_all, editor)") != NULL);
		g_free(embedded);
	}
	hide = (gchar *)function_body(window,
		"study_editor_set_visible(gboolean visible, gboolean persist)",
		"static void\nstudy_editor_apply_standard_visibility");
	CHECK(hide != NULL);
	if (hide) {
		CHECK(strstr(hide, "gtk_widget_set_visible(study_editor_panel") != NULL);
		CHECK(strstr(hide, "editor_maybe_save_all") == NULL);
		g_free(hide);
	}

done:
	g_free(editor);
	g_free(menu);
	g_free(window);
	fprintf(stdout, "studypad_lifecycle_contract_failures=%d\n", failures);
	return failures ? 1 : 0;
}
