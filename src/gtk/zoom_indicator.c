#include "zoom_indicator.h"

#include <glib/gi18n.h>

static const gchar *const display_names[ZOOM_SURFACE_COUNT] = {
	N_("Biblia principal"),
	N_("Biblia paralela"),
	N_("Comparación bíblica"),
	N_("Comentario"),
	N_("Diccionario"),
	N_("Vista previa lateral"),
	N_("Vista previa inferior"),
	N_("Libro general"),
	N_("Devocional"),
	N_("Vista previa de búsqueda")
};

const gchar *
zoom_indicator_surface_name(ZoomSurface surface)
{
	if (surface < 0 || surface >= ZOOM_SURFACE_COUNT)
		return display_names[ZOOM_SURFACE_BIBLE_MAIN];
	return display_names[surface];
}

gchar *
zoom_indicator_format(const ZoomState *state,
		      const gchar *translated_surface_name)
{
	ZoomSurface surface = zoom_state_active(state);
	const gchar *name = translated_surface_name;

	if (!name || !*name)
		name = zoom_indicator_surface_name(surface);
	return g_strdup_printf("%s · %d%%", name,
			       zoom_state_get(state, surface));
}
