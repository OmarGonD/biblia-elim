#include "main/font_size.h"

#include <stdlib.h>
#include <glib.h>

int
mod_font_size_steps(const char *fontsize, int base_font_size)
{
	int n;

	if (!fontsize || !*fontsize)
		return base_font_size;
	n = atoi(fontsize);
	if ((fontsize[0] != '+') && (fontsize[0] != '-') && (n >= 8))
		n -= 12;
	return n + base_font_size;
}

int
bible_body_font_size_steps(const char *default_fontsize, int base_font_size)
{
	return mod_font_size_steps(
	    (default_fontsize && *default_fontsize) ? default_fontsize : "+0",
	    base_font_size);
}

double
html_font_size_scale(const char *size)
{
	static const double html[7] = { 0.63, 0.82, 1.0, 1.13, 1.5, 2.0, 3.0 };
	int n;

	if (!size || !*size)
		return 1.0;
	n = atoi(size);
	if (size[0] == '+' || size[0] == '-')
		return CLAMP(1.0 + (n / 12.0), 0.5, 3.0);
	if (n >= 1 && n <= 7)
		return html[n - 1];
	return 1.0;
}
