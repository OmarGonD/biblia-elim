#include <glib.h>
#include <stdio.h>

#include "main_window_layout.h"

static int failures;

#define EXPECT_FIELD(name, actual, expected)                                  \
	do {                                                                     \
		if ((actual) != (expected)) {                                      \
			fprintf(stderr, "%s: %s=%d expected=%d\n", (name),         \
				#actual, (actual), (expected));                         \
			failures++;                                                   \
		}                                                                \
	} while (0)

static void
expect_layout(const char *name, gboolean comments, gboolean dictionary,
	      gboolean pane_visible, gboolean set_divider)
{
	MainStudyPaneLayout layout =
	    main_study_pane_layout(comments, dictionary, 237);

	if (layout.pane_visible != pane_visible ||
	    layout.commentary_visible != comments ||
	    layout.dictionary_visible != dictionary ||
	    layout.set_divider_position != set_divider ||
	    layout.divider_position != 237) {
		fprintf(stderr,
			"%s: pane=%d comments=%d dictionary=%d divider=%d "
			"position=%d\n",
			name, layout.pane_visible, layout.commentary_visible,
			layout.dictionary_visible, layout.set_divider_position,
			layout.divider_position);
		failures++;
	}
}

static void
expect_default_startup(void)
{
	MainStartupVisibility visibility = main_startup_visibility(
	    FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE);

	EXPECT_FIELD("default startup", visibility.tabstrip_visible, FALSE);
	EXPECT_FIELD("default startup", visibility.navbar_visible, TRUE);
	EXPECT_FIELD("default startup", visibility.text_pane_visible, TRUE);
	EXPECT_FIELD("default startup", visibility.lower_previewer_visible, FALSE);
	EXPECT_FIELD("default startup", visibility.commentary_visible, FALSE);
	EXPECT_FIELD("default startup", visibility.dictionary_visible, FALSE);
	EXPECT_FIELD("default startup", visibility.study_pane_visible, FALSE);
	EXPECT_FIELD("default startup", visibility.compare_visible, FALSE);
	EXPECT_FIELD("default startup", visibility.statusbar_visible, FALSE);
	EXPECT_FIELD("default startup", visibility.header_menu_visible, TRUE);
	EXPECT_FIELD("default startup", visibility.interlinear_bar_visible, TRUE);
}

static void
expect_all_visible_startup(void)
{
	MainStartupVisibility visibility = main_startup_visibility(
	    TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, FALSE);

	EXPECT_FIELD("all visible", visibility.tabstrip_visible, TRUE);
	EXPECT_FIELD("all visible", visibility.navbar_visible, TRUE);
	EXPECT_FIELD("all visible", visibility.text_pane_visible, TRUE);
	EXPECT_FIELD("all visible", visibility.lower_previewer_visible, TRUE);
	EXPECT_FIELD("all visible", visibility.commentary_visible, TRUE);
	EXPECT_FIELD("all visible", visibility.dictionary_visible, TRUE);
	EXPECT_FIELD("all visible", visibility.study_pane_visible, TRUE);
	EXPECT_FIELD("all visible", visibility.compare_visible, TRUE);
	EXPECT_FIELD("all visible", visibility.statusbar_visible, TRUE);
}

static void
expect_reading_startup(void)
{
	MainStartupVisibility visibility = main_startup_visibility(
	    TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE);

	EXPECT_FIELD("reading mode", visibility.text_pane_visible, TRUE);
	EXPECT_FIELD("reading mode", visibility.tabstrip_visible, FALSE);
	EXPECT_FIELD("reading mode", visibility.navbar_visible, FALSE);
	EXPECT_FIELD("reading mode", visibility.lower_previewer_visible, FALSE);
	EXPECT_FIELD("reading mode", visibility.study_pane_visible, FALSE);
	EXPECT_FIELD("reading mode", visibility.compare_visible, FALSE);
	EXPECT_FIELD("reading mode", visibility.statusbar_visible, FALSE);
	EXPECT_FIELD("reading mode", visibility.header_menu_visible, FALSE);
	EXPECT_FIELD("reading mode", visibility.interlinear_bar_visible, FALSE);
}

static void
expect_hpaned_position(void)
{
	EXPECT_FIELD("study split",
		     main_study_hpaned_position(TRUE, FALSE, 420, 960), 420);
	EXPECT_FIELD("dictionary split",
		     main_study_hpaned_position(FALSE, TRUE, 420, 960), 420);
	EXPECT_FIELD("both study panes",
		     main_study_hpaned_position(TRUE, TRUE, 420, 960), 420);
	EXPECT_FIELD("bible only uses window width",
		     main_study_hpaned_position(FALSE, FALSE, 420, 960), 960);
}

int
main(void)
{
	expect_layout("no study panes", FALSE, FALSE, FALSE, FALSE);
	expect_layout("commentary only", TRUE, FALSE, TRUE, FALSE);
	expect_layout("dictionary only", FALSE, TRUE, TRUE, FALSE);
	expect_layout("split study panes", TRUE, TRUE, TRUE, TRUE);
	expect_default_startup();
	expect_all_visible_startup();
	expect_reading_startup();
	expect_hpaned_position();

	printf("main_window_layout_failures=%d\n", failures);
	return failures ? 1 : 0;
}
