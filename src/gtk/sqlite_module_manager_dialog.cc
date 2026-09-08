#include <gtk/gtk.h>
#include <cstring>
#include <algorithm>
#include <vector>
#include "backend/sqlite_module_manager.h"
#include "main/sword.h"
#include "main/settings.h"

struct DialogState { GtkListStore *store; GtkTreeView *view; };
static const char *module_type_name(BibleModuleType t) {
    return t == BibleModuleType::Bible ? "Bible" : "";
}
static void show_error(const std::string &error) {
    GtkWidget *m = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
        GTK_BUTTONS_CLOSE, "%s", error.c_str()); gtk_dialog_run(GTK_DIALOG(m)); gtk_widget_destroy(m);
}
static void refresh_list(GtkListStore *store)
{
    gtk_list_store_clear(store);
    for (const auto &m : listSqliteModules()) {
        GtkTreeIter it; gtk_list_store_append(store, &it);
        gtk_list_store_set(store, &it, 0, m.info.id.c_str(), 1,
                           m.info.description.c_str(), 2,
                           m.info.language.c_str(), 3,
                           module_type_name(m.info.type), 4,
                           (m.capabilities.strongs ? "Strong ✓  " : "") +
                           std::string(m.capabilities.search ? "Search ✓" : ""), -1);
    }
}

static void on_remove(GtkButton *, gpointer data)
{
    DialogState *state = static_cast<DialogState *>(data);
    GtkTreeSelection *sel = gtk_tree_view_get_selection(state->view);
    GtkTreeModel *model; GtkTreeIter it;
    if (!gtk_tree_selection_get_selected(sel, &model, &it)) return;
    gchar *id = nullptr; gtk_tree_model_get(model, &it, 0, &id, -1);
    if (settings.MainWindowModule && !strcmp(settings.MainWindowModule, id)) {
        show_error("No se puede eliminar el módulo actualmente abierto."); g_free(id); return;
    }
    GtkWidget *q = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO, "¿Eliminar el módulo %s?", id);
    gboolean yes = gtk_dialog_run(GTK_DIALOG(q)) == GTK_RESPONSE_YES; gtk_widget_destroy(q);
    if (yes) { std::string error; if (removeSqliteModule(id, error)) { main_recreate_bible_backend(); refresh_list(state->store); } else show_error(error); }
    g_free(id);
}

static bool import_options(GtkWindow *parent, UsfmImportOptions &o) {
    GtkWidget *d = gtk_dialog_new_with_buttons("Importar USFM", parent, GTK_DIALOG_MODAL,
        "Cancelar", GTK_RESPONSE_CANCEL, "Importar", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *grid = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid), 5); gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    const char *labels[] = {"ID del módulo", "Nombre", "Idioma", "Versificación"};
    GtkWidget *entries[4]; const char *defaults[] = {"rv1909", "RV1909", "es", "custom"};
    for (int i=0;i<4;++i) { gtk_grid_attach(GTK_GRID(grid), gtk_label_new(labels[i]), 0, i, 1, 1); entries[i]=gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(entries[i]), defaults[i]); gtk_grid_attach(GTK_GRID(grid), entries[i], 1, i, 1, 1); }
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(d))), grid, TRUE, TRUE, 8); gtk_widget_show_all(d);
    bool ok = gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT;
    if (ok) { o.moduleId=gtk_entry_get_text(GTK_ENTRY(entries[0])); o.name=gtk_entry_get_text(GTK_ENTRY(entries[1])); o.language=gtk_entry_get_text(GTK_ENTRY(entries[2])); o.versification=gtk_entry_get_text(GTK_ENTRY(entries[3])); }
    gtk_widget_destroy(d); return ok;
}

static std::vector<std::string> usfm_files(const char *dir) {
    std::vector<std::string> out; GDir *d=g_dir_open(dir,0,NULL); if (!d) return out; const char *n;
    while ((n=g_dir_read_name(d))) { std::string s(n); if (s.size()>5 && s.substr(s.size()-5)==".usfm") out.emplace_back(std::string(dir)+"/"+s); }
    g_dir_close(d); std::sort(out.begin(), out.end()); return out;
}

extern "C" void gui_open_sqlite_module_manager(void)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Módulos SQLite",
        NULL, GTK_DIALOG_MODAL, "Cerrar", GTK_RESPONSE_CLOSE, NULL);
    GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkListStore *store = gtk_list_store_new(5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget *view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    const char *titles[] = { "ID", "Nombre", "Idioma", "Tipo", "Capacidades" };
    for (int i = 0; i < 5; ++i) {
        GtkCellRenderer *r = gtk_cell_renderer_text_new();
        gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view), -1,
            titles[i], r, "text", i, NULL);
    }
    gtk_box_pack_start(GTK_BOX(area), view, TRUE, TRUE, 4);
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *install = gtk_button_new_with_label("Instalar SQLite");
    GtkWidget *import = gtk_button_new_with_label("Importar USFM");
    GtkWidget *remove = gtk_button_new_with_label("Eliminar");
    gtk_box_pack_start(GTK_BOX(buttons), install, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttons), import, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttons), remove, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(area), buttons, FALSE, FALSE, 4);
    refresh_list(store);
    DialogState *state = new DialogState{store, GTK_TREE_VIEW(view)};
    g_signal_connect_swapped(install, "clicked", G_CALLBACK(+[](GtkListStore *s) {
        GtkWidget *chooser = gtk_file_chooser_dialog_new("Seleccionar módulo SQLite", NULL,
            GTK_FILE_CHOOSER_ACTION_OPEN, "Cancelar", GTK_RESPONSE_CANCEL,
            "Instalar", GTK_RESPONSE_ACCEPT, NULL);
        if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
            char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
            std::string error;
            if (!installSqliteModule(path, error)) {
                GtkWidget *m = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_CLOSE, "%s", error.c_str()); gtk_dialog_run(GTK_DIALOG(m)); gtk_widget_destroy(m);
            } else { main_recreate_bible_backend(); refresh_list(s); }
            g_free(path);
        }
        gtk_widget_destroy(chooser);
    }), store);
    g_signal_connect(remove, "clicked", G_CALLBACK(on_remove), state);
    g_signal_connect_swapped(import, "clicked", G_CALLBACK(+[](GtkListStore *s) {
        GtkWidget *c=gtk_file_chooser_dialog_new("Seleccionar directorio USFM", NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
            "Cancelar", GTK_RESPONSE_CANCEL, "Seleccionar", GTK_RESPONSE_ACCEPT, NULL);
        if (gtk_dialog_run(GTK_DIALOG(c))==GTK_RESPONSE_ACCEPT) { char *dir=gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(c)); UsfmImportOptions o; if (import_options(GTK_WINDOW(c),o)) { auto files=usfm_files(dir); UsfmImportStats st; std::string e; if (files.empty() || !importUsfmModule(files,o,st,e)) show_error(files.empty()?"No se encontraron archivos USFM.":e); else { main_recreate_bible_backend(); refresh_list(s); } } g_free(dir); }
        gtk_widget_destroy(c);
    }), store);
    gtk_widget_set_size_request(dialog, 560, 360);
    gtk_widget_show_all(dialog); gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog); delete state; g_object_unref(store);
}
