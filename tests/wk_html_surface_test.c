#include "webkit/wk-html-surface.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                        \
	do {                                                                      \
		if (!(condition)) {                                                \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,    \
				#condition);                                        \
			failures++;                                               \
		}                                                                 \
	} while (0)

static void
check_panel_contract(void)
{
	GtkWidget *content_a;
	GtkWidget *content_b;
	GtkWidget *loading_a = NULL;
	GtkWidget *loading_b = NULL;
	GtkWidget *error_a;
	GtkWidget *content_child_a;
	GtkWidget *error_label;
	GtkWidget *panel_a;
	GtkWidget *panel_b;
	GtkWidget *label;
	GList *loading_children;
	GtkRequisition loading_min;
	GtkRequisition loading_natural;
	GtkRequisition content_min;
	GtkRequisition content_natural;

	content_a = gtk_label_new("Bible content");
	content_b = gtk_label_new("Commentary content");
	gtk_widget_set_size_request(content_a, 320, 180);
	panel_a = wk_html_surface_create_panel(content_a, "Loading Bible…",
					       &loading_a);
	panel_b = wk_html_surface_create_panel(content_b, "Loading commentary…",
					       &loading_b);
	gtk_widget_show_all(panel_a);
	gtk_widget_show_all(panel_b);
	content_child_a = gtk_stack_get_child_by_name(GTK_STACK(panel_a),
						      WK_HTML_CONTENT_CHILD);

	CHECK(GTK_IS_STACK(panel_a));
	CHECK(GTK_IS_STACK(panel_b));
	CHECK(gtk_stack_get_homogeneous(GTK_STACK(panel_a)));
	CHECK(gtk_stack_get_transition_type(GTK_STACK(panel_a)) ==
	      GTK_STACK_TRANSITION_TYPE_NONE);
	CHECK(gtk_widget_get_hexpand(panel_a));
	CHECK(gtk_widget_get_vexpand(panel_a));
	CHECK(gtk_stack_get_child_by_name(GTK_STACK(panel_a),
					  WK_HTML_LOADING_CHILD) == loading_a);
	CHECK(gtk_stack_get_child_by_name(GTK_STACK(panel_a),
					  WK_HTML_CONTENT_CHILD) == content_a);
	error_a = gtk_stack_get_child_by_name(GTK_STACK(panel_a),
					      WK_HTML_ERROR_CHILD);
	CHECK(error_a != NULL);
	CHECK(gtk_stack_get_visible_child(GTK_STACK(panel_a)) == loading_a);
	CHECK(gtk_stack_get_visible_child(GTK_STACK(panel_b)) == loading_b);
	/* show_all() must not make the renderer itself visible before READY. */
	CHECK(content_child_a == content_a);
	CHECK(gtk_widget_get_no_show_all(content_a));
	CHECK(!gtk_widget_get_visible(content_a));
	CHECK(gtk_style_context_has_class(gtk_widget_get_style_context(loading_a),
					  WK_HTML_LOADING_CLASS));
	loading_children = gtk_container_get_children(GTK_CONTAINER(loading_a));
	CHECK(g_list_length(loading_children) == 1);
	label = loading_children ? GTK_WIDGET(loading_children->data) : NULL;
	CHECK(GTK_IS_LABEL(label));
	if (GTK_IS_LABEL(label))
		CHECK(g_strcmp0(gtk_label_get_text(GTK_LABEL(label)),
			       "Loading Bible…") == 0);
	g_list_free(loading_children);

	/* A ready panel reveals independently; the other remains loading. */
	wk_html_surface_show_content(panel_a);
	CHECK(gtk_stack_get_visible_child(GTK_STACK(panel_a)) == content_a);
	CHECK(gtk_widget_get_visible(content_a));
	CHECK(gtk_stack_get_visible_child(GTK_STACK(panel_b)) == loading_b);
	wk_html_surface_show_content(panel_b);
	CHECK(gtk_stack_get_visible_child(GTK_STACK(panel_b)) == content_b);

	/* Failed cold loads have a visible terminal state, and a later load can
	 * recover without reconstructing the panel. */
	wk_html_surface_show_error(panel_a, "Unable to load test content.");
	CHECK(gtk_stack_get_visible_child(GTK_STACK(panel_a)) == error_a);
	CHECK(gtk_style_context_has_class(gtk_widget_get_style_context(error_a),
					  WK_HTML_ERROR_CLASS));
	loading_children = gtk_container_get_children(GTK_CONTAINER(error_a));
	CHECK(g_list_length(loading_children) == 1);
	error_label = loading_children ? GTK_WIDGET(loading_children->data) : NULL;
	CHECK(GTK_IS_LABEL(error_label));
	if (GTK_IS_LABEL(error_label))
		CHECK(g_strcmp0(gtk_label_get_text(GTK_LABEL(error_label)),
				"Unable to load test content.") == 0);
	g_list_free(loading_children);
	wk_html_surface_show_loading(panel_a);
	wk_html_surface_show_content(panel_a);
	CHECK(gtk_stack_get_visible_child(GTK_STACK(panel_a)) == content_a);

	/* Homogeneous sizing keeps a split pane's allocation stable across the
	 * LOADING -> CONTENT switch. */
	wk_html_surface_show_loading(panel_a);
	gtk_widget_get_preferred_size(panel_a, &loading_min, &loading_natural);
	wk_html_surface_show_content(panel_a);
	gtk_widget_get_preferred_size(panel_a, &content_min, &content_natural);
	CHECK(loading_min.width == content_min.width);
	CHECK(loading_min.height == content_min.height);
	CHECK(loading_natural.width == content_natural.width);
	CHECK(loading_natural.height == content_natural.height);

	gtk_widget_destroy(panel_a);
	gtk_widget_destroy(panel_b);
}

