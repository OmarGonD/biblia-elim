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
expect_column(const char *name, gint available, gint expect_width,
	      gint expect_left, gint expect_right)
{
	StudyReadingColumn col = main_study_reading_column(
	    available, STUDY_READING_COLUMN_MAX, STUDY_READING_COLUMN_PAD);

	if (col.column_width != expect_width ||
	    col.left_margin != expect_left ||
	    col.right_margin != expect_right) {
		fprintf(stderr,
			"%s: column=%d left=%d right=%d expected %d/%d/%d\n",
			name, col.column_width, col.left_margin,
			col.right_margin, expect_width, expect_left,
			expect_right);
		failures++;
	}
	if (available > 0 &&
	    col.left_margin + col.column_width + col.right_margin !=
		available) {
		fprintf(stderr, "%s: margins+column=%d available=%d\n", name,
			col.left_margin + col.column_width + col.right_margin,
			available);
		failures++;
	}
}

static void
expect_reading_column(void)
{
	StudyReadingColumn wide;
	StudyReadingColumn narrow;
	StudyReadingColumn mid;

	/* Full window, no side panels: 950px column centred. */
	expect_column("no panels 1800", 1800, 950, 425, 425);
	/* Right (or left) panel leaves 800px: fill with padding, do not
	 * keep a stale 425px left margin from the previous state. */
	expect_column("one panel 800", 800, 772, 14, 14);
	/* Both panels: a tight centre still uses min padding. */
	expect_column("both panels 600", 600, 572, 14, 14);
	/* Just above the cap: leftover splits evenly. */
	expect_column("cap plus padding", 978, 950, 14, 14);
	expect_column("cap plus extra", 980, 950, 15, 15);
	expect_column("odd leftover", 981, 950, 15, 16);
	expect_column("unallocated", 0, 0, 14, 14);

	wide = main_study_reading_column(1800, STUDY_READING_COLUMN_MAX,
					 STUDY_READING_COLUMN_PAD);
	narrow = main_study_reading_column(800, STUDY_READING_COLUMN_MAX,
					   STUDY_READING_COLUMN_PAD);
	mid = main_study_reading_column(1200, STUDY_READING_COLUMN_MAX,
					STUDY_READING_COLUMN_PAD);
	EXPECT_FIELD("wide margins equal", wide.left_margin == wide.right_margin,
		     TRUE);
	EXPECT_FIELD("closing a panel drops the old left offset",
		     wide.left_margin > narrow.left_margin, TRUE);
	EXPECT_FIELD("mid column still capped", mid.column_width,
		     STUDY_READING_COLUMN_MAX);
	EXPECT_FIELD("narrow column uses leftover", narrow.column_width,
		     800 - 2 * STUDY_READING_COLUMN_PAD);
}

static void
expect_hpaned_position(void)
{
	EXPECT_FIELD("allocated splitter wins",
		     main_study_hpaned_available_width(800, 960), 800);
	EXPECT_FIELD("unrealized splitter falls back to window",
		     main_study_hpaned_available_width(1, 960), 960);
	EXPECT_FIELD("zero allocation falls back to window",
		     main_study_hpaned_available_width(0, 960), 960);
	EXPECT_FIELD("study split",
		     main_study_hpaned_position(TRUE, FALSE, 420, 800), 420);
	EXPECT_FIELD("dictionary split",
		     main_study_hpaned_position(FALSE, TRUE, 420, 800), 420);
	EXPECT_FIELD("both study panes",
		     main_study_hpaned_position(TRUE, TRUE, 420, 800), 420);
	EXPECT_FIELD("bible only uses splitter allocation",
		     main_study_hpaned_position(FALSE, FALSE, 420, 800), 800);
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
	expect_reading_column();

	printf("main_window_layout_failures=%d\n", failures);
	return failures ? 1 : 0;
}
