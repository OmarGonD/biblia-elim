/*
 * Biblia Elim
 * planes_lectura.c - diálogo Planes de lectura
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gui/planes_lectura.h"
#include "gui/plan_personal.h"
#include "gui/progreso_lectura.h"
#include "gui/recordatorio.h"
#include "gui/dialog.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/planes_lectura.h"
#include "main/navbar_versekey.h"
#include "main/settings.h"
#include "main/url.hh"
#include "main/xml.h"

#include "gui/debug_glib_null.h"

enum {
	PCOL_MARCA = 0,	/* markup: nombre + días + "en curso" */
	PCOL_INDICE,
	N_PCOLS
};

enum {
	DCOL_HECHO = 0,
	DCOL_DIA,	/* "Día 12" */
	DCOL_LECTURA,
	DCOL_NUMERO,
	DCOL_PESO,	/* negrita para el día que toca */
	N_DCOLS
};

typedef struct {
	GtkWidget *dialog;
	GtkWidget *tree_planes;
	GtkWidget *tree_dias;
	GtkWidget *lbl_nombre;
	GtkWidget *lbl_desc;
	GtkWidget *lbl_estado;
	GtkWidget *barra;
	GtkWidget *btn_empezar;
	GtkWidget *btn_hoy;
	GtkWidget *btn_reiniciar;
	GtkWidget *btn_al_dia;
	GtkWidget *btn_progreso;
	GtkWidget *btn_nuevo;
	GtkWidget *btn_editar;
	GtkWidget *btn_borrar;
	GtkWidget *chk_recordatorio;
	GtkWidget *spin_hora;
	GtkWidget *spin_minuto;
	GtkWidget *btn_probar;
	GtkWidget *lbl_recordatorio;
	gboolean poniendo_hora;	/* cargando los valores, no tocar el disco */
	GtkListStore *planes;
	GtkListStore *dias;
	const PL_PLAN *plan;	/* el que se está mirando */
} PLANES_UI;

static PLANES_UI *ui = NULL;

/* Marcar un día es de las pocas cosas que el lector espera no perder
 * aunque la aplicación se cierre mal, y pasa una vez al día: vale la
 * pena volcar settings.xml en el momento en vez de esperar al cierre. */
static void
guardar_ya(void)
{
	if (settings.fnconfigure)
		xml_save_settings_doc(settings.fnconfigure);
}

/* --------------------------------------------------------------------
 * Abrir una lectura en la ventana principal
 * ------------------------------------------------------------------ */

static void
abrir_referencia(const char *ref)
{
	gchar *url;
	char *valida;

	if (!ref || !*ref)
		return;
	/* Las referencias del plan vienen con el nombre OSIS del libro
	 * ("John 1"); pasarlas por el motor las deja en el idioma del
	 * módulo antes de navegar, igual que hace la barra de arriba. */
	valida = (char *)main_get_valid_key(settings.MainWindowModule, ref);
	url = g_strdup_printf("sword:///%s",
			      (valida && *valida) ? valida : ref);
	free(valida);
	main_url_handler(url, TRUE);
	g_free(url);
}

static void
abrir_dia(const PL_PLAN *plan, int dia)
{
	GList *refs = main_planes_referencias(plan, dia);
	if (refs)
		abrir_referencia((const char *)refs->data);
	g_list_free_full(refs, g_free);
}

/* --------------------------------------------------------------------
 * La lectura de hoy, sin pasar por el diálogo
 *
 * Es lo que se hace todos los días, así que la ventana principal la
 * tiene a un clic; el diálogo queda para elegir plan y llevar la cuenta.
 * ------------------------------------------------------------------ */

static const PL_PLAN *
plan_en_curso(void)
{
	const char *activo = main_planes_activo();
	return activo ? main_planes_por_id(activo) : NULL;
}

gchar *
gui_planes_lectura_resumen_hoy(void)
{
	const PL_PLAN *plan = plan_en_curso();
	const char *titulo;
	gchar *lectura, *cabeza, *texto;
	int dia;

	if (!plan)
		return NULL;

	dia = main_planes_dia_de_hoy(plan);
	lectura = main_planes_lectura(plan, dia);
	titulo = main_planes_titulo(plan, dia);

	if (main_planes_dias_hechos(plan) >= plan->dias)
		cabeza = g_strdup_printf(_("%s · terminado"), _(plan->nombre));
	else
		cabeza = g_strdup_printf(_("%s · día %d de %d"),
					 _(plan->nombre), dia, plan->dias);

	texto = g_strdup_printf("%s\n%s%s%s", cabeza,
				lectura ? lectura : "",
				(titulo && *titulo) ? " — " : "",
				(titulo && *titulo) ? titulo : "");
	g_free(cabeza);
	g_free(lectura);
	return texto;
}

