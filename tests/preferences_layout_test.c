#include <gtk/gtk.h>
#include "gui/preferences_layout.h"

static void css_error(GtkCssProvider *provider, GtkCssSection *section,
                      const GError *error, gpointer data)
{
    (void)provider; (void)section;
    (*(int *)data)++;
    g_printerr("CSS: %s\n", error->message);
}

int main(int argc, char **argv)
{
    g_assert_cmpint(argc, ==, 2);
    /* CSS parsing needs no display, so it runs even on headless builders. */
    GtkCssProvider *css = gtk_css_provider_new();
    int errors = 0;
    g_signal_connect(css, "parsing-error", G_CALLBACK(css_error), &errors);
    gtk_css_provider_load_from_path(css, argv[1]);
    g_assert_cmpint(errors, ==, 0);
    g_object_unref(css);
    if (!gtk_init_check()) {
        g_print("CSS PASS; preferences widget checks require a display\n");
        return 77;
    }
    GtkWidget *dialog = gtk_window_new();
    GtkWidget *sidebar = gtk_list_view_new(NULL, NULL);
    g_object_ref_sink(sidebar);
    GtkNotebook *book = GTK_NOTEBOOK(gtk_notebook_new());
    gtk_window_set_child(GTK_WINDOW(dialog), GTK_WIDGET(book));
    GtkWidget *pages[3];
    GtkWidget *tabs[3];
    for (int i = 0; i < 3; ++i) {
        pages[i] = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        tabs[i] = gtk_label_new("Settings");
        gtk_box_append(GTK_BOX(pages[i]), gtk_check_button_new_with_label("Option"));
        gtk_notebook_append_page(book, pages[i], tabs[i]);
    }
    elim_preferences_layout(dialog, sidebar, book);
    elim_preferences_layout(dialog, sidebar, book); /* no double wrapping */
    g_assert_cmpint(gtk_notebook_get_n_pages(book), ==, 3);
    for (int i = 0; i < 3; ++i) {
        GtkWidget *scroll = gtk_notebook_get_nth_page(book, i);
        g_assert_true(GTK_IS_SCROLLED_WINDOW(scroll));
        GtkWidget *viewport = gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(scroll));
        g_assert_true(gtk_viewport_get_child(GTK_VIEWPORT(viewport)) == pages[i]);
        g_assert_true(gtk_notebook_get_tab_label(book, scroll) == tabs[i]);
        g_assert_cmpint(gtk_widget_get_margin_start(pages[i]), ==, 24);
        g_assert_cmpint(gtk_widget_get_margin_bottom(pages[i]), ==, 24);
        g_assert_true(gtk_widget_has_css_class(pages[i], "elim-preferences-page"));
        gtk_notebook_set_current_page(book, i);
        g_assert_cmpint(gtk_notebook_get_current_page(book), ==, i);
    }
    g_assert_true(gtk_widget_has_css_class(sidebar, "elim-preferences-sidebar"));
    gtk_window_destroy(GTK_WINDOW(dialog));
    g_object_unref(sidebar);
    g_print("preferences_layout_test: PASS\n");
    return 0;
}
