#ifndef BIBLIA_ELIM_EDITABLE_EDITION_DIALOG_H
#define BIBLIA_ELIM_EDITABLE_EDITION_DIALOG_H
#include <gtk/gtk.h>
G_BEGIN_DECLS
void gui_open_editable_edition_dialog(GtkWindow *parent);
void gui_show_edition_comparison(GtkWindow *parent, const char *module, int book, int chapter, int verse);
void gui_open_edition_editor(GtkWindow *parent, const char *module);
GtkWidget *gui_edition_contributions_button_new(gboolean compact);
void gui_edition_contributions_button_sync(const char *module);
void gui_open_edition_contributions(GtkWindow *parent, const char *module);
void gui_export_study_bible_pdf_path(GtkWindow *parent, const char *module_path);
G_END_DECLS
#endif
