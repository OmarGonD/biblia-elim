#include <gtk/gtk.h>
#include "gui/widget_helpers.h"
#include "gui/dropdown_helpers.h"
#include "gui/table_helpers.h"
#include <gio/gio.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include "backend/sqlite_module_manager.h"
#include "backend/sword_bible_conversion.h"
#include "gui/sqlite_module_manager_dialog.h"
#include "main/mod_mgr.h"
#include "gui/widgets.h"
#include "gui/utilities.h"
#include "main/sword.h"
#include "main/settings.h"
#include "main/sidebar.h"
#include "gui/sidebar.h"
#include "gui/lectura_sync.h"
#include "gui/navbar_versekey.h"

static std::string module_directory() {
    const char *selected = main_sqlite_modules_directory();
    return selected && *selected ? selected : sqliteModuleDirectory();
}

struct DialogState { GListStore *store; GtkWidget *view; };
static const char *module_type_name(BibleModuleType t) {
    return t == BibleModuleType::Bible ? "Bible" : "";
}
static void show_error(const std::string &error) {
    GtkWidget *m = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
        GTK_BUTTONS_CLOSE, "%s", error.c_str()); gui_dialog_run(GTK_DIALOG(m)); gui_widget_destroy(m);
}
static void refresh_list(GListStore *store)
{
    g_list_store_remove_all(store);
    for (const auto &m : listSqliteModules(module_directory())) {
        ElimRow *row = elim_row_new(5);
        elim_row_set_string(row, 0, m.info.id.c_str());
        elim_row_set_string(row, 1, m.info.description.c_str());
        elim_row_set_string(row, 2, m.info.language.c_str());
        elim_row_set_string(row, 3, module_type_name(m.info.type));
        elim_row_set_string(row, 4,
            ((m.capabilities.strongs ? "Strong ✓  " : "") +
             std::string(m.capabilities.search ? "Search ✓" : "")).c_str());
        g_list_store_append(store, row);
        g_object_unref(row);
    }
}

static void on_remove(GtkButton *, gpointer data)
{
    DialogState *state = static_cast<DialogState *>(data);
    ElimRow *picked = elim_table_get_selected(state->view);
    if (!picked) return;
    gchar *id = g_strdup(elim_row_get_string(picked, 0));
    if (settings.MainWindowModule && !strcmp(settings.MainWindowModule, id)) {
        show_error("No se puede eliminar el módulo actualmente abierto."); g_free(id); return;
    }
    GtkWidget *q = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO, "¿Eliminar el módulo %s?", id);
    gboolean yes = gui_dialog_run(GTK_DIALOG(q)) == GTK_RESPONSE_YES; gui_widget_destroy(q);
    if (yes) { std::string error; if (removeSqliteModule(id, error, module_directory())) { main_recreate_bible_backend(); refresh_list(state->store); } else show_error(error); }
    g_free(id);
}

static bool import_options(GtkWindow *parent, UsfmImportOptions &o) {
    GtkWidget *d = gtk_dialog_new_with_buttons("Importar USFM", parent, GTK_DIALOG_MODAL,
        "Cancelar", GTK_RESPONSE_CANCEL, "Importar", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *grid = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(grid), 5); gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    const char *labels[] = {"ID del módulo", "Nombre", "Idioma", "Versificación"};
    GtkWidget *entries[4]; const char *defaults[] = {"rv1909", "RV1909", "es", "custom"};
    for (int i=0;i<4;++i) { gtk_grid_attach(GTK_GRID(grid), gtk_label_new(labels[i]), 0, i, 1, 1); entries[i]=gtk_entry_new(); gtk_editable_set_text(GTK_EDITABLE(entries[i]), defaults[i]); gtk_grid_attach(GTK_GRID(grid), entries[i], 1, i, 1, 1); }
    gui_box_pack(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(d))), grid, TRUE, TRUE, 8); gtk_widget_show(d);
    bool ok = gui_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT;
    if (ok) { o.moduleId=gtk_editable_get_text(GTK_EDITABLE(entries[0])); o.name=gtk_editable_get_text(GTK_EDITABLE(entries[1])); o.language=gtk_editable_get_text(GTK_EDITABLE(entries[2])); o.versification=gtk_editable_get_text(GTK_EDITABLE(entries[3])); }
    gui_widget_destroy(d); return ok;
}