PL_HOY
gui_planes_lectura_estado_hoy(gchar **detalle)
{
	return main_planes_estado_hoy(detalle);
}

/* Lo que pinta el diálogo, que se monta más abajo: marcar desde la
 * ventana principal tiene que poder refrescarlo si está abierto. */
static void llenar_dias(const PL_PLAN *plan);
static void refrescar_cabecera(void);
static void refrescar_lista_planes(void);
static void seleccionar_plan_en_lista(const PL_PLAN *plan);

/* El diálogo puede estar abierto mientras se marca desde la ventana
 * principal: que no se quede enseñando lo de antes. */
static void
refrescar_dialogo_si_abierto(void)
{
	const PL_PLAN *sel;

	if (!ui || !ui->dialog)
		return;
	sel = ui->plan;
	refrescar_lista_planes();
	seleccionar_plan_en_lista(sel);
	llenar_dias(ui->plan);
	refrescar_cabecera();
}

void
gui_planes_lectura_marcar_hoy(void)
{
	const PL_PLAN *plan = plan_en_curso();
	gchar *aviso;
	int dia, hechos, calendario;

	if (!plan) {
		gui_planes_lectura_dialog();
		return;
	}

	dia = main_planes_dia_de_hoy(plan);
	if (main_planes_dia_hecho(plan, dia))
		return;

	/* El botón marca el primer día sin marcar: si el de hoy ya está,
	 * el siguiente clic se comería el de mañana. Se puede querer (leí
	 * dos de una sentada), pero no a ciegas. */
	calendario = main_planes_dia_segun_calendario(plan);
	if (calendario > 0 && dia > calendario) {
		gchar *pregunta =
		    g_strdup_printf(_("La lectura de hoy de «%s» ya está "
				      "marcada. ¿Marcas también el día %d?"),
				    _(plan->nombre), dia);
		gboolean sigue = gui_yes_no_dialog(pregunta, NULL);
		g_free(pregunta);
		if (!sigue)
			return;
	}

	main_planes_marcar(plan, dia, TRUE);
	guardar_ya();

	hechos = main_planes_dias_hechos(plan);
	if (hechos >= plan->dias) {
		aviso = g_strdup_printf(_("Día %d marcado · terminaste «%s». "
					  "Enhorabuena."),
					dia, _(plan->nombre));
	} else {
		gchar *siguiente =
		    main_planes_lectura(plan, main_planes_dia_de_hoy(plan));
		aviso = g_strdup_printf(_("Día %d marcado · %d de %d días · "
					  "lo siguiente: %s"),
					dia, hechos, plan->dias,
					siguiente ? siguiente : "");
		g_free(siguiente);
	}
	gui_set_statusbar(aviso);
	g_free(aviso);

	refrescar_dialogo_si_abierto();
}

void
gui_planes_lectura_hoy(void)
{
	const PL_PLAN *plan = plan_en_curso();
	gchar *lectura, *aviso;
	int dia;

	if (!plan) {
		/* Sin plan que seguir no hay lectura de hoy: lo primero
		 * es elegir uno. */
		gui_planes_lectura_dialog();
		return;
	}

	dia = main_planes_dia_de_hoy(plan);
	abrir_dia(plan, dia);

	lectura = main_planes_lectura(plan, dia);
	aviso = g_strdup_printf(_("%s · día %d de %d · %s"),
				_(plan->nombre), dia, plan->dias,
				lectura ? lectura : "");
	gui_set_statusbar(aviso);
	g_free(lectura);
	g_free(aviso);
}

/* --------------------------------------------------------------------
 * Pintar el detalle del plan elegido
 * ------------------------------------------------------------------ */

