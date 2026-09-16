/*
 * navigation_prefs.h - the reader's wheel and reading-focus preferences
 *
 * Two settings from Ver > Navegación y rueda, between the dialog and the
 * modules that use them:
 *
 *   wheel_scroll_percent  how far a mouse wheel notch moves the page, as a
 *                         percentage of GTK's own distance (wheel_scroll.h):
 *                         25-100 in steps of 5, 50 by default;
 *   reading_focus_mode    how readily the focused verse changes while
 *                         scrolling (reading_focus.h): immediate, balanced
 *                         (default) or stable.
 *
 * Stored under <misc> in settings.xml. Missing or unreadable values mean the
 * defaults, which are exactly how the pane behaved before the settings
 * existed. The percentage is stored as an integer: a decimal would be
 * written with the locale's comma.
 */
#ifndef XIPHOS_NAVIGATION_PREFS_H
#define XIPHOS_NAVIGATION_PREFS_H

#include <glib.h>

#include "main/reading_focus.h"

G_BEGIN_DECLS

#define NAVIGATION_PREFS_WHEEL_PERCENT_MIN 25
#define NAVIGATION_PREFS_WHEEL_PERCENT_MAX 100
#define NAVIGATION_PREFS_WHEEL_PERCENT_STEP 5
#define NAVIGATION_PREFS_WHEEL_PERCENT_DEFAULT 50

/* settings.xml, section "misc" */
#define NAVIGATION_PREFS_KEY_WHEEL_PERCENT "wheel_scroll_percent"
#define NAVIGATION_PREFS_KEY_FOCUS_MODE "reading_focus_mode"

typedef struct {
	gint wheel_percent;
	ReadingFocusMode focus_mode;
} NavigationPrefs;

NavigationPrefs navigation_prefs_defaults(void);

/* A percentage brought into 25..100 and onto a 5 % step. */
gint navigation_prefs_snap_percent(gdouble percent);

/* "immediate" / "balanced" / "stable" */
const char *navigation_prefs_focus_mode_name(ReadingFocusMode mode);

/* Stored strings to preferences; NULL or unknown values give the default
 * for that setting, numbers are snapped. */
NavigationPrefs navigation_prefs_parse(const char *wheel_percent,
				       const char *focus_mode);

/* Makes the modules use `prefs` from the next wheel event / focus update. */
void navigation_prefs_apply(const NavigationPrefs *prefs);

/* What the modules use now. */
NavigationPrefs navigation_prefs_current(void);

/* From the loaded settings document (xml_get_value), then applied. Nothing
 * is written when the settings are missing. */
NavigationPrefs navigation_prefs_load_settings(void);

/* Into the loaded settings document, then written to `settings_file`
 * (settings.fnconfigure in the app). */
void navigation_prefs_save_settings(const NavigationPrefs *prefs,
				    const char *settings_file);

G_END_DECLS

#endif /* XIPHOS_NAVIGATION_PREFS_H */