static std::vector<std::string> usfm_files(const char *dir) {
    std::vector<std::string> out; GDir *d=g_dir_open(dir,0,NULL); if (!d) return out; const char *n;
    while ((n=g_dir_read_name(d))) { std::string s(n); if (s.size()>5 && s.substr(s.size()-5)==".usfm") out.emplace_back(std::string(dir)+"/"+s); }
    g_dir_close(d); std::sort(out.begin(), out.end()); return out;
}

struct ConversionJob {
    std::string id, directory, error;
    UsfmImportStats stats;
    std::vector<std::string> repaired;
    bool ok = false;
};

// Converts `ids` one after another behind a single modal progress dialog.
// Each conversion runs on a separate thread while the dialog processes GTK
// events. Returns the IDs converted; failures are reported in `failed`.
static std::vector<std::string> run_conversions(const std::vector<std::string> &ids,
                                                std::vector<std::pair<std::string, std::string>> &failed) {
    std::vector<std::string> converted;
    if (ids.empty()) return converted;
    GtkWidget *progress = gtk_dialog_new_with_buttons("Convertir Biblias a SQLite",
        widgets.app ? GTK_WINDOW(widgets.app) : NULL, GTK_DIALOG_MODAL, NULL);
    gtk_window_set_deletable(GTK_WINDOW(progress), FALSE);
    GtkWidget *label = gtk_label_new("");
    gui_box_pack(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(progress))), label, TRUE, TRUE, 16);
    gtk_widget_show(progress);
    for (size_t i = 0; i < ids.size(); ++i) {
        gchar *text = g_strdup_printf("Preparando %s para lectura rápida (SQLite)… %zu de %zu\n"
            "Se conservan el ID, las notas y los marcadores.\n"
            "El módulo SWORD original queda como respaldo.", ids[i].c_str(), i + 1, ids.size());
        gtk_label_set_text(GTK_LABEL(label), text);
        g_free(text);
        ConversionJob job{ids[i], module_directory(), {}, {}, {}, false};
        GTask *task = g_task_new(nullptr, nullptr, +[](GObject *, GAsyncResult *, gpointer dialog) {
            gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
        }, progress);
        g_task_set_task_data(task, &job, nullptr);
        g_task_run_in_thread(task, +[](GTask *task, gpointer, gpointer data, GCancellable *) {
            auto *job = static_cast<ConversionJob *>(data);
            job->ok = convertSwordBible(job->id, job->directory, job->stats, job->error, &job->repaired);
            g_task_return_boolean(task, job->ok);
        });
        // Modal GTK loop stays responsive; only the completion callback closes it.
        while (!g_task_get_completed(task)) gui_dialog_run(GTK_DIALOG(progress));
        g_object_unref(task);
        for (const auto &reference : job.repaired)
            g_message("SWORD -> SQLite %s: repaired truncated entry %s",
                      job.id.c_str(), reference.c_str());
        if (job.ok) converted.push_back(job.id);
        else failed.emplace_back(job.id, job.error);
    }
    gui_widget_destroy(progress);
    return converted;
}

// After a conversion the backend is recreated: a session that fell back to
// SWORD for lack of SQLite Bibles now reads them from SQLite.
static void reload_after_conversion(void) {
    main_update_module_lists();
    if (sidebar.module_list) main_load_module_tree(sidebar.module_list);
    gui_lectura_sync_rellenar_combo();
    gui_navbar_version_combo_refill();
    if (widgets.app && settings.MainWindowModule && settings.currentverse)
        main_display_bible(settings.MainWindowModule, settings.currentverse);
}

static bool run_conversion(const std::string &id) {
    std::vector<std::pair<std::string, std::string>> failed;
    if (run_conversions({id}, failed).empty()) {
        show_error(failed.empty() ? id : failed.front().second);
        return false;
    }
    reload_after_conversion();
    gui_set_statusbar((id + ": convertida a SQLite; ya se lee desde SQLite.").c_str());
    return true;
}

// Automatic conversion runs in the background, one child process per Bible
// (this same executable with --convert-sword): SWORD's managers share global
// state that is not thread-safe, so the conversion never runs beside the
// reader's SWORD in the same process. The reader keeps reading while a
// progress bar in the status bar follows it; completion is handled on the
// main thread.
struct BatchConversion {
    std::vector<std::string> ids;
    std::string directory;
    size_t current = 0;
    std::vector<std::string> converted;
    std::vector<std::pair<std::string, std::string>> failed;
    GSubprocess *child = nullptr;
};
static BatchConversion *active_batch = nullptr;
static bool batch_rerun = false;
static GtkWidget *batch_bar = nullptr;
static bool batch_statusbar_was_visible = true;

