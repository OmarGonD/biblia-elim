/*
 * Keep an HTML-like widget on the application paper color before its first
 * document supplies a body background.  The color itself deliberately lives
 * in the GTK stylesheet so light, dark, and live theme changes use the same
 * palette as the rest of the application.
 */

#include "wk-html-surface.h"

#define WK_HTML_ERROR_LABEL_DATA "wk-html-error-label"

void
wk_html_surface_prepare(GtkWidget *widget)
{
	GtkStyleContext *context;

	g_return_if_fail(GTK_IS_WIDGET(widget));
	context = gtk_widget_get_style_context(widget);
	gtk_style_context_add_class(context, WK_HTML_SURFACE_CLASS);
}

GtkWidget *
wk_html_surface_create_panel(GtkWidget *content, const gchar *loading_text,
			     GtkWidget **loading_surface)
{
	GtkWidget *stack;
	GtkWidget *loading;
	GtkWidget *label;
	GtkWidget *error;
	GtkWidget *error_label;

	g_return_val_if_fail(GTK_IS_WIDGET(content), NULL);
	stack = gtk_stack_new();
	wk_html_surface_prepare(stack);
	gtk_widget_set_hexpand(stack, TRUE);
	gtk_widget_set_vexpand(stack, TRUE);
	/* Both children retain the same allocation, so revealing content does
	 * not disturb a paned window's user-selected position. */
	gtk_stack_set_homogeneous(GTK_STACK(stack), TRUE);
	gtk_stack_set_transition_type(GTK_STACK(stack),
				      GTK_STACK_TRANSITION_TYPE_NONE);

	/* A window-level gtk_widget_show_all() must not make the renderer a
	 * candidate for the first frame.  GtkStack only draws its selected child,
	 * but keeping the content widget itself hidden makes the startup contract
	 * explicit and prevents future container changes from exposing it.  The
	 * ready path shows it directly, which intentionally ignores no-show-all. */
	gtk_widget_set_no_show_all(content, TRUE);
	gtk_widget_hide(content);

	loading = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	wk_html_surface_prepare(loading);
	gtk_style_context_add_class(gtk_widget_get_style_context(loading),
				    WK_HTML_LOADING_CLASS);
	label = gtk_label_new(loading_text);
	gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
	gtk_style_context_add_class(gtk_widget_get_style_context(label),
				    "dim-label");
	gtk_box_pack_start(GTK_BOX(loading), label, TRUE, TRUE, 0);

	error = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	wk_html_surface_prepare(error);
	gtk_style_context_add_class(gtk_widget_get_style_context(error),
				    WK_HTML_ERROR_CLASS);
	error_label = gtk_label_new(NULL);
	gtk_widget_set_halign(error_label, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(error_label, GTK_ALIGN_CENTER);
	gtk_label_set_line_wrap(GTK_LABEL(error_label), TRUE);
	gtk_box_pack_start(GTK_BOX(error), error_label, TRUE, TRUE, 0);
	g_object_set_data(G_OBJECT(stack), WK_HTML_ERROR_LABEL_DATA, error_label);

	gtk_stack_add_named(GTK_STACK(stack), loading, WK_HTML_LOADING_CHILD);
	gtk_stack_add_named(GTK_STACK(stack), content, WK_HTML_CONTENT_CHILD);
	gtk_stack_add_named(GTK_STACK(stack), error, WK_HTML_ERROR_CHILD);
	wk_html_surface_show_loading(stack);
	if (loading_surface)
		*loading_surface = loading;
	return stack;
}

void
wk_html_surface_show_loading(GtkWidget *stack)
{
	g_return_if_fail(GTK_IS_STACK(stack));
	gtk_stack_set_visible_child_name(GTK_STACK(stack), WK_HTML_LOADING_CHILD);
}

void
wk_html_surface_show_content(GtkWidget *stack)
{
	GtkWidget *content;

	g_return_if_fail(GTK_IS_STACK(stack));
	content = gtk_stack_get_child_by_name(GTK_STACK(stack),
					      WK_HTML_CONTENT_CHILD);
	if (content)
		gtk_widget_show(content);
	gtk_stack_set_visible_child_name(GTK_STACK(stack), WK_HTML_CONTENT_CHILD);
}

void
wk_html_surface_show_error(GtkWidget *stack, const gchar *message)
{
	GtkWidget *label;

	g_return_if_fail(GTK_IS_STACK(stack));
	label = GTK_WIDGET(g_object_get_data(G_OBJECT(stack),
					     WK_HTML_ERROR_LABEL_DATA));
	if (GTK_IS_LABEL(label))
		gtk_label_set_text(GTK_LABEL(label), message ? message : "");
	gtk_stack_set_visible_child_name(GTK_STACK(stack), WK_HTML_ERROR_CHILD);
}
