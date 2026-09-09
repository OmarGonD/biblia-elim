/*
 * Shared content-panel lifecycle.  Kept separate from the GTK presentation
 * wrapper so the state machine cannot acquire renderer or backend knowledge.
 */

#include "gui/panel_load_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

bool panel_load_debug_enabled(void)
{
	static int enabled = -1;
	if (enabled < 0)
		enabled = (getenv("BIBLIA_ELIM_UI_LOAD_DEBUG") &&
			    !strcmp(getenv("BIBLIA_ELIM_UI_LOAD_DEBUG"), "1"));
	return enabled != 0;
}

void panel_load_debug(const char *surface, const char *event,
			     const char *detail)
{
	static struct timespec origin;
	struct timespec now;
	double elapsed_ms;
	if (!panel_load_debug_enabled())
		return;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (!origin.tv_sec && !origin.tv_nsec)
		origin = now;
	elapsed_ms = (now.tv_sec - origin.tv_sec) * 1000.0 +
		(now.tv_nsec - origin.tv_nsec) / 1000000.0;
	fprintf(stderr, "[UI-LOAD] %s %s %.1fms%s%s\n",
		   surface ? surface : "app", event ? event : "EVENT",
		   elapsed_ms,
		   detail ? " " : "", detail ? detail : "");
}

static void invalidate_current_request(PanelLoadModel *model)
{
	model->current_token++;
	/* Zero is reserved for an uninitialised/no-request token. */
	if (model->current_token == 0)
		model->current_token = 1;
}

void panel_load_model_init(PanelLoadModel *model)
{
	if (!model)
		return;
	model->state = PANEL_LOAD_EMPTY;
	model->current_token = 0;
	model->content_available = false;
}

PanelLoadToken panel_load_model_begin(PanelLoadModel *model)
{
	if (!model)
		return 0;
	invalidate_current_request(model);
	model->state = PANEL_LOAD_LOADING;
	return model->current_token;
}

void panel_load_model_clear(PanelLoadModel *model)
{
	if (!model)
		return;
	invalidate_current_request(model);
	model->state = PANEL_LOAD_EMPTY;
	model->content_available = false;
}

static bool finish(PanelLoadModel *model, PanelLoadToken token,
		   PanelLoadState result)
{
	if (!model || token == 0 || token != model->current_token ||
	    model->state != PANEL_LOAD_LOADING)
		return false;
	model->state = result;
	if (result == PANEL_LOAD_READY)
		model->content_available = true;
	return true;
}

bool panel_load_model_ready(PanelLoadModel *model, PanelLoadToken token)
{
	return finish(model, token, PANEL_LOAD_READY);
}

bool panel_load_model_error(PanelLoadModel *model, PanelLoadToken token)
{
	return finish(model, token, PANEL_LOAD_ERROR);
}

bool panel_load_model_content_available(const PanelLoadModel *model)
{
	return model && model->content_available;
}

const char *panel_load_state_name(PanelLoadState state)
{
	switch (state) {
	case PANEL_LOAD_EMPTY:
		return "EMPTY";
	case PANEL_LOAD_LOADING:
		return "LOADING";
	case PANEL_LOAD_READY:
		return "READY";
	case PANEL_LOAD_ERROR:
		return "ERROR";
	default:
		return "UNKNOWN";
	}
}
