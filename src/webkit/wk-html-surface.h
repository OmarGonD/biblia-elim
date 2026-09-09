/* Theme-coherent background policy for HTML-like content surfaces. */

#ifndef __WK_HTML_SURFACE_H__
#define __WK_HTML_SURFACE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WK_HTML_SURFACE_CLASS "elim-html-surface"
#define WK_HTML_LOADING_CLASS "elim-html-loading"
#define WK_HTML_ERROR_CLASS "elim-html-error"
#define WK_HTML_LOADING_CHILD "loading"
#define WK_HTML_CONTENT_CHILD "content"
#define WK_HTML_ERROR_CHILD "error"

void wk_html_surface_prepare(GtkWidget *widget);
GtkWidget *wk_html_surface_create_panel(GtkWidget *content,
					const gchar *loading_text,
					GtkWidget **loading_surface);
void wk_html_surface_show_loading(GtkWidget *stack);
void wk_html_surface_show_content(GtkWidget *stack);
void wk_html_surface_show_error(GtkWidget *stack, const gchar *message);

G_END_DECLS

#endif