static void
llenar_dias(const PL_PLAN *plan)
{
	int dia, hoy;

	gtk_list_store_clear(ui->dias);
	if (!plan)
		return;

	hoy = main_planes_dia_de_hoy(plan);

	for (dia = 1; dia <= plan->dias; ++dia) {
		GtkTreeIter iter;
		gchar *etiqueta = g_strdup_printf(_("Día %d"), dia);
		gchar *lectura = main_planes_lectura(plan, dia);
		const char *titulo = main_planes_titulo(plan, dia);
		gchar *texto;

		if (titulo && *titulo)
			texto = g_strdup_printf("%s — %s", lectura, titulo);
		else
			texto = g_strdup(lectura);

		gtk_list_store_append(ui->dias, &iter);
		gtk_list_store_set(ui->dias, &iter,
				   DCOL_HECHO, main_planes_dia_hecho(plan, dia),
				   DCOL_DIA, etiqueta,
				   DCOL_LECTURA, texto,
				   DCOL_NUMERO, dia,
				   DCOL_PESO, (dia == hoy) ? PANGO_WEIGHT_BOLD
							   : PANGO_WEIGHT_NORMAL,
				   -1);
		g_free(etiqueta);
		g_free(lectura);
		g_free(texto);
	}
}

static void
ir_a_dia(int dia, gboolean abrir)
{
	GtkTreePath *path;

	if (!ui->plan || dia < 1)
		return;
	path = gtk_tree_path_new_from_indices(dia - 1, -1);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(ui->tree_dias), path, NULL, FALSE);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(ui->tree_dias), path, NULL,
				     TRUE, 0.4, 0.0);
	gtk_tree_path_free(path);
	if (abrir)
		abrir_dia(ui->plan, dia);
}

static void
refrescar_cabecera(void)
{
	const PL_PLAN *plan = ui->plan;
	const char *activo;
	gchar *markup, *estado;
	int hechos, hoy, calendario;
	gboolean en_curso;

	if (!plan) {
		gtk_label_set_text(GTK_LABEL(ui->lbl_nombre), "");
		gtk_label_set_text(GTK_LABEL(ui->lbl_desc), "");
		gtk_label_set_text(GTK_LABEL(ui->lbl_estado), "");
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ui->barra), 0.0);
		gtk_widget_set_sensitive(ui->btn_empezar, FALSE);
		gtk_widget_set_sensitive(ui->btn_hoy, FALSE);
		gtk_widget_set_sensitive(ui->btn_reiniciar, FALSE);
		gtk_widget_set_sensitive(ui->btn_al_dia, FALSE);
		gtk_widget_set_sensitive(ui->btn_editar, FALSE);
		gtk_widget_set_sensitive(ui->btn_borrar, FALSE);
		return;
	}

	activo = main_planes_activo();
	en_curso = (activo && !strcmp(activo, plan->id));

	markup = g_markup_printf_escaped("<span size='x-large' weight='bold'>%s</span>",
					 _(plan->nombre));
	gtk_label_set_markup(GTK_LABEL(ui->lbl_nombre), markup);
	g_free(markup);

	markup = g_markup_printf_escaped("<span alpha='75%%'>%s</span>",
					 _(plan->descripcion));
	gtk_label_set_markup(GTK_LABEL(ui->lbl_desc), markup);
	g_free(markup);

	hechos = main_planes_dias_hechos(plan);
	hoy = main_planes_dia_de_hoy(plan);
	calendario = main_planes_dia_segun_calendario(plan);

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ui->barra),
				      (double)hechos / (double)plan->dias);
	markup = g_strdup_printf(_("%d de %d días · %d%%"),
				 hechos, plan->dias,
				 (hechos * 100) / plan->dias);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(ui->barra), markup);
	g_free(markup);

	if (hechos >= plan->dias)
		estado = g_strdup(_("Plan terminado. Enhorabuena."));
	else if (!calendario)
		estado = g_strdup_printf(_("Sin empezar · %d capítulos en total"),
					 main_planes_total_capitulos(plan));
	else if (hoy < calendario)
		estado = g_strdup_printf(ngettext("Toca el día %d · vas %d día por detrás del calendario",
						  "Toca el día %d · vas %d días por detrás del calendario",
						  calendario - hoy),
					 hoy, calendario - hoy);
	else if (hoy > calendario)
		estado = g_strdup_printf(ngettext("Toca el día %d · vas %d día adelantado",
						  "Toca el día %d · vas %d días adelantado",
						  hoy - calendario),
					 hoy, hoy - calendario);
	else
		estado = g_strdup_printf(_("Toca el día %d · vas al día"), hoy);
	gtk_label_set_text(GTK_LABEL(ui->lbl_estado), estado);
	g_free(estado);

	gtk_button_set_label(GTK_BUTTON(ui->btn_empezar),
			     en_curso ? _("Plan en curso")
				      : _("Empezar este plan"));
	gtk_widget_set_sensitive(ui->btn_empezar, !en_curso);
	gtk_widget_set_sensitive(ui->btn_hoy, TRUE);
	gtk_widget_set_sensitive(ui->btn_reiniciar, hechos > 0 || calendario > 0);
	/* Solo hay días que recuperar si el calendario va por delante. */
	gtk_widget_set_sensitive(ui->btn_al_dia,
				 main_planes_dias_atrasados(plan) > 0);

	/* Los planes de la casa no se tocan; los del lector, sí. */
	{
		gboolean mio = main_planes_es_personal(plan);
		gtk_widget_set_sensitive(ui->btn_editar, mio);
		gtk_widget_set_sensitive(ui->btn_borrar, mio);
	}
}

