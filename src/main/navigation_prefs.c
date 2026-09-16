/*
 * navigation_prefs.c - the reader's wheel and reading-focus preferences
 *
 * Policy only; reading and writing settings.xml is in
 * navigation_prefs_settings.c.
 */

#include "main/navigation_prefs.h"
#include "main/wheel_scroll.h"

#include <string.h>

NavigationPrefs
navigation_prefs_defaults(void)
{
	NavigationPrefs prefs = { NAVIGATION_PREFS_WHEEL_PERCENT_DEFAULT,
				  READING_FOCUS_BALANCED };
	return prefs;
}

gint
navigation_prefs_snap_percent(gdouble percent)
{
	gint step = NAVIGATION_PREFS_WHEEL_PERCENT_STEP;
	gint snapped = (gint)(percent / step + 0.5) * step;

	return CLAMP(snapped, NAVIGATION_PREFS_WHEEL_PERCENT_MIN,
		     NAVIGATION_PREFS_WHEEL_PERCENT_MAX);
}

const char *
navigation_prefs_focus_mode_name(ReadingFocusMode mode)
{
	switch (mode) {
	case READING_FOCUS_IMMEDIATE:
		return "immediate";
	case READING_FOCUS_STABLE:
		return "stable";
	case READING_FOCUS_BALANCED:
	default:
		return "balanced";
	}
}

NavigationPrefs
navigation_prefs_parse(const char *wheel_percent, const char *focus_mode)
{
	NavigationPrefs prefs = navigation_prefs_defaults();

	if (wheel_percent && *wheel_percent) {
		gchar *end;
		gint64 value = g_ascii_strtoll(wheel_percent, &end, 10);

		if (end != wheel_percent && *end == '\0')
			prefs.wheel_percent = navigation_prefs_snap_percent((gdouble)value);
	}
	if (focus_mode) {
		if (!strcmp(focus_mode, "immediate"))
			prefs.focus_mode = READING_FOCUS_IMMEDIATE;
		else if (!strcmp(focus_mode, "stable"))
			prefs.focus_mode = READING_FOCUS_STABLE;
	}
	return prefs;
}

void
navigation_prefs_apply(const NavigationPrefs *prefs)
{
	wheel_scroll_set_distance_scale(
	    navigation_prefs_snap_percent(prefs->wheel_percent) / 100.0);
	reading_focus_set_mode(prefs->focus_mode);
}

NavigationPrefs
navigation_prefs_current(void)
{
	NavigationPrefs prefs;

	prefs.wheel_percent = navigation_prefs_snap_percent(
	    wheel_scroll_get_distance_scale() * 100.0);
	prefs.focus_mode = reading_focus_get_mode();
	return prefs;
}
