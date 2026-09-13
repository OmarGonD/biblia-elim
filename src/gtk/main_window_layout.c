/*
 * Keep the main study pane's visibility and divider policy independent from
 * GTK calls so every combination can be regression-tested without a display.
 */

#include "main_window_layout.h"

MainStudyPaneLayout main_study_pane_layout(gboolean show_commentary,
					   gboolean show_dictionary,
					   gint divider_position)
{
	MainStudyPaneLayout layout;

	layout.commentary_visible = show_commentary != FALSE;
	layout.dictionary_visible = show_dictionary != FALSE;
	layout.pane_visible = layout.commentary_visible || layout.dictionary_visible;
	/* A GtkPaned needs an explicit divider only while both children are
	 * visible. With one child, hiding the other is the layout contract: the
	 * remaining child receives the complete allocation automatically. */
	layout.set_divider_position =
	    layout.commentary_visible && layout.dictionary_visible;
	layout.divider_position = divider_position;
	return layout;
}

MainStartupVisibility
main_startup_visibility(gboolean browsing, gboolean show_texts,
			gboolean show_preview, gboolean preview_in_sidebar,
			gboolean show_commentary, gboolean show_dictionary,
			gboolean show_compare, gboolean show_statusbar,
			gboolean reading_mode)
{
	MainStartupVisibility visibility;
	MainStudyPaneLayout study =
	    main_study_pane_layout(show_commentary, show_dictionary, 0);

	visibility.tabstrip_visible = browsing && !reading_mode;
	visibility.navbar_visible =
	    (show_texts || show_commentary) && !reading_mode;
	/* vpaned owns both the Bible and lower previewer. The existing View
	 * semantics hide that complete left pane when Bible text is disabled. */
	visibility.text_pane_visible = show_texts != FALSE;
	visibility.lower_previewer_visible =
	    show_preview && !preview_in_sidebar && !reading_mode;
	visibility.commentary_visible =
	    study.commentary_visible && !reading_mode;
	visibility.dictionary_visible =
	    study.dictionary_visible && !reading_mode;
	visibility.study_pane_visible = study.pane_visible && !reading_mode;
	visibility.compare_visible = show_compare && !reading_mode;
	visibility.statusbar_visible = show_statusbar && !reading_mode;
	visibility.header_menu_visible = !reading_mode;
	visibility.interlinear_bar_visible = !reading_mode;
	return visibility;
}

gint
main_study_hpaned_available_width(gint allocated_hpaned, gint window_width)
{
	/* gtk_widget_get_allocated_width() returns 1 before the first
	 * size-allocate. That is not a usable splitter width. */
	if (allocated_hpaned > 1)
		return allocated_hpaned;
	return window_width;
}

gint
main_study_hpaned_position(gboolean show_commentary,
			   gboolean show_dictionary,
			   gint biblepane_width,
			   gint available_width)
{
	/* gui_set_bible_comm_layout() historically wrote the hpaned position
	 * up to three times. The surviving value depends only on whether any
	 * study child participates in the split. With no study child, pin
	 * the divider to the splitter allocation so the Bible pane receives
	 * the leftover width instead of a stale window-size value. */
	if (show_commentary || show_dictionary)
		return biblepane_width;
	return available_width;
}

StudyReadingColumn
main_study_reading_column(gint available_width, gint max_width, gint min_pad)
{
	StudyReadingColumn col = { 0, 0, 0 };
	gint pad, inner, leftover;

	if (max_width < 1)
		max_width = 1;
	if (min_pad < 0)
		min_pad = 0;
	if (available_width < 1) {
		col.left_margin = min_pad;
		col.right_margin = min_pad;
		return col;
	}

	pad = min_pad;
	if (available_width < 2 * pad + 1)
		pad = available_width / 3;

	inner = available_width - 2 * pad;
	if (inner < 1)
		inner = available_width;
	if (inner > max_width)
		inner = max_width;

	leftover = available_width - inner;
	col.column_width = inner;
	col.left_margin = leftover / 2;
	col.right_margin = leftover - col.left_margin;
	return col;
}
