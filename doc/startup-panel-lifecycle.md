# Startup panel lifecycle audit

This audit is the implementation contract for `UI-LOAD-102` and later startup
presentation work. It describes the code in this repository as of
`UI-LOAD-101`; older names and build options can otherwise give a misleading
picture of the renderer in use.

## Renderer finding

The main content panels are **not WebKit-backed**. `XiphosHtml` is an alias for
`WkHtml`, but `src/webkit/wk-html.h` defines `WkHtml` as a `GtkBox` containing a
native `GtkTextView`. `wk_html_close()` calls `load_html()` synchronously;
`load_html()` parses into an off-screen `GtkTextBuffer` and attaches that
completed buffer to the view before returning. The `src/webkit` directory,
`USE_WEBKIT2` layout branches, comments that say “WebKit”, and the
`Xiphos::Webkit` CMake target are historical names, not evidence of a
`WebKitWebView` in the reading path.

Consequently there is no `WebKitLoadEvent`, `load-changed`,
`WEBKIT_LOAD_FINISHED`, or equivalent asynchronous renderer signal available
on these panels. For the current renderer, successful return from
`HtmlOutput()` (and therefore `wk_html_close()`) is the exact document-ready
event: parsing, buffer construction, buffer attachment, body-color application,
and requested anchor handoff have completed. A later move back to WebKit must
adapt `WEBKIT_LOAD_FINISHED` (and load failure) to the same panel contract
rather than exposing WebKit types to panel containers.

## Observed startup sequence

The normal launcher and the SUPER+B desktop binding both enter `main()`; there
is no separate in-process SUPER+B path. Source-order tracing gives this
sequence:

1. `settings_init()` selects/loads settings and module lists, `gui_init()`
   initializes GTK, and the splash is created.
2. `create_mainwindow()` creates all main-window pane widgets and immediately
   calls `gtk_widget_show_all(widgets.app)`. At this point the content
   `GtkTextView` buffers are empty and the Bible backend has not been
   initialized.
3. `main_init_backend()` opens the selected SQLite backend by default (or the
   SWORD compatibility backend) and establishes module availability.
4. `frontend_init()` creates the sidebar and optional parallel pane, realizes
   all major content widgets, then `main_flush_widgets_content()` synchronously
   submits a one-space blank HTML document to Bible, commentary, dictionary,
   and the hidden general-book view. It initializes the previewer separately.
5. `frontend_display()` calls `gui_show_main_window()` again, applies pane
   visibility, and calls `sync_windows()`, which allows GTK painting while the
   content panes still contain blank/placeholder documents.
6. `gui_notebook_main_setup()` restores the selected passage tab. Its switch
   callback synchronously requests, renders, and submits general-book,
   commentary, Bible, and dictionary content in that order. Devotional content
   is loaded afterward only when its saved tab is active. Each `HtmlOutput()`
   completes its native buffer swap before the next call begins.
7. Saved auxiliary windows are opened, the verse-notes panel is refreshed,
   the splash is dismissed from an idle callback, and `gtk_main()` processes
   normal interaction.

The blank first paint is therefore a combination of **premature content-widget
visibility** and **real content being submitted later**. It is not a WebKit
default-white surface and there is no post-submission WebKit load delay. The
native `GtkTextView` may use a light theme background while empty, and the
explicit blank HTML uses the configured Bible background, so either can appear
as a white empty region. Backend work lies between initial visibility and real
submission, but this audit found no backend correctness or performance bug and
does not propose backend changes.

## Panel inventory and readiness event

“HTML” below means the native `WkHtml`/`GtkTextView` renderer. Native GTK panes
already have synchronous model/widget updates and do not need an HTML load
completion signal.

| Main-window region | Implementation | Startup exposure and initial content | READY event |
|---|---|---|---|
| Bible/standard reading pane | Native HTML (`widgets.html_text`) | Visible whenever texts are enabled; blanked in `frontend_init()`, populated by restored-tab `main_display_bible()` | Return from its final `HtmlOutput()`/`wk_html_close()` |
| Secondary compare/interlinear reading pane | Native HTML (`widgets.html_lectura_sync`) plus native GTK controls | Constructed during the Bible pane; deliberately hidden again after `show_all()` unless enabled; content is written on demand | Return from its `HtmlOutput()`; EMPTY while feature is disabled |
| Parallel page | Native HTML (`widgets.html_parallel`) | Created in `frontend_init()` when a Bible exists; only exposed if the parallel page/tab is selected | Return from the parallel renderer's final `wk_html_close()`; EMPTY when not requested |
| Commentary | Native HTML (`widgets.html_comm`) | Can be visible at startup; blanked first, then populated by restored-tab `main_display_commentary()` | Return from the backend display's final `HtmlOutput()`/`wk_html_close()` |
| Verse notes | Native GTK (`GtkTextView`, labels, buttons, list) | Second commentary notebook page; refreshed after Bible/tab restoration | Return from `gui_verse_notes_panel_actualizar()` |
| Dictionary | Native HTML (`widgets.html_dict`) plus native GTK navigation | Can be visible at startup; blanked first, then populated by restored-tab `main_display_dictionary()` | Return from the resulting `HtmlOutput()`/`wk_html_close()` |
| Devotional | Native HTML (`widgets.html_devotional`) plus native GTK calendar/navigation | Not blanked by `main_flush_widgets_content()`; loaded lazily when the saved devotional tab is active | Return from `main_display_devotional()` after its `HtmlOutput()` |
| Previewer below Bible | Native HTML (`widgets.html_previewer_text`) | Can be visible at startup; receives the “Previewer” document during `frontend_init()` | Return from `main_init_previewer()` |
| Sidebar previewer | Native HTML (`sidebar.html_viewer_widget`) | Created during `frontend_init()` and used instead of the lower previewer when configured | Return from `main_init_previewer()` or the selection-driven `main_entry_display()` |
| Sidebar module/bookmark/search controls | Native GTK trees, notebooks, entries and buttons | Visible with chrome as soon as the main window is painted | Completion of the synchronous GTK model update; no panel loading placeholder required |
| General book | Native HTML (`widgets.html_book`) | Constructed but intentionally not inserted into the visible main notebook in the current Bible-study layout | Return from its `HtmlOutput()` when an auxiliary/on-demand route exposes it; otherwise EMPTY |

