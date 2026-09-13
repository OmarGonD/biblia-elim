/* Pure layout policy for the main commentary/dictionary GtkPaned. */

#ifndef BIBLIA_ELIM_MAIN_WINDOW_LAYOUT_H
#define BIBLIA_ELIM_MAIN_WINDOW_LAYOUT_H

#include <glib.h>

typedef struct {
	gboolean pane_visible;
	gboolean commentary_visible;
	gboolean dictionary_visible;
	gboolean set_divider_position;
	gint divider_position;
} MainStudyPaneLayout;

typedef struct {
	gboolean tabstrip_visible;
	gboolean navbar_visible;
	gboolean text_pane_visible;
	gboolean lower_previewer_visible;
	gboolean commentary_visible;
	gboolean dictionary_visible;
	gboolean study_pane_visible;
	gboolean compare_visible;
	gboolean statusbar_visible;
	gboolean header_menu_visible;
	gboolean interlinear_bar_visible;
} MainStartupVisibility;

MainStudyPaneLayout main_study_pane_layout(gboolean show_commentary,
					   gboolean show_dictionary,
					   gint divider_position);

MainStartupVisibility main_startup_visibility(gboolean browsing,
					      gboolean show_texts,
					      gboolean show_preview,
					      gboolean preview_in_sidebar,
					      gboolean show_commentary,
					      gboolean show_dictionary,
					      gboolean show_compare,
					      gboolean show_statusbar,
					      gboolean reading_mode);

/* Width the Bible/study splitter should treat as its allocation. An
 * unrealized GtkPaned reports 1; fall back to the saved window width. */
gint main_study_hpaned_available_width(gint allocated_hpaned,
				       gint window_width);

/* Final GtkPaned position for the Bible/study splitter. Matches the last
 * assignment performed by gui_set_bible_comm_layout() after its historical
 * intermediate writes. available_width is the splitter's own allocation,
 * not the toplevel window. */
gint main_study_hpaned_position(gboolean show_commentary,
				gboolean show_dictionary,
				gint biblepane_width,
				gint available_width);

/* Standard-view reading column: a capped width centred in the Bible
 * pane's actual allocation. Verses stay left-aligned inside the column;
 * only the block is centred. Parallel view does not use this. */
#define STUDY_READING_COLUMN_MAX 950
#define STUDY_READING_COLUMN_PAD 14

typedef struct {
	gint left_margin;
	gint right_margin;
	gint column_width;
} StudyReadingColumn;

StudyReadingColumn main_study_reading_column(gint available_width,
					     gint max_width,
					     gint min_pad);

#endif
