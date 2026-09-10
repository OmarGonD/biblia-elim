#ifndef BIBLIA_ELIM_ZOOM_INDICATOR_H
#define BIBLIA_ELIM_ZOOM_INDICATOR_H

#include <glib.h>

#include "main/zoom_state.h"

const gchar *zoom_indicator_surface_name(ZoomSurface surface);
gchar *zoom_indicator_format(const ZoomState *state,
			     const gchar *translated_surface_name);

#endif