int
main(int argc, char **argv)
{
	GtkCssProvider *provider;
	gchar *stylesheet = NULL;
	gchar *renderer = NULL;
	gchar *startup = NULL;
	gchar *rule;
	gchar *rule_end;
	gchar *loading_rule;
	gchar *loading_rule_end;
	gchar *error_rule;
	gchar *error_rule_end;
	GError *error = NULL;
	gboolean have_display;

	have_display = gtk_init_check(&argc, &argv);

	/* GtkCssProvider can validate the production stylesheet without a
	 * display server, which keeps this regression runnable in headless CI. */
	provider = gtk_css_provider_new();
	gtk_css_provider_load_from_path(provider,
					SRCDIR "/ui/xiphos-style.css", &error);
	CHECK(error == NULL);
	if (error)
		g_error_free(error);
	CHECK(g_file_get_contents(SRCDIR "/ui/xiphos-style.css", &stylesheet,
				  NULL, NULL));
	CHECK(g_file_get_contents(SRCDIR "/src/webkit/wk-html.c", &renderer,
				  NULL, NULL));
	CHECK(g_file_get_contents(SRCDIR "/src/main/main.c", &startup,
				  NULL, NULL));

	rule = stylesheet ? strstr(stylesheet, "." WK_HTML_SURFACE_CLASS " {")
			  : NULL;
	CHECK(rule != NULL);
	rule_end = rule ? strchr(rule, '}') : NULL;
	CHECK(rule_end != NULL);
	if (rule && rule_end) {
		gchar *surface_rule = g_strndup(rule, rule_end - rule + 1);

		/* Named palette colors are replaced by every supported light/dark
		 * theme.  A literal white fallback would recreate the startup flash. */
		CHECK(strstr(surface_rule, "background-color: @elim_paper") != NULL);
		CHECK(strstr(surface_rule, "color: @elim_ink") != NULL);
		CHECK(strstr(surface_rule, "#fff") == NULL);
		CHECK(strstr(surface_rule, "white") == NULL);
		g_free(surface_rule);
	}
	loading_rule = stylesheet
			   ? strstr(stylesheet,
				    "." WK_HTML_LOADING_CLASS " {")
			   : NULL;
	CHECK(loading_rule != NULL);
	loading_rule_end = loading_rule ? strchr(loading_rule, '}') : NULL;
	CHECK(loading_rule_end != NULL);
	if (loading_rule && loading_rule_end) {
		gchar *placeholder_rule =
		    g_strndup(loading_rule, loading_rule_end - loading_rule + 1);

		CHECK(strstr(placeholder_rule,
			     "background-color: @elim_chrome") != NULL);
		CHECK(strstr(placeholder_rule, "color: alpha(@elim_ink") != NULL);
		CHECK(strstr(placeholder_rule, "#fff") == NULL);
		CHECK(strstr(placeholder_rule, "white") == NULL);
		g_free(placeholder_rule);
	}
	error_rule = stylesheet ? strstr(stylesheet, "." WK_HTML_ERROR_CLASS " {")
				: NULL;
	CHECK(error_rule != NULL);
	error_rule_end = error_rule ? strchr(error_rule, '}') : NULL;
	CHECK(error_rule_end != NULL);
	if (error_rule && error_rule_end) {
		gchar *failure_rule =
		    g_strndup(error_rule, error_rule_end - error_rule + 1);

		CHECK(strstr(failure_rule, "background-color: @elim_paper") != NULL);
		CHECK(strstr(failure_rule, "color: @elim_ink") != NULL);
		CHECK(strstr(failure_rule, "#fff") == NULL);
		CHECK(strstr(failure_rule, "white") == NULL);
		g_free(failure_rule);
	}
	if (renderer) {
		/* Every real content panel goes through WkHtml's reusable stack.
		 * Initial loads reveal only on READY; reloads retain ready content. */
		CHECK(strstr(renderer, "wk_html_surface_create_panel") != NULL);
		CHECK(strstr(renderer, "wk_html_surface_show_loading") != NULL);
		CHECK(strstr(renderer, "wk_html_surface_show_content") != NULL);
		CHECK(strstr(renderer, "wk_html_surface_show_error") != NULL);
		CHECK(strstr(renderer, "wk_html_load_failed") != NULL);
		CHECK(strstr(renderer, "panel_load_model_content_available") != NULL);
		CHECK(strstr(renderer, "BIBLIA_ELIM_UI_LOAD_DEBUG") != NULL);
		CHECK(strstr(renderer, "LOAD_STARTED") != NULL);
		CHECK(strstr(renderer, "LOAD_COMMITTED") != NULL);
		CHECK(strstr(renderer, "LOAD_FINISHED") != NULL);
		CHECK(strstr(renderer, "schedule_content_reveal") != NULL);
		CHECK(strstr(renderer, "g_idle_add(reveal_content_idle") != NULL);
		/* Annotated-word activation keeps the established click-without-drag
		 * contract, defers past the double-click interval, and leaves the
		 * right-click context-menu branch intact. */
		CHECK(strstr(renderer, "action=showNeutralWord") != NULL);
		CHECK(strstr(renderer, "gtk_drag_check_threshold") != NULL);
		CHECK(strstr(renderer, "GDK_2BUTTON_PRESS") != NULL);
		CHECK(strstr(renderer, "event->button == 3") != NULL);
		CHECK(strstr(renderer, "activate_pending_word") != NULL);
	}
	if (startup) {
		const gchar *theme = strstr(startup, "gui_elim_tema_init();");
		const gchar *window = strstr(startup, "create_mainwindow();");

		/* The palette must exist before create_mainwindow() reaches its
		 * show_all()/event drain and maps the first content frame. */
		CHECK(theme != NULL);
		CHECK(window != NULL);
		if (theme && window)
			CHECK(theme < window);
	}
	if (have_display)
		check_panel_contract();
	else
		fprintf(stderr, "SKIP live GtkStack assertions: no display\n");

	g_free(renderer);
	g_free(startup);
	g_free(stylesheet);
	g_object_unref(provider);
	printf("wk_html_surface_failures=%d\n", failures);
	return failures ? 1 : 0;
}