/* La lista de la izquierda dice cuál está en curso y cuánto lleva. */
static void
refrescar_lista_planes(void)
{
	const char *activo = main_planes_activo();
	int i;

	gtk_list_store_clear(ui->planes);
	for (i = 0; i < main_planes_cuantos(); ++i) {
		const PL_PLAN *plan = main_planes_get(i);
		GtkTreeIter iter;
		gboolean en_curso = (activo && !strcmp(activo, plan->id));
		int hechos = main_planes_dias_hechos(plan);
		gchar *pie, *markup;

		if (en_curso)
			pie = g_strdup_printf(_("en curso · %d de %d días"),
					      hechos, plan->dias);
		else if (hechos)
			pie = g_strdup_printf(_("%d de %d días"),
					      hechos, plan->dias);
		else
			pie = g_strdup_printf(ngettext("%d día", "%d días",
						       plan->dias),
					      plan->dias);

		if (main_planes_es_personal(plan)) {
			gchar *mio = g_strdup_printf(_("%s · tuyo"), pie);
			g_free(pie);
			pie = mio;
		}

		{
			/* El nombre y el pie se escapan a mano: la negrita
			 * del plan en curso es marcado de verdad, y
			 * g_markup_printf_escaped() convertiría las
			 * etiquetas en texto literal. */
			gchar *nombre = g_markup_escape_text(_(plan->nombre), -1);
			gchar *pie_esc = g_markup_escape_text(pie, -1);
			markup = g_strdup_printf("%s%s%s\n"
						 "<span size='small' alpha='70%%'>%s</span>",
						 en_curso ? "<b>" : "", nombre,
						 en_curso ? "</b>" : "", pie_esc);
			g_free(nombre);
			g_free(pie_esc);
		}
		gtk_list_store_append(ui->planes, &iter);
		gtk_list_store_set(ui->planes, &iter,
				   PCOL_MARCA, markup,
				   PCOL_INDICE, i,
				   -1);
		g_free(pie);
		g_free(markup);
	}
}

static void
seleccionar_plan_en_lista(const PL_PLAN *plan)
{
	int i;
	if (!plan)
		return;
	for (i = 0; i < main_planes_cuantos(); ++i) {
		if (main_planes_get(i) == plan) {
			GtkTreePath *path =
			    gtk_tree_path_new_from_indices(i, -1);
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(ui->tree_planes),
						 path, NULL, FALSE);
			gtk_tree_path_free(path);
			return;
		}
	}
}

/* --------------------------------------------------------------------
 * Señales
 * ------------------------------------------------------------------ */

static void
on_plan_seleccionado(GtkTreeSelection *sel, gpointer datos)
{
	GtkTreeModel *modelo;
	GtkTreeIter iter;
	gint indice;

	(void)datos;
	if (!gtk_tree_selection_get_selected(sel, &modelo, &iter))
		return;
	gtk_tree_model_get(modelo, &iter, PCOL_INDICE, &indice, -1);
	ui->plan = main_planes_get(indice);
	llenar_dias(ui->plan);
	refrescar_cabecera();
	if (ui->plan)
		ir_a_dia(main_planes_dia_de_hoy(ui->plan), FALSE);
}

