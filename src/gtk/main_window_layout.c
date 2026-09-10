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
