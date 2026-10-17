#ifndef ELIM_PREFERENCES_LAYOUT_H
#define ELIM_PREFERENCES_LAYOUT_H

#include <gtk/gtk.h>

/* Keep notebook indexes and builder IDs intact while allowing every settings
 * page to scroll independently when its content exceeds the window size. */
static inline void elim_preferences_layout(GtkWidget *dialog, GtkWidget *sidebar,
                                           GtkNotebook *book)
{
    gtk_widget_add_css_class(dialog, "elim-preferences");
    gtk_widget_add_css_class(sidebar, "elim-preferences-sidebar");
    gtk_widget_set_hexpand(GTK_WIDGET(book), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(book), TRUE);
    int count = gtk_notebook_get_n_pages(book);
    for (int i = 0; i < count; ++i) {
        GtkWidget *page = gtk_notebook_get_nth_page(book, i);
        if (GTK_IS_SCROLLED_WINDOW(page))
            continue;
        GtkWidget *tab = gtk_notebook_get_tab_label(book, page);
        g_object_ref(page);
        if (tab)
            g_object_ref(tab);
        gtk_notebook_remove_page(book, i);
        GtkWidget *scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                      GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_widget_add_css_class(page, "elim-preferences-page");
        gtk_widget_set_margin_start(page, 24);
        gtk_widget_set_margin_end(page, 24);
        gtk_widget_set_margin_top(page, 20);
        gtk_widget_set_margin_bottom(page, 24);
        gtk_widget_set_valign(page, GTK_ALIGN_START);
        if (GTK_IS_BOX(page))
            gtk_box_set_spacing(GTK_BOX(page), 12);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), page);
        gtk_notebook_insert_page(book, scroll, tab, i);
        if (tab)
            g_object_unref(tab);
        g_object_unref(page);
    }
}
#endif
