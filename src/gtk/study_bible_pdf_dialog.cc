#include "gui/editable_edition_dialog.h"
#include "gtk/study_bible_pdf.h"
#include "backend/sqlite_module_manager.h"
#include "gui/utilities.h"
#include "gui/widget_helpers.h"

#include <glib/gi18n.h>
#include <algorithm>
#include <string>

namespace {
struct ExportJob {
    std::string source,output,error;
    StudyBiblePdfOptions options;
    StudyBiblePdfStats stats;
};
struct Progress {
    GtkWidget *status,*spinner,*cancel;
    GCancellable *cancellable=g_cancellable_new();
    std::string output;
    bool finished=false;
    ~Progress(){g_object_unref(cancellable);}
};

void generatePdf(GtkWindow *parent,ExportJob *job)
{
    GtkWidget *dialog=gtk_dialog_new_with_buttons(_("Exportar Biblia de estudio"),parent,
        GTK_DIALOG_MODAL,_("Cancelar"),GTK_RESPONSE_CANCEL,nullptr);
    gtk_widget_set_name(dialog,"study-pdf-progress");
    gtk_window_set_default_size(GTK_WINDOW(dialog),500,-1);
    GtkWidget *box=gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(box),14);
    for(auto set:{gtk_widget_set_margin_start,gtk_widget_set_margin_end,gtk_widget_set_margin_top,gtk_widget_set_margin_bottom})set(box,22);
    auto *state=new Progress;
    state->status=gtk_label_new(_("Generando el PDF… Puedes cancelar la exportación."));
    gtk_widget_set_name(state->status,"study-pdf-status");
    gtk_label_set_wrap(GTK_LABEL(state->status),TRUE);gtk_label_set_max_width_chars(GTK_LABEL(state->status),60);
    state->spinner=gtk_spinner_new();gtk_spinner_start(GTK_SPINNER(state->spinner));
    state->cancel=gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);
    state->output=job->output;
    gtk_box_append(GTK_BOX(box),state->spinner);gtk_box_append(GTK_BOX(box),state->status);
    g_object_set_data_full(G_OBJECT(dialog),"pdf-export-progress",state,+[](gpointer p){delete static_cast<Progress*>(p);});
    g_signal_connect(dialog,"response",G_CALLBACK(+[](GtkDialog *dialog,int response,gpointer data){
        auto *state=static_cast<Progress*>(data);
        if(!state->finished){
            g_cancellable_cancel(state->cancellable);
            gtk_label_set_text(GTK_LABEL(state->status),_("Cancelando la exportación…"));
            gtk_widget_set_sensitive(state->cancel,FALSE);return;
        }
        if(response==GTK_RESPONSE_ACCEPT){
            gchar *uri=g_filename_to_uri(state->output.c_str(),nullptr,nullptr);
            if(uri){gtk_show_uri(GTK_WINDOW(dialog),uri,GDK_CURRENT_TIME);g_free(uri);}
        }
        gui_widget_destroy(GTK_WIDGET(dialog));
    }),state);
    GTask *task=g_task_new(dialog,state->cancellable,+[](GObject *object,GAsyncResult *result,gpointer){
        auto *state=static_cast<Progress*>(g_object_get_data(object,"pdf-export-progress"));
        auto *job=static_cast<ExportJob*>(g_task_get_task_data(G_TASK(result)));
        GError *failure=nullptr;const bool success=g_task_propagate_boolean(G_TASK(result),&failure);
        state->finished=true;gtk_spinner_stop(GTK_SPINNER(state->spinner));gtk_widget_set_visible(state->spinner,FALSE);
        gtk_widget_set_sensitive(state->cancel,TRUE);gtk_button_set_label(GTK_BUTTON(state->cancel),_("Cerrar"));
        if(success){
            gchar *message=g_strdup_printf(_("PDF guardado: %d páginas · %d versículos · %d aportes\n%s"),
                job->stats.pages,job->stats.verses,job->stats.contributions,job->output.c_str());
            gtk_label_set_text(GTK_LABEL(state->status),message);g_free(message);
            GtkWidget *open=gtk_dialog_add_button(GTK_DIALOG(object),_("Abrir PDF"),GTK_RESPONSE_ACCEPT);
            gtk_widget_set_margin_start(open,12);gtk_widget_add_css_class(open,"suggested-action");
        }else gtk_label_set_text(GTK_LABEL(state->status),failure?failure->message:_("No se pudo exportar el PDF."));
        g_clear_error(&failure);
    },nullptr);
    // The worker reports cancellation only after releasing the temporary PDF.
    g_task_set_check_cancellable(task,FALSE);
    g_task_set_task_data(task,job,+[](gpointer p){delete static_cast<ExportJob*>(p);});
    g_task_run_in_thread(task,+[](GTask *task,gpointer,gpointer data,GCancellable *cancel){
        auto *job=static_cast<ExportJob*>(data);
        job->options.cancelled=[cancel]{return g_cancellable_is_cancelled(cancel);};
        if(exportStudyBiblePdf(job->source,job->output,job->options,job->stats,job->error))g_task_return_boolean(task,TRUE);
        else g_task_return_new_error(task,G_IO_ERROR,g_cancellable_is_cancelled(cancel)?G_IO_ERROR_CANCELLED:G_IO_ERROR_FAILED,"%s",job->error.c_str());
    });
    g_object_unref(task);gtk_widget_show(dialog);
}
}