static std::string executable_path(void) {
    gchar *self = g_file_read_link("/proc/self/exe", nullptr);
    std::string path = self ? self : "";
    g_free(self);
    if (path.empty()) {
        const gchar *name = g_get_prgname();
        gchar *found = name ? g_find_program_in_path(name) : nullptr;
        path = found ? found : "";
        g_free(found);
    }
    return path;
}

static void show_batch_progress(size_t index, size_t total, const std::string &id) {
    if (!widgets.appbar) return;
    if (!batch_bar) {
        batch_bar = gtk_progress_bar_new();
        gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(batch_bar), TRUE);
        gtk_widget_set_size_request(batch_bar, 320, -1);
        gtk_widget_set_valign(batch_bar, GTK_ALIGN_CENTER);
        gtk_widget_set_tooltip_text(batch_bar,
            "Se conservan el ID, las notas y los marcadores; el módulo SWORD original "
            "queda como respaldo. Puede seguir leyendo mientras tanto.");
        /* at the right end of the status bar */
        gtk_widget_set_hexpand(batch_bar, TRUE);
        gtk_widget_set_halign(batch_bar, GTK_ALIGN_END);
        gui_box_pack(GTK_BOX(widgets.appbar), batch_bar, FALSE, FALSE, 6);
        batch_statusbar_was_visible = gtk_widget_get_visible(widgets.appbar);
        gtk_widget_show(widgets.appbar);
    }
    gchar *text = g_strdup_printf("Pasando a SQLite: %s (%zu de %zu)", id.c_str(), index + 1, total);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(batch_bar), text);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(batch_bar), total ? double(index) / total : 0);
    gtk_widget_show(batch_bar);
    g_free(text);
}

static void hide_batch_progress(void) {
    if (batch_bar) { gui_widget_destroy(batch_bar); batch_bar = nullptr; }
    if (widgets.appbar && !batch_statusbar_was_visible) gtk_widget_hide(widgets.appbar);
}

static void finish_batch(BatchConversion *batch) {
    active_batch = nullptr;
    hide_batch_progress();
    for (const auto &failure : batch->failed) {
        // Not retried at every startup; the SWORD original keeps serving it and
        // the SQLite manager can still convert it by hand.
        recordFailedSwordConversion(failure.first, batch->directory);
        g_message("SWORD -> SQLite conversion of %s failed: %s",
                  failure.first.c_str(), failure.second.c_str());
    }
    if (!batch->converted.empty()) reload_after_conversion();
    std::string status;
    if (!batch->converted.empty())
        status = std::to_string(batch->converted.size()) +
            (batch->converted.size() == 1 ? " Biblia convertida a SQLite." : " Biblias convertidas a SQLite.");
    for (const auto &failure : batch->failed)
        status += (status.empty() ? "" : " ") + failure.first + ": sin conversión, se lee con SWORD.";
    if (!status.empty()) gui_set_statusbar(status.c_str());
    delete batch;
    if (batch_rerun) { batch_rerun = false; gui_convert_pending_sword_bibles(); }
}

// Child output: "REPAIRED <ref>" lines, then "ERROR <message>" on failure.
static void record_child_result(BatchConversion *batch, bool ok, const char *output) {
    const std::string &id = batch->ids[batch->current];
    std::string error = "la conversión terminó de forma inesperada";
    gchar **lines = g_strsplit(output ? output : "", "\n", -1);
    for (gchar **line = lines; *line; ++line) {
        if (g_str_has_prefix(*line, "REPAIRED "))
            g_message("SWORD -> SQLite %s: repaired truncated entry %s", id.c_str(), *line + 9);
        else if (g_str_has_prefix(*line, "ERROR "))
            error = *line + 6;
    }
    g_strfreev(lines);
    if (ok) batch->converted.push_back(id);
    else batch->failed.emplace_back(id, error);
}

static void start_next_conversion(BatchConversion *batch);

