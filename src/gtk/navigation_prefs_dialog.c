/*
 * Biblia Elim
 * navigation_prefs_dialog.c - Ver > Navegación y rueda
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

/*
 * Two reading preferences in reader's terms: how much a turn of the mouse
 * wheel moves the page, and how readily the highlighted verse follows the
 * scroll. The dialog only edits NavigationPrefs (main/navigation_prefs.h);
 * the Bible pane knows nothing about it.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gui/navigation_prefs_dialog.h"
#include "gui/widgets.h"
#include "main/navigation_prefs.h"
#include "main/settings.h"

#include "gui/debug_glib_null.h"

#define RESPONSE_RESET 1

typedef struct {
	GtkWidget *dialog;
	GtkWidget *scale;
	GtkWidget *percent;
	GtkWidget *immediate;
	GtkWidget *balanced;
	GtkWidget *stable;
	gboolean syncing;	/* widgets being set from code, not by the reader */
	gboolean unsaved;	/* applied but not yet written to settings.xml */
} PrefsDialog;

static PrefsDialog *open_dialog = NULL;

static NavigationPrefs
dialog_prefs(PrefsDialog *d)
{
	NavigationPrefs prefs;

	prefs.wheel_percent =
	    navigation_prefs_snap_percent(gtk_range_get_value(GTK_RANGE(d->scale)));
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->immediate)))
		prefs.focus_mode = READING_FOCUS_IMMEDIATE;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->stable)))
		prefs.focus_mode = READING_FOCUS_STABLE;
	else
		prefs.focus_mode = READING_FOCUS_BALANCED;
	return prefs;
}

static void
save(PrefsDialog *d)
{
	NavigationPrefs prefs = dialog_prefs(d);

	navigation_prefs_save_settings(&prefs, settings.fnconfigure);
	d->unsaved = FALSE;
}

static void
show_percent(PrefsDialog *d, gint percent)
{
	gchar *text = g_strdup_printf(_("%d %%"), percent);

	gtk_label_set_text(GTK_LABEL(d->percent), text);
	g_free(text);
}

/* Live: the next wheel notch uses it. A glide already moving keeps its
 * target (wheel_scroll_set_distance_scale()). Written later. */
static void
on_scale_changed(GtkRange *range, gpointer data)
{
	PrefsDialog *d = data;
	gdouble value = gtk_range_get_value(range);
	gint percent = navigation_prefs_snap_percent(value);
	NavigationPrefs prefs;

	if ((gdouble)percent != value && !d->syncing) {
		/* dragging lands between steps: keep it on a 5 % step */
		d->syncing = TRUE;
		gtk_range_set_value(range, percent);
		d->syncing = FALSE;
	}
	show_percent(d, percent);
	prefs = navigation_prefs_current();
	if (prefs.wheel_percent == percent)
		return;
	prefs.wheel_percent = percent;
	navigation_prefs_apply(&prefs);
	d->unsaved = TRUE;
}

/* the slider let go: write once, not on every step of the drag */
static gboolean
on_scale_released(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	PrefsDialog *d = data;

	(void)widget;
	(void)event;
	if (d->unsaved)
		save(d);
	return FALSE;
}

static void
on_mode_toggled(GtkToggleButton *button, gpointer data)
{
	PrefsDialog *d = data;
	NavigationPrefs prefs;

	if (d->syncing || !gtk_toggle_button_get_active(button))
		return;
	prefs = navigation_prefs_current();
	prefs.focus_mode = dialog_prefs(d).focus_mode;
	navigation_prefs_apply(&prefs);
	save(d);
}

static void
set_widgets(PrefsDialog *d, const NavigationPrefs *prefs)
{
	d->syncing = TRUE;
	gtk_range_set_value(GTK_RANGE(d->scale), prefs->wheel_percent);
	show_percent(d, prefs->wheel_percent);
	gtk_toggle_button_set_active(
	    GTK_TOGGLE_BUTTON(prefs->focus_mode == READING_FOCUS_IMMEDIATE
				  ? d->immediate
				  : prefs->focus_mode == READING_FOCUS_STABLE
					? d->stable
					: d->balanced),
	    TRUE);
	d->syncing = FALSE;
}

static void
on_response(GtkDialog *dialog, gint response, gpointer data)
{
	PrefsDialog *d = data;

	if (response == RESPONSE_RESET) {
		NavigationPrefs defaults = navigation_prefs_defaults();

		set_widgets(d, &defaults);
		navigation_prefs_apply(&defaults);
		save(d);
		return;
	}
	/* Cerrar, Escape, the window's close button */
	if (d->unsaved)
		save(d);
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void
on_destroy(GtkWidget *widget, gpointer data)
{
	PrefsDialog *d = data;

	(void)widget;
	if (open_dialog == d)
		open_dialog = NULL;
	g_free(d);
}

static GtkWidget *
heading(const char *text)
{
	GtkWidget *label = gtk_label_new(NULL);
	gchar *markup = g_markup_printf_escaped("<b>%s</b>", text);

	gtk_label_set_markup(GTK_LABEL(label), markup);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	g_free(markup);
	return label;
}

static GtkWidget *
caption(const char *text, gboolean dim)
{
	GtkWidget *label = gtk_label_new(text);

	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(label), 52);
	if (dim)
		gtk_style_context_add_class(gtk_widget_get_style_context(label),
					    GTK_STYLE_CLASS_DIM_LABEL);
	return label;
}