extern "C" void gui_export_study_bible_pdf_path(GtkWindow *parent,const char *path)
{
    if(!path || !*path)return;
    const std::string source=path;
    const std::string name=sqliteModuleMetadata(source,"name");
    GtkWidget *dialog=gtk_dialog_new_with_buttons(_("Exportar Biblia de estudio a PDF"),parent,
        GTK_DIALOG_MODAL,_("Cancelar"),GTK_RESPONSE_CANCEL,_("Elegir archivo…"),GTK_RESPONSE_ACCEPT,nullptr);
    gtk_widget_set_name(dialog,"study-pdf-options");gtk_window_set_default_size(GTK_WINDOW(dialog),540,-1);
    GtkWidget *accept=gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog),GTK_RESPONSE_ACCEPT);
    gtk_widget_set_margin_start(accept,12);gtk_widget_add_css_class(accept,"suggested-action");
    GtkWidget *box=gtk_dialog_get_content_area(GTK_DIALOG(dialog));gtk_box_set_spacing(GTK_BOX(box),14);
    for(auto set:{gtk_widget_set_margin_start,gtk_widget_set_margin_end,gtk_widget_set_margin_top,gtk_widget_set_margin_bottom})set(box,22);
    GtkWidget *title=gtk_label_new(name.c_str());gtk_widget_add_css_class(title,"title-2");
    gtk_label_set_xalign(GTK_LABEL(title),0);gtk_label_set_wrap(GTK_LABEL(title),TRUE);gtk_box_append(GTK_BOX(box),title);
    const char *choices[]={_("Biblia completa con todos sus aportes"),_("Solo versículos con aportes"),nullptr};
    GtkWidget *scope=gtk_drop_down_new_from_strings(choices);gtk_widget_set_name(scope,"study-pdf-scope");
    gtk_box_append(GTK_BOX(box),scope);
    GtkWidget *hint=gtk_label_new(_("El PDF incluye índices y marcadores. Pulsa «Aporte» junto al versículo para leer el comentario y sus fuentes; «Volver al versículo» te devuelve al pasaje. Se exportan todos los aportes guardados, sin aplicar los filtros de la lista."));
    gtk_label_set_wrap(GTK_LABEL(hint),TRUE);gtk_label_set_max_width_chars(GTK_LABEL(hint),64);gtk_label_set_xalign(GTK_LABEL(hint),0);
    gtk_box_append(GTK_BOX(box),hint);gtk_widget_show(dialog);
    const int response=gui_dialog_run(GTK_DIALOG(dialog));
    const bool only=gtk_drop_down_get_selected(GTK_DROP_DOWN(scope))==1;gui_widget_destroy(dialog);
    if(response!=GTK_RESPONSE_ACCEPT)return;
    GtkWidget *chooser=gtk_file_chooser_dialog_new(_("Guardar PDF"),parent,GTK_FILE_CHOOSER_ACTION_SAVE,
        _("Cancelar"),GTK_RESPONSE_CANCEL,_("Exportar PDF"),GTK_RESPONSE_ACCEPT,nullptr);
    gtk_widget_set_name(chooser,"study-pdf-file");
    gtk_window_set_modal(GTK_WINDOW(chooser),TRUE);
    gtk_widget_set_margin_start(gtk_dialog_get_widget_for_response(GTK_DIALOG(chooser),GTK_RESPONSE_ACCEPT),12);
    GtkFileFilter *filter=gtk_file_filter_new();gtk_file_filter_set_name(filter,_("Documento PDF"));
    gtk_file_filter_add_pattern(filter,"*.pdf");gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser),filter);g_object_unref(filter);
    std::string filename=name;
    for(char &c:filename)if(static_cast<unsigned char>(c)<32 || std::string("<>:\"/\\|?*").find(c)!=std::string::npos)c='-';
    if(filename.empty())filename="Biblia de estudio";
    filename+=only?" - aportes.pdf":".pdf";
    const char *documents=g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS);
    gui_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser),
        documents && g_file_test(documents,G_FILE_TEST_IS_DIR)?documents:g_get_home_dir());
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser),filename.c_str());
    gtk_widget_show(chooser);std::string output;
    if(gui_dialog_run(GTK_DIALOG(chooser))==GTK_RESPONSE_ACCEPT){
        gchar *selected=gui_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));if(selected)output=selected;g_free(selected);
    }
    gui_widget_destroy(chooser);if(output.empty())return;
    gchar *lower=g_ascii_strdown(output.c_str(),-1);const bool pdf=g_str_has_suffix(lower,".pdf");g_free(lower);
    if(!pdf){
        output+=".pdf";
        if(g_file_test(output.c_str(),G_FILE_TEST_EXISTS)){
            GtkWidget *replace=gtk_message_dialog_new(parent,GTK_DIALOG_MODAL,GTK_MESSAGE_QUESTION,
                GTK_BUTTONS_YES_NO,"%s",_("El archivo PDF ya existe. ¿Quieres reemplazarlo?"));
            gtk_widget_show(replace);const int answer=gui_dialog_run(GTK_DIALOG(replace));gui_widget_destroy(replace);
            if(answer!=GTK_RESPONSE_YES)return;
        }
    }
    auto *job=new ExportJob;job->source=source;job->output=output;job->options.contributionsOnly=only;
    generatePdf(parent,job);
}
