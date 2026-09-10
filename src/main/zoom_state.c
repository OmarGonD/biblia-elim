#include "main/zoom_state.h"

#include <errno.h>

static const gchar *const surface_names[ZOOM_SURFACE_COUNT] = {
	"bible-main",
	"bible-parallel",
	"bible-compare",
	"commentary",
	"dictionary",
	"sidebar-previewer",
	"lower-previewer",
	"general-book",
	"devotional",
	"search-previewer"
};

static gboolean
valid_surface(ZoomSurface surface)
{
	return surface >= 0 && surface < ZOOM_SURFACE_COUNT;
}

void
zoom_state_init(ZoomState *state)
{
	gint i;

	g_return_if_fail(state != NULL);
	for (i = 0; i < ZOOM_SURFACE_COUNT; i++)
		state->percent[i] = ZOOM_PERCENT_DEFAULT;
	state->active = ZOOM_SURFACE_BIBLE_MAIN;
}

const gchar *
zoom_surface_name(ZoomSurface surface)
{
	return valid_surface(surface) ? surface_names[surface] : NULL;
}

ZoomSurface
zoom_surface_from_name(const gchar *name)
{
	gint i;

	if (!name)
		return ZOOM_SURFACE_INVALID;
	for (i = 0; i < ZOOM_SURFACE_COUNT; i++)
		if (g_str_equal(name, surface_names[i]))
			return (ZoomSurface)i;
	return ZOOM_SURFACE_INVALID;
}

gint
zoom_state_get(const ZoomState *state, ZoomSurface surface)
{
	if (!state || !valid_surface(surface))
		return ZOOM_PERCENT_DEFAULT;
	if (state->percent[surface] < ZOOM_PERCENT_MIN ||
	    state->percent[surface] > ZOOM_PERCENT_MAX)
		return ZOOM_PERCENT_DEFAULT;
	return state->percent[surface];
}

gint
zoom_state_set(ZoomState *state, ZoomSurface surface, gint percent)
{
	g_return_val_if_fail(state != NULL, ZOOM_PERCENT_DEFAULT);
	if (!valid_surface(surface))
		return ZOOM_PERCENT_DEFAULT;
	state->percent[surface] = CLAMP(percent, ZOOM_PERCENT_MIN,
					ZOOM_PERCENT_MAX);
	return state->percent[surface];
}

gint
zoom_state_adjust(ZoomState *state, ZoomSurface surface, gint delta)
{
	if (!state || !valid_surface(surface))
		return ZOOM_PERCENT_DEFAULT;
	return zoom_state_set(state, surface,
			      zoom_state_get(state, surface) + delta);
}

void
zoom_state_set_active(ZoomState *state, ZoomSurface surface)
{
	if (state && valid_surface(surface))
		state->active = surface;
}

ZoomSurface
zoom_state_active(const ZoomState *state)
{
	return state && valid_surface(state->active) ? state->active
						     : ZOOM_SURFACE_BIBLE_MAIN;
}

gchar *
zoom_state_serialize(const ZoomState *state)
{
	GString *serialized;
	gint i;

	g_return_val_if_fail(state != NULL, g_strdup(""));
	serialized = g_string_sized_new(256);
	for (i = 0; i < ZOOM_SURFACE_COUNT; i++) {
		if (i)
			g_string_append_c(serialized, ';');
		g_string_append_printf(serialized, "%s=%d", surface_names[i],
				       zoom_state_get(state, (ZoomSurface)i));
	}
	return g_string_free(serialized, FALSE);
}

void
zoom_state_deserialize(ZoomState *state, const gchar *serialized)
{
	gchar **items;
	gint i;

	g_return_if_fail(state != NULL);
	zoom_state_init(state);
	if (!serialized || !*serialized)
		return;

	items = g_strsplit(serialized, ";", -1);
	for (i = 0; items[i]; i++) {
		gchar *equals = strchr(items[i], '=');
		ZoomSurface surface;
		gchar *end = NULL;
		gint64 value;

		if (!equals || equals == items[i] || !equals[1])
			continue;
		*equals = '\0';
		surface = zoom_surface_from_name(items[i]);
		if (surface == ZOOM_SURFACE_INVALID)
			continue;
		errno = 0;
		value = g_ascii_strtoll(equals + 1, &end, 10);
		if (errno || end == equals + 1 || *end != '\0')
			continue;
		zoom_state_set(state, surface,
			       value < G_MININT ? G_MININT :
			       value > G_MAXINT ? G_MAXINT : (gint)value);
	}
	g_strfreev(items);
}
