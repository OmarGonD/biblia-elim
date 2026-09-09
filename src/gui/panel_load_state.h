/*
 * Shared lifecycle for independently loaded content panels.
 *
 * This model deliberately has no GTK, renderer, or Bible backend dependency.
 * Presentation code owns placeholders and error messages; content producers
 * only move a panel between these states.
 */

#ifndef PANEL_LOAD_STATE_H
#define PANEL_LOAD_STATE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	PANEL_LOAD_EMPTY = 0,
	PANEL_LOAD_LOADING,
	PANEL_LOAD_READY,
	PANEL_LOAD_ERROR
} PanelLoadState;

typedef uint64_t PanelLoadToken;

typedef struct {
	PanelLoadState state;
	PanelLoadToken current_token;
	/* A ready document remains present while a replacement is loading. */
	bool content_available;
} PanelLoadModel;

void panel_load_model_init(PanelLoadModel *model);
PanelLoadToken panel_load_model_begin(PanelLoadModel *model);
void panel_load_model_clear(PanelLoadModel *model);
bool panel_load_model_ready(PanelLoadModel *model, PanelLoadToken token);
bool panel_load_model_error(PanelLoadModel *model, PanelLoadToken token);
bool panel_load_model_content_available(const PanelLoadModel *model);
const char *panel_load_state_name(PanelLoadState state);

/* Opt-in startup/navigation diagnostics.  This is deliberately shared with
 * the existing panel lifecycle logger so all timestamps use one origin. */
bool panel_load_debug_enabled(void);
void panel_load_debug(const char *surface, const char *event,
			      const char *detail);

#ifdef __cplusplus
}
#endif

#endif /* PANEL_LOAD_STATE_H */