static void on_child_done(GObject *source, GAsyncResult *result, gpointer data) {
    auto *batch = static_cast<BatchConversion *>(data);
    gchar *output = nullptr;
    GError *error = nullptr;
    const bool finished = g_subprocess_communicate_utf8_finish(G_SUBPROCESS(source), result,
                                                              &output, nullptr, &error);
    const bool ok = finished && g_subprocess_get_if_exited(G_SUBPROCESS(source)) &&
                    g_subprocess_get_exit_status(G_SUBPROCESS(source)) == 0;
    record_child_result(batch, ok, output);
    if (error) g_error_free(error);
    g_free(output);
    g_clear_object(&batch->child);
    ++batch->current;
    start_next_conversion(batch);
}

static void start_next_conversion(BatchConversion *batch) {
    if (batch->current >= batch->ids.size()) { finish_batch(batch); return; }
    const std::string &id = batch->ids[batch->current];
    show_batch_progress(batch->current, batch->ids.size(), id);
    const std::string self = executable_path();
    GError *error = nullptr;
    batch->child = self.empty() ? nullptr :
        g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, self.c_str(),
                         "--convert-sword", id.c_str(), batch->directory.c_str(), NULL);
    if (!batch->child) {
        batch->failed.emplace_back(id, error ? error->message : "no se encontró el ejecutable");
        if (error) g_error_free(error);
        ++batch->current;
        start_next_conversion(batch);
        return;
    }
    g_subprocess_communicate_utf8_async(batch->child, nullptr, nullptr, on_child_done, batch);
}

extern "C" void gui_convert_pending_sword_bibles(void) {
    if (!main_sqlite_backend_preferred()) return;
    // A Bible installed while a batch runs is picked up when it ends.
    if (active_batch) { batch_rerun = true; return; }
    const auto pending = pendingSwordBibleConversions(convertibleSwordBibles(), module_directory());
    if (pending.empty()) return;
    auto *batch = new BatchConversion;
    batch->directory = module_directory();
    for (const auto &candidate : pending) batch->ids.push_back(candidate.id);
    active_batch = batch;
    start_next_conversion(batch);
}

extern "C" void gui_stop_sword_conversion(void) {
    // The Bible in progress finishes (its installation is atomic, and an
    // interrupted one would leave temporary files); the rest are converted
    // at the next start. Its pending callback never runs: the app exits.
    if (active_batch && active_batch->child)
        g_subprocess_wait(active_batch->child, nullptr, nullptr);
}

extern "C" void gui_forget_converted_sword_bible(const char *module_id) {
    if (!module_id || !*module_id) return;
    std::string error;
    if (!removeConvertedSwordBible(module_id, module_directory(), error))
        g_message("SQLite copy of %s not removed: %s", module_id, error.c_str());
}

extern "C" int sword_conversion_child_main(const char *id, const char *directory) {
    UsfmImportStats stats;
    std::string error;
    std::vector<std::string> repaired;
    const bool ok = convertSwordBible(id, directory, stats, error, &repaired);
    for (const auto &reference : repaired) std::printf("REPAIRED %s\n", reference.c_str());
    if (!ok) std::printf("ERROR %s\n", error.c_str());
    return ok ? 0 : 1;
}

extern "C" void gui_offer_sword_conversion(const char *module_id) {
    if (active_batch) {
        show_error("Ya se están pasando Biblias a SQLite; espere a que termine la barra de progreso.");
        return;
    }
    const auto installed = listSqliteModules(module_directory());
    auto candidates = convertibleSwordBibles();
    candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&](const SwordBibleConversionInfo &candidate) {
        return (module_id && *module_id && candidate.id != module_id) ||
            std::any_of(installed.begin(), installed.end(), [&](const SqliteManagedModule &m) { return m.info.id == candidate.id; });
    }), candidates.end());
    if (candidates.empty()) return;
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Convertir Biblias SWORD a SQLite",
        GTK_WINDOW(widgets.app), GTK_DIALOG_MODAL, "Ahora no", GTK_RESPONSE_CANCEL,
        "Convertir", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *combo = elim_dropdown_new();
    for (const auto &m : candidates)
        elim_dropdown_append(GTK_DROP_DOWN(combo), m.id.c_str(),
            (m.id + " — " + m.language + " · " + m.versification).c_str());
    elim_dropdown_set_active(GTK_DROP_DOWN(combo), 0);
    GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gui_box_pack(GTK_BOX(area), gtk_label_new("Copia SQLite de una Biblia instalada. Se conserva su identificador.\nEl original SWORD permanece disponible como respaldo."), FALSE, FALSE, 12);
    gui_box_pack(GTK_BOX(area), combo, FALSE, FALSE, 8);
    gtk_widget_show(dialog);
    if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const std::string id = elim_dropdown_get_active_id(GTK_DROP_DOWN(combo));
        gui_widget_destroy(dialog);
        run_conversion(id);
    } else gui_widget_destroy(dialog);
}