static void
on_dia_marcado(GtkCellRendererToggle *celda, gchar *ruta, gpointer datos)
{
	GtkTreeIter iter;
	gboolean hecho;
	gint dia;

	(void)celda;
	(void)datos;
	if (!ui->plan)
		return;
	if (!gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(ui->dias),
						 &iter, ruta))
		return;
	gtk_tree_model_get(GTK_TREE_MODEL(ui->dias), &iter,
			   DCOL_HECHO, &hecho, DCOL_NUMERO, &dia, -1);
	hecho = !hecho;
	main_planes_marcar(ui->plan, dia, hecho);
	/* Marcar un día también adopta el plan: para eso lo está usando. */
	main_planes_activar(ui->plan);
	guardar_ya();

	gtk_list_store_set(ui->dias, &iter, DCOL_HECHO, hecho, -1);
	/* El día que toca se movió, así que se repinta la negrita. */
	llenar_dias(ui->plan);
	refrescar_cabecera();
	refrescar_lista_planes();
	seleccionar_plan_en_lista(ui->plan);
}

static void
on_dia_activado(GtkTreeView *tree, GtkTreePath *ruta,
		GtkTreeViewColumn *columna, gpointer datos)
{
	GtkTreeIter iter;
	gint dia;

	(void)tree;
	(void)columna;
	(void)datos;
	if (!ui->plan)
		return;
	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(ui->dias), &iter, ruta))
		return;
	gtk_tree_model_get(GTK_TREE_MODEL(ui->dias), &iter,
			   DCOL_NUMERO, &dia, -1);
	abrir_dia(ui->plan, dia);
}

static void
on_empezar(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	if (!ui->plan)
		return;
	main_planes_activar(ui->plan);
	guardar_ya();
	refrescar_cabecera();
	refrescar_lista_planes();
	seleccionar_plan_en_lista(ui->plan);
	ir_a_dia(main_planes_dia_de_hoy(ui->plan), TRUE);
}

static void
on_hoy(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	if (!ui->plan)
		return;
	ir_a_dia(main_planes_dia_de_hoy(ui->plan), TRUE);
}

static void
on_reiniciar(GtkButton *boton, gpointer datos)
{
	gchar *pregunta;

	(void)boton;
	(void)datos;
	if (!ui->plan)
		return;
	pregunta = g_strdup_printf(_("¿Borrar el progreso de «%s» y empezar de cero?"),
				   _(ui->plan->nombre));
	if (gui_yes_no_dialog(pregunta, NULL)) {
		main_planes_reiniciar(ui->plan);
		guardar_ya();
		llenar_dias(ui->plan);
		refrescar_cabecera();
		refrescar_lista_planes();
		seleccionar_plan_en_lista(ui->plan);
		ir_a_dia(1, FALSE);
	}
	g_free(pregunta);
}

/* Deja el diálogo mirando al plan que se acaba de tocar. */
static void
volver_a(const PL_PLAN *plan)
{
	ui->plan = plan;
	refrescar_lista_planes();
	seleccionar_plan_en_lista(plan);
	llenar_dias(plan);
	refrescar_cabecera();
	if (plan)
		ir_a_dia(main_planes_dia_de_hoy(plan), FALSE);
}

static void
on_nuevo(GtkButton *boton, gpointer datos)
{
	const PL_PLAN *nuevo;

	(void)boton;
	(void)datos;
	nuevo = gui_plan_personal_dialog(GTK_WINDOW(ui->dialog), NULL);
	if (!nuevo)
		return;
	guardar_ya();
	volver_a(nuevo);
}

static void
on_editar(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	if (!ui->plan || !main_planes_es_personal(ui->plan))
		return;
	if (!gui_plan_personal_dialog(GTK_WINDOW(ui->dialog), ui->plan))
		return;
	guardar_ya();
	volver_a(ui->plan);
}

static void
on_borrar(GtkButton *boton, gpointer datos)
{
	gchar *pregunta;

	(void)boton;
	(void)datos;
	if (!ui->plan || !main_planes_es_personal(ui->plan))
		return;
	pregunta = g_strdup_printf(_("¿Borrar el plan «%s» y lo que llevas leído de él?"),
				   _(ui->plan->nombre));
	if (gui_yes_no_dialog(pregunta, NULL)) {
		main_planes_personal_borrar(ui->plan);
		guardar_ya();
		volver_a(main_planes_get(0));
	}
	g_free(pregunta);
}