void
gui_navigation_prefs_dialog_show(void)
{
	PrefsDialog *d;
	GtkWidget *content, *box, *row, *less, *more, *reset;
	NavigationPrefs prefs;

	if (open_dialog) {
		gtk_window_present(GTK_WINDOW(open_dialog->dialog));
		return;
	}

	d = g_new0(PrefsDialog, 1);
	open_dialog = d;
	d->dialog = gtk_dialog_new_with_buttons(
	    _("Navegación y rueda"),
	    widgets.app ? GTK_WINDOW(widgets.app) : NULL,
	    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	    NULL, NULL);
	gtk_window_set_resizable(GTK_WINDOW(d->dialog), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(d->dialog), 460, -1);

	reset = gtk_dialog_add_button(GTK_DIALOG(d->dialog),
				      _("Restablecer valores predeterminados"),
				      RESPONSE_RESET);
	gtk_dialog_add_button(GTK_DIALOG(d->dialog), _("Cerrar"),
			      GTK_RESPONSE_CLOSE);
	gtk_dialog_set_default_response(GTK_DIALOG(d->dialog), GTK_RESPONSE_CLOSE);
	(void)reset;

	content = gtk_dialog_get_content_area(GTK_DIALOG(d->dialog));
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_set_border_width(GTK_CONTAINER(box), 18);
	gtk_box_pack_start(GTK_BOX(content), box, TRUE, TRUE, 0);

	/* ---- Rueda del mouse ---- */
	gtk_box_pack_start(GTK_BOX(box), heading(_("Rueda del mouse")), FALSE,
			   FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), caption(_("Desplazamiento por muesca"), FALSE),
			   FALSE, FALSE, 0);

	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	less = gtk_label_new(_("Menos"));
	more = gtk_label_new(_("Más"));
	d->scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
					    NAVIGATION_PREFS_WHEEL_PERCENT_MIN,
					    NAVIGATION_PREFS_WHEEL_PERCENT_MAX,
					    NAVIGATION_PREFS_WHEEL_PERCENT_STEP);
	/* arrow keys and Page keys: one 5 % step */
	gtk_range_set_increments(GTK_RANGE(d->scale),
				 NAVIGATION_PREFS_WHEEL_PERCENT_STEP,
				 NAVIGATION_PREFS_WHEEL_PERCENT_STEP);
	gtk_scale_set_draw_value(GTK_SCALE(d->scale), FALSE);
	gtk_scale_add_mark(GTK_SCALE(d->scale),
			   NAVIGATION_PREFS_WHEEL_PERCENT_DEFAULT, GTK_POS_BOTTOM,
			   NULL);
	gtk_widget_set_hexpand(d->scale, TRUE);
	gtk_widget_set_can_focus(d->scale, TRUE);
	gtk_widget_set_tooltip_text(
	    d->scale, _("Controla cuánto avanza la página con cada giro de la rueda."));
	atk_object_set_name(gtk_widget_get_accessible(d->scale),
			    _("Desplazamiento por muesca"));
	gtk_box_pack_start(GTK_BOX(row), less, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(row), d->scale, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(row), more, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), row, FALSE, FALSE, 0);

	d->percent = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(box), d->percent, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box),
			   caption(_("Menos desplazamiento requiere girar más la "
				     "rueda para avanzar por el texto."),
				   TRUE),
			   FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(box),
			   gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE,
			   FALSE, 8);

	/* ---- Seguimiento del versículo ---- */
	gtk_box_pack_start(GTK_BOX(box), heading(_("Seguimiento del versículo")),
			   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box),
			   caption(_("Cambio de versículo enfocado"), FALSE), FALSE,
			   FALSE, 0);
	d->immediate = gtk_radio_button_new_with_mnemonic(NULL, _("_Inmediato"));
	d->balanced = gtk_radio_button_new_with_mnemonic_from_widget(
	    GTK_RADIO_BUTTON(d->immediate), _("_Equilibrado"));
	d->stable = gtk_radio_button_new_with_mnemonic_from_widget(
	    GTK_RADIO_BUTTON(d->immediate), _("E_stable"));
	gtk_widget_set_tooltip_text(
	    d->immediate,
	    _("El versículo resaltado cambia en cuanto el siguiente llega a la "
	      "línea de lectura."));
	gtk_widget_set_tooltip_text(
	    d->balanced, _("El comportamiento habitual."));
	gtk_widget_set_tooltip_text(
	    d->stable, _("Requiere desplazar un poco más antes de cambiar el "
			 "versículo resaltado."));
	gtk_box_pack_start(GTK_BOX(box), d->immediate, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), d->balanced, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), d->stable, FALSE, FALSE, 0);

	prefs = navigation_prefs_current();
	set_widgets(d, &prefs);

	g_signal_connect(d->scale, "value-changed", G_CALLBACK(on_scale_changed), d);
	g_signal_connect(d->scale, "button-release-event",
			 G_CALLBACK(on_scale_released), d);
	g_signal_connect(d->immediate, "toggled", G_CALLBACK(on_mode_toggled), d);
	g_signal_connect(d->balanced, "toggled", G_CALLBACK(on_mode_toggled), d);
	g_signal_connect(d->stable, "toggled", G_CALLBACK(on_mode_toggled), d);
	g_signal_connect(d->dialog, "response", G_CALLBACK(on_response), d);
	g_signal_connect(d->dialog, "destroy", G_CALLBACK(on_destroy), d);

	gtk_widget_show_all(d->dialog);
	gtk_widget_grab_focus(d->scale);
}
