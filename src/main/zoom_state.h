/* Persistent, source-neutral identities for independently zoomable UI panes. */
#ifndef BIBLIA_ELIM_ZOOM_STATE_H
#define BIBLIA_ELIM_ZOOM_STATE_H

#include <glib.h>

G_BEGIN_DECLS

#define ZOOM_PERCENT_DEFAULT 100
#define ZOOM_PERCENT_MIN 50
#define ZOOM_PERCENT_MAX 300
#define ZOOM_PERCENT_STEP 10

typedef enum {
	ZOOM_SURFACE_INVALID = -1,
	ZOOM_SURFACE_BIBLE_MAIN = 0,
	ZOOM_SURFACE_BIBLE_PARALLEL,
	ZOOM_SURFACE_BIBLE_COMPARE,
	ZOOM_SURFACE_COMMENTARY,
	ZOOM_SURFACE_DICTIONARY,
	ZOOM_SURFACE_SIDEBAR_PREVIEWER,
	ZOOM_SURFACE_LOWER_PREVIEWER,
	ZOOM_SURFACE_GENERAL_BOOK,
	ZOOM_SURFACE_DEVOTIONAL,
	ZOOM_SURFACE_SEARCH_PREVIEWER,
	ZOOM_SURFACE_COUNT
} ZoomSurface;

typedef struct {
	gint percent[ZOOM_SURFACE_COUNT];
	ZoomSurface active;
} ZoomState;

void zoom_state_init(ZoomState *state);
const gchar *zoom_surface_name(ZoomSurface surface);
ZoomSurface zoom_surface_from_name(const gchar *name);
gint zoom_state_get(const ZoomState *state, ZoomSurface surface);
gint zoom_state_set(ZoomState *state, ZoomSurface surface, gint percent);
gint zoom_state_adjust(ZoomState *state, ZoomSurface surface, gint delta);
void zoom_state_set_active(ZoomState *state, ZoomSurface surface);
ZoomSurface zoom_state_active(const ZoomState *state);
gchar *zoom_state_serialize(const ZoomState *state);
void zoom_state_deserialize(ZoomState *state, const gchar *serialized);

G_END_DECLS

#endif