extern "C" void gui_open_sqlite_module_manager(void)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Módulos SQLite",
        NULL, GTK_DIALOG_MODAL, "Cerrar", GTK_RESPONSE_CLOSE, NULL);
    GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GListStore *store = elim_table_new();
    GtkWidget *view = elim_table_view_new(store);
    const char *titles[] = { "ID", "Nombre", "Idioma", "Tipo", "Capacidades" };
    for (int i = 0; i < 5; ++i)
        elim_table_add_text_column(view, titles[i], i, i == 1);
    GtkWidget *scroller = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), view);
    gtk_widget_set_vexpand(scroller, TRUE);
    gui_box_pack(GTK_BOX(area), scroller, TRUE, TRUE, 4);
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *install = gtk_button_new_with_label("Instalar SQLite");
    GtkWidget *import = gtk_button_new_with_label("Importar USFM");
    GtkWidget *remove = gtk_button_new_with_label("Eliminar");
    GtkWidget *convert = gtk_button_new_with_label("Convertir Biblia SWORD…");
    gtk_box_append(GTK_BOX(buttons), convert);
    g_signal_connect_swapped(convert, "clicked", G_CALLBACK(+[](GListStore *s) {
        gui_offer_sword_conversion(nullptr); refresh_list(s);
    }), store);
    gtk_box_append(GTK_BOX(buttons), install);
    gtk_box_append(GTK_BOX(buttons), import);
    gtk_box_append(GTK_BOX(buttons), remove);
    gui_box_pack(GTK_BOX(area), buttons, FALSE, FALSE, 4);
    refresh_list(store);
    DialogState *state = new DialogState{store, view};
    g_signal_connect_swapped(install, "clicked", G_CALLBACK(+[](GListStore *s) {
        GtkWidget *chooser = gtk_file_chooser_dialog_new("Seleccionar módulo SQLite", NULL,
            GTK_FILE_CHOOSER_ACTION_OPEN, "Cancelar", GTK_RESPONSE_CANCEL,
            "Instalar", GTK_RESPONSE_ACCEPT, NULL);
        if (gui_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
            char *path = gui_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
            std::string error;
            if (!installSqliteModule(path, error, nullptr, module_directory())) {
                GtkWidget *m = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                    GTK_BUTTONS_CLOSE, "%s", error.c_str()); gui_dialog_run(GTK_DIALOG(m)); gui_widget_destroy(m);
            } else { main_recreate_bible_backend(); refresh_list(s); }
            g_free(path);
        }
        gui_widget_destroy(chooser);
    }), store);
    g_signal_connect(remove, "clicked", G_CALLBACK(on_remove), state);
    g_signal_connect_swapped(import, "clicked", G_CALLBACK(+[](GListStore *s) {
        GtkWidget *c=gtk_file_chooser_dialog_new("Seleccionar directorio USFM", NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
            "Cancelar", GTK_RESPONSE_CANCEL, "Seleccionar", GTK_RESPONSE_ACCEPT, NULL);
        if (gui_dialog_run(GTK_DIALOG(c))==GTK_RESPONSE_ACCEPT) { char *dir=gui_file_chooser_get_filename(GTK_FILE_CHOOSER(c)); UsfmImportOptions o; if (import_options(GTK_WINDOW(c),o)) { auto files=usfm_files(dir); UsfmImportStats st; std::string e; if (files.empty() || !importUsfmModule(files,o,st,e,module_directory())) show_error(files.empty()?"No se encontraron archivos USFM.":e); else { main_recreate_bible_backend(); refresh_list(s); } } g_free(dir); }
        gui_widget_destroy(c);
    }), store);
    gtk_widget_set_size_request(dialog, 560, 360);
    gtk_widget_show(dialog); gui_dialog_run(GTK_DIALOG(dialog));
    gui_widget_destroy(dialog); delete state; g_object_unref(store);
}