/* Recuperar los días perdidos. Las dos salidas honradas: dar por leído
 * lo atrasado, o correr el calendario y seguir desde hoy sin marcar
 * nada. Marcar por el lector lo que no ha leído sería mentirle, y
 * dejarle el plan roto para siempre, también. */
static void
on_al_dia(GtkButton *boton, gpointer datos)
{
	const PL_PLAN *plan = ui->plan;
	GtkWidget *dlg;
	gchar *pregunta;
	int atraso, hoy, calendario;
	gint respuesta;

	(void)boton;
	(void)datos;
	if (!plan)
		return;
	atraso = main_planes_dias_atrasados(plan);
	if (atraso < 1)
		return;
	hoy = main_planes_dia_de_hoy(plan);
	calendario = main_planes_dia_segun_calendario(plan);

	pregunta = g_strdup_printf(ngettext("Vas %d día por detrás en «%s».",
					    "Vas %d días por detrás en «%s».",
					    atraso),
				   atraso, _(plan->nombre));
	dlg = gtk_message_dialog_new(GTK_WINDOW(ui->dialog),
				     GTK_DIALOG_MODAL |
					 GTK_DIALOG_DESTROY_WITH_PARENT,
				     GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
				     "%s", pregunta);
	gtk_message_dialog_format_secondary_text(
	    GTK_MESSAGE_DIALOG(dlg),
	    _("Del día %d al %d están sin marcar.\n\n"
	      "Darlos por leídos: quedan marcados y sigues por el día %d.\n\n"
	      "Correr el calendario: no se marca nada; el plan cuenta como "
	      "si lo hubieras empezado más tarde y vuelves a ir al día por "
	      "el %d."),
	    hoy, calendario, calendario + 1, hoy);
	gtk_dialog_add_button(GTK_DIALOG(dlg), _("Cancelar"),
			      GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button(GTK_DIALOG(dlg), _("Correr el calendario"), 2);
	gtk_dialog_add_button(GTK_DIALOG(dlg), _("Darlos por leídos"), 1);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), 2);
	respuesta = gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);
	g_free(pregunta);

	if (respuesta == 1)
		main_planes_marcar_hasta(plan, calendario);
	else if (respuesta == 2)
		main_planes_reprogramar(plan);
	else
		return;

	guardar_ya();
	llenar_dias(plan);
	refrescar_cabecera();
	refrescar_lista_planes();
	seleccionar_plan_en_lista(plan);
	ir_a_dia(main_planes_dia_de_hoy(plan), FALSE);
}

static void
on_progreso(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	gui_progreso_lectura_dialog(GTK_WINDOW(ui->dialog));
}

/* --------------------------------------------------------------------
 * El recordatorio diario
 * ------------------------------------------------------------------ */

/* Las horas se escriben con dos cifras: "07:05", no "7:5". */
static gboolean
on_dos_cifras(GtkSpinButton *spin, gpointer datos)
{
	gchar *texto;

	(void)datos;
	texto = g_strdup_printf("%02d",
				gtk_spin_button_get_value_as_int(spin));
	gtk_entry_set_text(GTK_ENTRY(spin), texto);
	g_free(texto);
	return TRUE;
}

static void
refrescar_recordatorio(void)
{
	gboolean activo = gtk_toggle_button_get_active(
	    GTK_TOGGLE_BUTTON(ui->chk_recordatorio));
	gchar *texto;

	gtk_widget_set_sensitive(ui->spin_hora, activo);
	gtk_widget_set_sensitive(ui->spin_minuto, activo);
	gtk_widget_set_sensitive(ui->btn_probar, activo);

	if (activo)
		texto = g_markup_printf_escaped(
		    "<span size='small' alpha='70%%'>%s</span>",
		    _("Aviso del escritorio, solo en este equipo. Llega "
		      "aunque Biblia Elim esté cerrada. Si ya marcaste la "
		      "lectura del día, no avisa."));
	else
		texto = g_strdup("");
	gtk_label_set_markup(GTK_LABEL(ui->lbl_recordatorio), texto);
	g_free(texto);
}

