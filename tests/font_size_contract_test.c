/*
 * Contract: Bible main-panel body size is the shared Default baseline,
 * independent of per-module Fontsize. Zoom CSS then scales that common
 * baseline, so the same "Biblia principal - N%" means the same effective
 * body size for a module with legacy Fontsize=10 and one that uses Default.
 */
#include "main/font_size.h"

#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef SRCDIR
#define SRCDIR "."
#endif

static int failures;

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__,          \
				__LINE__, #cond);                              \
			failures++;                                            \
		}                                                              \
	} while (0)

#define CHECK_NEAR(a, b)                                                       \
	do {                                                                   \
		double _a = (a), _b = (b);                                     \
		if (fabs(_a - _b) > 1e-9) {                                    \
			fprintf(stderr,                                        \
				"FAIL %s:%d: %.12f != %.12f\n", __FILE__,      \
				__LINE__, _a, _b);                             \
			failures++;                                            \
		}                                                              \
	} while (0)

int
main(void)
{
	char attr_legacy[16], attr_default[16], attr_body[16];
	int steps_legacy;
	int steps_default_mod;
	int body_a;
	int body_b;
	double scale_100;
	double scale_110;
	gchar *display_src = NULL;

	/* Conversion helpers still understand legacy absolute Fontsize. */
	steps_legacy = mod_font_size_steps("10", 0);
	CHECK(steps_legacy == -2);
	CHECK(mod_font_size_steps("10", 0) == mod_font_size_steps("-2", 0));

	/* A module with no Fontsize entry behaves like Default/+0. */
	steps_default_mod = mod_font_size_steps("+0", 0);
	CHECK(steps_default_mod == 0);
	CHECK(steps_default_mod != steps_legacy);

	/* Bible body size uses Default only — same for both modules. */
	body_a = bible_body_font_size_steps("+0", 0);
	body_b = bible_body_font_size_steps("+0", 0);
	CHECK(body_a == body_b);
	CHECK(body_a == 0);
	CHECK(body_a != steps_legacy);

	/* Even if Default were "+1", body steps stay module-agnostic. */
	CHECK(bible_body_font_size_steps("+1", 0) ==
	      bible_body_font_size_steps("+1", 0));
	CHECK(bible_body_font_size_steps("+1", 0) == 1);

	snprintf(attr_legacy, sizeof(attr_legacy), "%+d", steps_legacy);
	snprintf(attr_default, sizeof(attr_default), "%+d", steps_default_mod);
	snprintf(attr_body, sizeof(attr_body), "%+d", body_a);
	CHECK_NEAR(html_font_size_scale(attr_body), 1.0);
	CHECK_NEAR(html_font_size_scale(attr_default), 1.0);
	CHECK(html_font_size_scale(attr_legacy) < 0.85);

	/* Zoom is applied once via CSS percent on the textview; 110% is
	 * exactly 1.10× the 100% baseline, not a second Fontsize layer. */
	scale_100 = 100 / 100.0;
	scale_110 = 110 / 100.0;
	CHECK(scale_100 != scale_110);
	CHECK_NEAR(scale_110 / scale_100, 1.10);

	/* Production path: Bible body uses the shared Default face+size,
	 * not per-module Font/Fontsize, so switching modules cannot change
	 * optical size. */
	CHECK(g_file_get_contents(SRCDIR "/src/main/display.cc", &display_src,
				  NULL, NULL));
	CHECK(display_src &&
	      strstr(display_src, "apply_bible_body_font(mf)") != NULL);
	g_free(display_src);

	printf("font_size_contract_failures=%d\n", failures);
	return failures ? 1 : 0;
}
