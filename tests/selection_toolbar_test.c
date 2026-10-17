#include "gui/selection_toolbar.h"

static guint closes;
static void closed(GtkPopover *popover, gpointer data)
{
    (void)popover;
    (void)data;
    closes++;
}

int main(void)
{
    if (!gtk_init_check()) return 77;
    GtkWidget *window = gtk_window_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *outside = gtk_button_new_with_label("Reader / another panel");
    gtk_window_set_child(GTK_WINDOW(window), box);
    gtk_box_append(GTK_BOX(box), outside);
    GtkWidget *toolbar = gui_popover_new(box);
    GtkWidget *action = gtk_button_new_with_label("Underline");
    gtk_popover_set_child(GTK_POPOVER(toolbar), action);
    selection_toolbar_install(toolbar);
    g_signal_connect(toolbar, "closed", G_CALLBACK(closed), NULL);
    gtk_window_present(GTK_WINDOW(window));
    while (g_main_context_pending(NULL)) g_main_context_iteration(NULL, FALSE);
    g_assert_false(gtk_popover_get_autohide(GTK_POPOVER(toolbar)));

    gtk_popover_popup(GTK_POPOVER(toolbar));
    GuiButtonEvent click = {.target = action};
    g_assert_false(selection_toolbar_outside_press(window, &click, toolbar));
    g_assert_true(gtk_widget_get_visible(toolbar));
    g_assert_cmpuint(closes, ==, 0);
    click.target = outside;
    g_assert_false(selection_toolbar_outside_press(window, &click, toolbar));
    g_assert_false(gtk_widget_get_visible(toolbar));
    g_assert_cmpuint(closes, ==, 1);

    gtk_popover_popup(GTK_POPOVER(toolbar));
    GuiKeyEvent key = {.keyval = GDK_KEY_a};
    g_assert_false(selection_toolbar_escape(window, &key, toolbar));
    g_assert_true(gtk_widget_get_visible(toolbar));
    key.keyval = GDK_KEY_Escape;
    g_assert_true(selection_toolbar_escape(window, &key, toolbar));
    g_assert_false(gtk_widget_get_visible(toolbar));
    g_assert_cmpuint(closes, ==, 2);
    g_assert_false(selection_toolbar_escape(window, &key, toolbar));

    /* A subsequent selection can offer the same toolbar again. */
    gtk_popover_popup(GTK_POPOVER(toolbar));
    g_assert_true(gtk_widget_get_visible(toolbar));
    click.target = NULL;
    g_assert_false(selection_toolbar_outside_press(window, &click, toolbar));
    g_assert_false(gtk_widget_get_visible(toolbar));
    gtk_widget_unparent(toolbar);
    gtk_window_destroy(GTK_WINDOW(window));
    g_print("selection_toolbar_test=PASS\n");
    return 0;
}