static void
guardar_recordatorio(void)
{
	if (ui->poniendo_hora)
		return;
	main_planes_recordatorio_poner(
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->chk_recordatorio)),
	    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ui->spin_hora)),
	    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(ui->spin_minuto)));
	guardar_ya();
	/* La otra mitad del recordatorio, la que avisa con la aplicación
	 * cerrada, vive en un temporizador de systemd: que se entere de la
	 * hora nueva. */
	gui_recordatorio_sincronizar_pronto();
	refrescar_recordatorio();
}

static void
on_recordatorio_cambia(GtkWidget *widget, gpointer datos)
{
	(void)widget;
	(void)datos;
	guardar_recordatorio();
}

static void
on_probar(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	gui_recordatorio_probar();
}

static void
on_cerrar(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	if (ui && ui->dialog)
		gtk_widget_destroy(ui->dialog);
}

static void
on_destroy(GtkWidget *widget, gpointer datos)
{
	(void)widget;
	(void)datos;
	if (!ui)
		return;
	if (ui->planes)
		g_object_unref(ui->planes);
	if (ui->dias)
		g_object_unref(ui->dias);
	g_free(ui);
	ui = NULL;
}

/* --------------------------------------------------------------------
 * Construcción
 * ------------------------------------------------------------------ */

static void
montar_arboles(void)
{
	GtkCellRenderer *celda;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;

	ui->planes = gtk_list_store_new(N_PCOLS, G_TYPE_STRING, G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(ui->tree_planes),
				GTK_TREE_MODEL(ui->planes));
	celda = gtk_cell_renderer_text_new();
	g_object_set(celda, "ypad", 4, NULL);
	col = gtk_tree_view_column_new_with_attributes(NULL, celda,
						       "markup", PCOL_MARCA,
						       NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(ui->tree_planes), col);

	ui->dias = gtk_list_store_new(N_DCOLS, G_TYPE_BOOLEAN, G_TYPE_STRING,
				      G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(ui->tree_dias),
				GTK_TREE_MODEL(ui->dias));

	celda = gtk_cell_renderer_toggle_new();
	g_signal_connect(celda, "toggled", G_CALLBACK(on_dia_marcado), NULL);
	col = gtk_tree_view_column_new_with_attributes(_("Leído"), celda,
						       "active", DCOL_HECHO,
						       NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(ui->tree_dias), col);

	celda = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Día"), celda,
						       "text", DCOL_DIA,
						       "weight", DCOL_PESO,
						       NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(ui->tree_dias), col);

	celda = gtk_cell_renderer_text_new();
	g_object_set(celda, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	col = gtk_tree_view_column_new_with_attributes(_("Lectura"), celda,
						       "text", DCOL_LECTURA,
						       "weight", DCOL_PESO,
						       NULL);
	gtk_tree_view_column_set_expand(col, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(ui->tree_dias), col);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->tree_planes));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_BROWSE);
	g_signal_connect(sel, "changed", G_CALLBACK(on_plan_seleccionado), NULL);
	g_signal_connect(ui->tree_dias, "row-activated",
			 G_CALLBACK(on_dia_activado), NULL);
}

