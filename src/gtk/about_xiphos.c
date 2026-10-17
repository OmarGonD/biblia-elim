/*
 * Ministerios Elim — Bible study
 * about_xiphos.c - Xiphos about dialog
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include "gui/widget_helpers.h"
#include <glib/gi18n.h>

#include "gui/about_xiphos.h"
#include "main/sword.h"
#include "gui/utilities.h"
#include "main/settings.h"

static const gchar *authors[] = {
    "Omar Gonzáles Díaz",
    NULL};

/******************************************************************************
 * Name
 *   on_dialog_response
 *
 * Synopsis
 *   #include "gui/.h"
 *
 *   void on_dialog_response(GtkDialog * dialog, gint response_id,
					gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void
on_dialog_response(GtkDialog *dialog, gint response_id,
		   gpointer user_data)
{
	gui_widget_destroy(GTK_WIDGET(dialog));
}

/******************************************************************************
 * Name
 *   gui_create_about_xiphos
 *
 * Synopsis
 *   #include "gui/about_xiphos.h"
 *
 *   void gui_create_about_xiphos(void);
 *
 * Description
 *   Create Ministerios Elim about dialog and return pointer on it.
 *
 * Return value
 *   GtkWidget *
 */

#  define	BUILD_TYPE	"gtk4 native"

GtkWidget *gui_create_about_xiphos(void)
{
	GdkPixbuf *about1_logo_pixbuf;
	GtkWidget *about1;
	gchar versionbuild[128];

	snprintf(versionbuild, 120, "%s (%s)", VERSION, BUILD_TYPE);

	about1_logo_pixbuf = pixbuf_finder("ministerios-elim.png", 0, NULL);

	about1 = gtk_about_dialog_new();
	gtk_widget_add_css_class(about1, "elim-about-dialog");
	g_signal_connect(about1, "response",
			 G_CALLBACK(on_dialog_response), NULL);

	gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about1), versionbuild);
	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about1), "Ministerios Elim");
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about1),
					       "Copyright © 2026 Omar Gonzáles Díaz");
	gchar *backend_status = main_backend_status();
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about1), backend_status);
	g_free(backend_status);
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about1),
					     "https://github.com/OmarGonD/biblia-elim");
	{
		static const gchar *stepbible[] = {
			"STEPBible.org, Tyndale House, Cambridge (CC BY 4.0)",
			NULL};
		gtk_about_dialog_add_credit_section(GTK_ABOUT_DIALOG(about1),
						    _("Atribución de datos TAGNT"),
						    stepbible);
	}
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about1), authors);
	if (about1_logo_pixbuf) {
		GdkPixbuf *scaled_logo = gdk_pixbuf_scale_simple(
			about1_logo_pixbuf, 310, 228, GDK_INTERP_BILINEAR);
		GdkTexture *logo = gdk_texture_new_for_pixbuf(
			scaled_logo ? scaled_logo : about1_logo_pixbuf);
		gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(about1),
					  GDK_PAINTABLE(logo));
		g_object_unref(logo);
		if (scaled_logo)
			g_object_unref(scaled_logo);
		g_object_unref(about1_logo_pixbuf);
	}

	set_window_icon(GTK_WINDOW(about1));

	return about1;
}