Dialog-only HTML views (search preview, detached Bible/commentary/dictionary,
display info, word cloud, testimonies, and interlinear detail) are created on
demand and cannot cause the main startup blank. Saved detached parallel/module
manager windows may open during step 7; the former uses the same native HTML
completion rule, while the module manager is native GTK.

## Shared state contract

`PanelLoadModel` in `src/gui/panel_load_state.h` defines the renderer- and
backend-independent lifecycle:

- `EMPTY`: no document has been requested for this panel (including a disabled
  or intentionally hidden optional pane).
- `LOADING`: a current content request exists and the panel should present its
  intentional loading surface while the producer fetches/builds/submits it.
- `READY`: the current request's document is attached and is safe to reveal.
- `ERROR`: the current request failed; an intentional error surface must replace
  the loading surface.

`panel_load_model_begin()` returns a monotonically changing token. Only a
matching current token can transition LOADING to READY or ERROR; starting or
clearing a request invalidates older completions. This keeps the shared model
usable for future asynchronous renderers and prevents stale navigation from
revealing the wrong document without importing GTK, SQLite, SWORD, OSIS, or
USFM concepts.

For the current synchronous renderer the integration boundary for the next
task is: set LOADING immediately before content acquisition/submission, call
READY after successful `HtmlOutput()`, and call ERROR when acquisition cannot
produce an intentional document. Panels transition independently. The main
window, header, navigation, tabs, and sidebar chrome must remain visible and
responsive; no aggregate “all panels ready” gate is part of this contract.

## First-paint validation

`UI-LOAD-104` rechecked the renderer boundary rather than assuming historical
WebKit behavior. There is still no `WebKitWebView` in a reading panel and thus
no `WEBKIT_LOAD_FINISHED` event to wait beyond. `load_html()` parses into an
off-screen `GtkTextBuffer`, attaches that completed buffer, and only then does
`wk_html_close()` synchronously switch the panel stack to CONTENT. The next GTK
paint therefore sees either the theme-painted LOADING child or the attached
CONTENT child; an idle delay would add latency without making this handoff
safer.

An initial parse or acquisition failure now transitions to a full-allocation,
theme-painted ERROR child through `wk_html_load_failed()`. A failed replacement
keeps the prior ready document visible, while the next successful request can
move ERROR to CONTENT. Parsing is attempted before current-document bookkeeping
is discarded, and generation tokens reject stale completions during rapid
navigation. The handoff schedules no idle, timeout, load-signal, or paint
callback, so destroying a panel while it is LOADING cannot leave a readiness
callback holding the panel pointer.

The loading, error, and content children remain local to each `WkHtml`; the
main window is still shown before backend initialization, and SQLite/SWORD
selection and the SQLite guards around SWORD-only parallel views are unchanged.
Focused state/surface regressions validate initial failure, reload retention,
ERROR-to-CONTENT recovery, independent panels, stable allocation, and stale
token rejection. Full live startup was attempted three times for SQLite plus
the SQLite/SWORD selection matrix, but this sandbox rejected D-Bus socket
creation before GTK application launch. The source-level first-paint contract,
headless CSS test, focused tests, and full build therefore provide the local
evidence; no unsupported screenshot or manual visual claim is made.

## UI-LOAD-105: confirmed real-display regression

A later real GTK startup disproved the presentation conclusion above.  The
state machine was functioning, but three first-frame details were outside what
the headless tests proved:

1. `create_mainwindow()` called `gtk_widget_show_all(widgets.app)` and then
   drained GTK events before `gui_elim_tema_init()` installed the selected live
   palette.  The placeholder therefore had a correct CSS class but its first
   mapped frame could still use the toolkit's default white background.
2. The loading child deliberately used `@elim_paper`, the final document color.
   In the light modes this looked exactly like a large empty white document,
   so the placeholder did not communicate a neutral application surface.