static void
crear_dialogo(void)
{
	GtkBuilder *gxml = elim_gtk_builder_new();
	GtkWidget *btn_cerrar;
	const char *activo;
	const PL_PLAN *inicial;

	if (!gtk_builder_add_from_resource(gxml,
					   "/org/xiphos/ui/planes-lectura.gtkbuilder",
					   NULL)) {
		g_object_unref(gxml);
		gui_generic_warning(_("No se pudo abrir el diálogo Planes de lectura."));
		return;
	}

	ui = g_new0(PLANES_UI, 1);
	ui->dialog = UI_GET_ITEM(gxml, "dialog_planes");
	ui->tree_planes = UI_GET_ITEM(gxml, "tree_planes");
	ui->tree_dias = UI_GET_ITEM(gxml, "tree_dias");
	ui->lbl_nombre = UI_GET_ITEM(gxml, "lbl_nombre");
	ui->lbl_desc = UI_GET_ITEM(gxml, "lbl_desc");
	ui->lbl_estado = UI_GET_ITEM(gxml, "lbl_estado");
	ui->barra = UI_GET_ITEM(gxml, "barra");
	ui->btn_empezar = UI_GET_ITEM(gxml, "btn_empezar");
	ui->btn_hoy = UI_GET_ITEM(gxml, "btn_hoy");
	ui->btn_reiniciar = UI_GET_ITEM(gxml, "btn_reiniciar");
	ui->btn_al_dia = UI_GET_ITEM(gxml, "btn_al_dia");
	ui->btn_progreso = UI_GET_ITEM(gxml, "btn_progreso");
	ui->btn_nuevo = UI_GET_ITEM(gxml, "btn_nuevo");
	ui->btn_editar = UI_GET_ITEM(gxml, "btn_editar");
	ui->btn_borrar = UI_GET_ITEM(gxml, "btn_borrar");
	ui->chk_recordatorio = UI_GET_ITEM(gxml, "chk_recordatorio");
	ui->spin_hora = UI_GET_ITEM(gxml, "spin_hora");
	ui->spin_minuto = UI_GET_ITEM(gxml, "spin_minuto");
	ui->btn_probar = UI_GET_ITEM(gxml, "btn_probar");
	ui->lbl_recordatorio = UI_GET_ITEM(gxml, "lbl_recordatorio");
	btn_cerrar = UI_GET_ITEM(gxml, "btn_cerrar");

	gui_prepare_floating_dialog(GTK_WINDOW(ui->dialog),
				    widgets.app ? GTK_WINDOW(widgets.app) : NULL);

	montar_arboles();

	g_signal_connect(ui->btn_empezar, "clicked", G_CALLBACK(on_empezar), NULL);
	g_signal_connect(ui->btn_hoy, "clicked", G_CALLBACK(on_hoy), NULL);
	g_signal_connect(ui->btn_reiniciar, "clicked", G_CALLBACK(on_reiniciar), NULL);
	g_signal_connect(ui->btn_al_dia, "clicked", G_CALLBACK(on_al_dia), NULL);
	g_signal_connect(ui->btn_progreso, "clicked", G_CALLBACK(on_progreso), NULL);
	g_signal_connect(ui->btn_nuevo, "clicked", G_CALLBACK(on_nuevo), NULL);
	g_signal_connect(ui->btn_editar, "clicked", G_CALLBACK(on_editar), NULL);
	g_signal_connect(ui->btn_borrar, "clicked", G_CALLBACK(on_borrar), NULL);
	g_signal_connect(btn_cerrar, "clicked", G_CALLBACK(on_cerrar), NULL);

	{
		int hora = 7, minuto = 0;
		gboolean activo = main_planes_recordatorio(&hora, &minuto);

		ui->poniendo_hora = TRUE;
		gtk_toggle_button_set_active(
		    GTK_TOGGLE_BUTTON(ui->chk_recordatorio), activo);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spin_hora), hora);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui->spin_minuto),
					  minuto);
		ui->poniendo_hora = FALSE;
	}
	g_signal_connect(ui->spin_hora, "output",
			 G_CALLBACK(on_dos_cifras), NULL);
	g_signal_connect(ui->spin_minuto, "output",
			 G_CALLBACK(on_dos_cifras), NULL);
	g_signal_connect(ui->chk_recordatorio, "toggled",
			 G_CALLBACK(on_recordatorio_cambia), NULL);
	g_signal_connect(ui->spin_hora, "value-changed",
			 G_CALLBACK(on_recordatorio_cambia), NULL);
	g_signal_connect(ui->spin_minuto, "value-changed",
			 G_CALLBACK(on_recordatorio_cambia), NULL);
	g_signal_connect(ui->btn_probar, "clicked", G_CALLBACK(on_probar), NULL);
	refrescar_recordatorio();
	g_signal_connect(ui->dialog, "destroy", G_CALLBACK(on_destroy), NULL);

	refrescar_lista_planes();

	/* Se abre en el plan que el lector tiene en curso; si no tiene
	 * ninguno, en el primero de la lista. */
	activo = main_planes_activo();
	inicial = activo ? main_planes_por_id(activo) : NULL;
	seleccionar_plan_en_lista(inicial ? inicial : main_planes_get(0));

	g_object_unref(gxml);
}

void
gui_planes_lectura_dialog(void)
{
	if (ui && ui->dialog) {
		gtk_window_present(GTK_WINDOW(ui->dialog));
		return;
	}
	crear_dialogo();
	if (ui && ui->dialog)
		gtk_widget_show(ui->dialog);
}
