/*
 * Shared bible font-size steps and HTML <font size> scale.
 * Used by get_font() / display.cc emitters and WkHtml.
 */
#ifndef BIBLIA_ELIM_FONT_SIZE_H
#define BIBLIA_ELIM_FONT_SIZE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Convert a fonts.conf Fontsize string into the relative step that
 * display.cc emits as <font size="%+d">.
 *
 * Signed values ("+2", "-1") are steps already. Unsigned values >= 8 are
 * legacy absolute point sizes from the old reading-font writer; they are
 * converted against a 12pt reference (the same base font_size_scale uses
 * for one step = one point).
 */
int mod_font_size_steps(const char *fontsize, int base_font_size);

/*
 * Common Bible body size step for a given Default Fontsize string and
 * global base bias. Per-module Fontsize is intentionally not an argument:
 * "Biblia principal - N%" must mean the same effective body size for every
 * Bible module; only the family may stay per-module.
 */
int bible_body_font_size_steps(const char *default_fontsize,
			       int base_font_size);

/*
 * Scale factor for an HTML <font size="..."> attribute.
 * Signed: relative steps over a 12pt conceptual base.
 * Unsigned 1..7: classic HTML absolute sizes (3 = normal).
 */
double html_font_size_scale(const char *size);

#ifdef __cplusplus
}
#endif
#endif
