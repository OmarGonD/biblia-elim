#include "gui/panel_load_state.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                        \
	do {                                                                      \
		if (!(condition)) {                                                \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,    \
				#condition);                                        \
			failures++;                                               \
		}                                                                 \
	} while (0)

int main(void)
{
	PanelLoadModel model;
	PanelLoadModel other;
	PanelLoadToken first;
	PanelLoadToken second;

	panel_load_model_init(&model);
	CHECK(model.state == PANEL_LOAD_EMPTY);
	CHECK(model.current_token == 0);
	CHECK(!panel_load_model_content_available(&model));
	CHECK(strcmp(panel_load_state_name(model.state), "EMPTY") == 0);

	first = panel_load_model_begin(&model);
	CHECK(first != 0);
	CHECK(model.state == PANEL_LOAD_LOADING);
	CHECK(strcmp(panel_load_state_name(model.state), "LOADING") == 0);
	CHECK(panel_load_model_error(&model, first));
	CHECK(model.state == PANEL_LOAD_ERROR);
	CHECK(!panel_load_model_content_available(&model));
	first = panel_load_model_begin(&model);
	CHECK(panel_load_model_ready(&model, first));
	CHECK(model.state == PANEL_LOAD_READY);
	CHECK(panel_load_model_content_available(&model));

	second = panel_load_model_begin(&model);
	CHECK(second != 0 && second != first);
	CHECK(!panel_load_model_ready(&model, first));
	CHECK(model.state == PANEL_LOAD_LOADING);
	CHECK(panel_load_model_ready(&model, second));
	CHECK(model.state == PANEL_LOAD_READY);
	CHECK(panel_load_model_content_available(&model));
	CHECK(!panel_load_model_error(&model, second));

	/* A replacement request keeps the previous document visible. */
	second = panel_load_model_begin(&model);
	CHECK(model.state == PANEL_LOAD_LOADING);
	CHECK(panel_load_model_content_available(&model));
	CHECK(panel_load_model_error(&model, second));
	CHECK(model.state == PANEL_LOAD_ERROR);
	CHECK(panel_load_model_content_available(&model));
	CHECK(strcmp(panel_load_state_name(model.state), "ERROR") == 0);

	/* Each panel owns its lifecycle; one panel cannot gate another. */
	panel_load_model_init(&other);
	first = panel_load_model_begin(&other);
	second = panel_load_model_begin(&model);
	CHECK(panel_load_model_ready(&other, first));
	CHECK(other.state == PANEL_LOAD_READY);
	CHECK(model.state == PANEL_LOAD_LOADING);
	CHECK(panel_load_model_content_available(&other));
	CHECK(panel_load_model_content_available(&model));
	CHECK(panel_load_model_ready(&model, second));

	/* Rapid navigation accepts only the newest completion, including when
	 * stale success and failure notifications arrive in either order. */
	first = panel_load_model_begin(&model);
	second = panel_load_model_begin(&model);
	CHECK(!panel_load_model_error(&model, first));
	CHECK(!panel_load_model_ready(&model, first));
	CHECK(model.state == PANEL_LOAD_LOADING);
	CHECK(panel_load_model_error(&model, second));
	CHECK(model.state == PANEL_LOAD_ERROR);
	CHECK(panel_load_model_content_available(&model));
	first = panel_load_model_begin(&model);
	CHECK(panel_load_model_ready(&model, first));
	CHECK(model.state == PANEL_LOAD_READY);

	second = panel_load_model_begin(&model);
	panel_load_model_clear(&model);
	CHECK(model.state == PANEL_LOAD_EMPTY);
	CHECK(!panel_load_model_content_available(&model));
	CHECK(!panel_load_model_ready(&model, second));
	CHECK(!panel_load_model_error(&model, second));

	CHECK(panel_load_model_begin(NULL) == 0);
	CHECK(!panel_load_model_ready(NULL, 1));
	CHECK(!panel_load_model_error(NULL, 1));
	CHECK(!panel_load_model_content_available(NULL));
	CHECK(strcmp(panel_load_state_name((PanelLoadState)99), "UNKNOWN") == 0);

	printf("panel_load_state_failures=%d\n", failures);
	return failures ? 1 : 0;
}