3. `gtk_widget_show_all()` made the renderer child's `visible` property true.
   GtkStack selection normally prevented it from being drawn, but the contract
   was weaker than the test description claimed and vulnerable to first-map or
   later container changes.  The live GtkStack portion of the test had in fact
   been skipped when no display was available.

The corrected startup installs the selected palette before constructing and
mapping the main window.  Each WkHtml stack keeps its content child hidden with
`no-show-all` while the full-allocation loading child, painted with
`@elim_chrome`, is selected.  After the native parser attaches its complete
off-screen GtkTextBuffer, READY schedules an idle handoff; only that callback
shows and selects the renderer.  A subsequent navigation already has a visible
document, so it keeps that document in place and does not return to LOADING.
The idle is generation-checked, cancelled by a newer request, and removed when
the widget is disposed.

The startup surface map is:

| Visible region | Content widget | Immediate container | Initial behavior |
|---|---|---|---|
| Main Bible | `widgets.html_text` | WkHtml GtkStack inside interlinear/compare wrappers and `notebook_bible_parallel` | Own loading child until Bible READY |
| Lower-left preview | `widgets.html_previewer_text` | WkHtml GtkStack in `vbox_previewer`/left `GtkPaned` | Own loading child until preview READY |
| Sidebar preview | `sidebar.html_viewer_widget` | WkHtml GtkStack in sidebar `GtkPaned` | Own loading child until preview READY |
| Commentary | `widgets.html_comm` | WkHtml GtkStack in `notebook_comm_book` | Own loading child until commentary READY |
| Verse notes | native editable `GtkTextView` | page 1 of `notebook_comm_book` | Not WebKit/WkHtml; populated synchronously before its startup selection |
| Dictionary | `widgets.html_dict` | WkHtml GtkStack below dictionary controls in `notebook_dict_devot` | Own loading child until dictionary READY |
| Devotional | `widgets.html_devotional` | WkHtml GtkStack on page 1 of `notebook_dict_devot` | Independent loading child; loaded only if its page is selected |
| Parallel view | `widgets.html_parallel` | WkHtml GtkStack on page 1 of `notebook_bible_parallel` | Independent loading child; only visible when selected |
| Compare Bible | `widgets.html_lectura_sync` | WkHtml GtkStack in the optional compare `GtkPaned` | Protected even when window `show_all()` runs; wrapper is restored hidden unless enabled |
| General book | `widgets.html_book` | WkHtml GtkStack in a retained, unattached pane | Cannot be startup-visible |

The only actual `WebKitWebView` constructor is still the optional study-pad
editor.  It is not one of the startup reading panels and uses the available
WebKit1 API (`webkit_web_view_set_transparent()`) plus the themed surface class
before `gtk_widget_show()`.  The reading surfaces have no `about:blank`,
`load-changed`, or `WEBKIT_LOAD_FINISHED`: `LOAD_COMMITTED` and `LOAD_FINISHED`
in their diagnostics describe completion of the native off-screen buffer path.
Final Bible/commentary/dictionary HTML obtains body colors from the selected
application palette; no new literal-white document background is introduced.

Set `BIBLIA_ELIM_UI_LOAD_DEBUG=1` for timestamped, per-surface diagnostics:
`CREATE`, `SHOW`, `MAP`, `PLACEHOLDER_VISIBLE`, `LOAD_STARTED`,
`LOAD_COMMITTED`, `LOAD_FINISHED`, `RENDERER_MAP`, and `RENDERER_VISIBLE`.
`RENDERER_VISIBLE` is the truthful native equivalent of `WEBVIEW_VISIBLE`.
An initial trace must show the placeholder map before renderer visibility, and
renderer visibility only after load completion.  These diagnostics are silent
by default.

## NAV-LOAD-101: verse-navigation readiness

Previous/next readiness is a backend-and-reference decision, not a Bible
surface lifecycle state. `prepareVerseNavigation()` validates the interlinear
lock, backend, selected Bible module, current reference, and backend navigation
target without reading `PanelLoadModel`, `WkHtml`, or GTK visibility. The GTK
handler only applies an accepted target to the entry and activates the existing
display path. A renderer may therefore be EMPTY, LOADING, READY, or ERROR
without changing whether a backend-ready verse request is accepted.

With `BIBLIA_ELIM_UI_LOAD_DEBUG=1`, `MODULE_READY` records backend availability;
`NAV_PREV_SENSITIVE` and `NAV_NEXT_SENSITIVE` record the initial widget state;
each click records `NAV_HANDLER_ENTER`, either a precise early-exit reason or
the before/after reference, and `DISPLAY_COMPLETE` records synchronous display
duration. `FIRST_CONTENT_REQUEST`, `FIRST_CONTENT_READY`, GTK heartbeat, window
show, and main-loop-entry events locate any startup starvation interval. Both
SQLite and SWORD display paths now reach the common completion timestamp and
restore the temporarily blocked scroll handler.

The focused regression exercises readiness in all four renderer states, all
documented early-exit reasons, and three immediate NEXT decisions. It does not
replace the required real-display startup and rapid-click check: a headless or
sandboxed GTK run cannot establish input responsiveness or painted-frame
timing.
