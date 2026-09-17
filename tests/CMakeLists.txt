# Biblia Elim - pruebas
#
# No entran en la compilación normal: se piden a mano.
#
#   cmake --build build --target pulpito_test && ./build/tests/pulpito_test

if (BUILD_TESTING AND UNIX)
  find_program (PYTHON3_EXECUTABLE NAMES python3)
  if (PYTHON3_EXECUTABLE)
    add_test (
      NAME startup_performance_baseline
      COMMAND ${PYTHON3_EXECUTABLE}
              ${CMAKE_CURRENT_SOURCE_DIR}/startup_performance_baseline_test.py
      )
    add_custom_target (startup_performance_baseline_test
      COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
              -R ^startup_performance_baseline$
      USES_TERMINAL
      )
  endif ()

  find_program (XVFB_RUN_EXECUTABLE NAMES xvfb-run)
  find_program (SQLITE3_EXECUTABLE NAMES sqlite3)
  if (XVFB_RUN_EXECUTABLE AND SQLITE3_EXECUTABLE)
    add_test (
      NAME gtk_lifecycle_smoke
      COMMAND ${CMAKE_COMMAND}
              -DAPP=$<TARGET_FILE:biblia-elim>
              -DSQLITE3=${SQLITE3_EXECUTABLE}
              -DXVFB_RUN=${XVFB_RUN_EXECUTABLE}
              -DFIXTURE=${PROJECT_SOURCE_DIR}/tests/fixtures/sqlite/test-bible.sql
              -DRUNTIME_ROOT=${CMAKE_CURRENT_BINARY_DIR}
              -P ${CMAKE_CURRENT_SOURCE_DIR}/run_gtk_lifecycle_smoke.cmake
      )
    set_tests_properties (gtk_lifecycle_smoke PROPERTIES
      SKIP_RETURN_CODE 77
      TIMEOUT 45
      )
    add_custom_target (gtk_lifecycle_smoke_test
      COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
              -R ^gtk_lifecycle_smoke$
      DEPENDS biblia-elim
      USES_TERMINAL
      )
  endif ()

  if (XVFB_RUN_EXECUTABLE)
    add_test (
      NAME author_commentary_probe
      COMMAND ${CMAKE_COMMAND}
              -DAPP=$<TARGET_FILE:biblia-elim>
              -DXVFB_RUN=${XVFB_RUN_EXECUTABLE}
              -DRUNTIME_ROOT=${CMAKE_CURRENT_BINARY_DIR}
              -P ${CMAKE_CURRENT_SOURCE_DIR}/run_author_commentary_probe.cmake
      )
    set_tests_properties (author_commentary_probe PROPERTIES
      SKIP_RETURN_CODE 77
      TIMEOUT 140
      )
    add_custom_target (author_commentary_probe_test
      COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
              -R ^author_commentary_probe$
      DEPENDS biblia-elim
      USES_TERMINAL
      )
  endif ()
endif ()

add_executable (pulpito_test
  EXCLUDE_FROM_ALL
  pulpito_test.c
  ${PROJECT_SOURCE_DIR}/src/main/pulpito_pasos.c
  )

target_link_libraries (pulpito_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (panel_load_state_test
  EXCLUDE_FROM_ALL
  panel_load_state_test.c
  ${PROJECT_SOURCE_DIR}/src/gtk/panel_load_state.c
  )

add_executable (zoom_state_test
  EXCLUDE_FROM_ALL
  zoom_state_test.c
  ${PROJECT_SOURCE_DIR}/src/main/zoom_state.c
  )

target_include_directories (zoom_state_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Core
  PkgConfig::Gtk
  )

target_compile_definitions (zoom_state_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_link_libraries (zoom_state_test
  PRIVATE
  PkgConfig::Core
  PkgConfig::Gtk
  )

add_executable (zoom_anchor_reflow_test
  EXCLUDE_FROM_ALL
  zoom_anchor_reflow_test.c
  ${PROJECT_SOURCE_DIR}/src/webkit/wk-html-zoom-anchor.c
  )

target_include_directories (zoom_anchor_reflow_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Core
  )

target_compile_definitions (zoom_anchor_reflow_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_link_libraries (zoom_anchor_reflow_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (zoom_indicator_test
  EXCLUDE_FROM_ALL
  zoom_indicator_test.c
  ${PROJECT_SOURCE_DIR}/src/gtk/zoom_indicator.c
  ${PROJECT_SOURCE_DIR}/src/main/zoom_state.c
  )

target_include_directories (zoom_indicator_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Core
  )

target_compile_definitions (zoom_indicator_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_link_libraries (zoom_indicator_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (comm_panel_close_test
  EXCLUDE_FROM_ALL
  comm_panel_close_test.c
  )

target_compile_definitions (comm_panel_close_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_link_libraries (comm_panel_close_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (application_name_startup_test
  EXCLUDE_FROM_ALL
  application_name_startup_test.c
  )

target_compile_definitions (application_name_startup_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_link_libraries (application_name_startup_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (barra_busqueda_startup_test
  EXCLUDE_FROM_ALL
  barra_busqueda_startup_test.c
  )

target_compile_definitions (barra_busqueda_startup_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_link_libraries (barra_busqueda_startup_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (startup_diagnostics_test
  EXCLUDE_FROM_ALL
  startup_diagnostics_test.cc
  ${PROJECT_SOURCE_DIR}/src/main/startup_diagnostics.cc
  )

target_include_directories (startup_diagnostics_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Core
  )

target_link_libraries (startup_diagnostics_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (startup_profile_test
  EXCLUDE_FROM_ALL
  startup_profile_test.c
  ${PROJECT_SOURCE_DIR}/src/main/startup_profile.c
  )

target_include_directories (startup_profile_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Core
  )

target_link_libraries (startup_profile_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (study_reading_layout_test
  EXCLUDE_FROM_ALL
  study_reading_layout_test.c
  )

target_compile_definitions (study_reading_layout_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_link_libraries (study_reading_layout_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (study_pane_collapse_test
  EXCLUDE_FROM_ALL
  study_pane_collapse_test.c
  )

target_include_directories (study_pane_collapse_test
  PRIVATE
  PkgConfig::Gtk
  )

target_link_libraries (study_pane_collapse_test
  PRIVATE
  PkgConfig::Gtk
  )

add_executable (main_window_layout_test
  EXCLUDE_FROM_ALL
  main_window_layout_test.c
  ${PROJECT_SOURCE_DIR}/src/gtk/main_window_layout.c
  )

target_include_directories (main_window_layout_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src/gtk
  PkgConfig::Core
  )

target_link_libraries (main_window_layout_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (verse_navigation_readiness_test
  EXCLUDE_FROM_ALL
  verse_navigation_readiness_test.cc
  fake_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/main/verse_navigation.cc
  ${PROJECT_SOURCE_DIR}/src/backend/content_resolver.cc
  ${PROJECT_SOURCE_DIR}/src/backend/content_availability.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/gtk/panel_load_state.c
  )

target_include_directories (verse_navigation_readiness_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Core
  PkgConfig::Sword
  )

target_link_libraries (verse_navigation_readiness_test
  PRIVATE
  PkgConfig::Core
  PkgConfig::Sword
  )

add_executable (reading_window_test
  EXCLUDE_FROM_ALL
  reading_window_test.c
  ${PROJECT_SOURCE_DIR}/src/main/reading_window.c
  )

target_include_directories (reading_window_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Core
  )

target_link_libraries (reading_window_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (wheel_scroll_test
  EXCLUDE_FROM_ALL
  wheel_scroll_test.c
  ${PROJECT_SOURCE_DIR}/src/main/wheel_scroll.c
  )

target_include_directories (wheel_scroll_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Core
  )

target_link_libraries (wheel_scroll_test
  PRIVATE
  PkgConfig::Core
  m
  )

target_compile_definitions (wheel_scroll_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

# the reader's wheel / reading-focus preferences, through settings.xml
# (xml.c parses it with libxml2)
pkg_check_modules (TestXml REQUIRED IMPORTED_TARGET "libxml-2.0")

add_executable (navigation_prefs_test
  EXCLUDE_FROM_ALL
  navigation_prefs_test.c
  navigation_prefs_stubs.c
  ${PROJECT_SOURCE_DIR}/src/main/navigation_prefs.c
  ${PROJECT_SOURCE_DIR}/src/main/navigation_prefs_settings.c
  ${PROJECT_SOURCE_DIR}/src/main/wheel_scroll.c
  ${PROJECT_SOURCE_DIR}/src/main/reading_focus.c
  ${PROJECT_SOURCE_DIR}/src/main/xml.c
  )

target_include_directories (navigation_prefs_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}
  PkgConfig::Core
  PkgConfig::Gtk
  PkgConfig::TestXml
  )

target_link_libraries (navigation_prefs_test
  PRIVATE
  PkgConfig::Core
  PkgConfig::Gtk
  PkgConfig::TestXml
  m
  )

add_executable (reading_focus_test
  EXCLUDE_FROM_ALL
  reading_focus_test.c
  ${PROJECT_SOURCE_DIR}/src/main/reading_focus.c
  )

target_include_directories (reading_focus_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Core
  )

target_link_libraries (reading_focus_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (navbar_valid_key_ownership_test
  EXCLUDE_FROM_ALL
  navbar_valid_key_ownership_test.cc
  fake_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/main/navbar_key.cc
  )
target_include_directories (navbar_valid_key_ownership_test PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Core PkgConfig::Gtk)
target_link_libraries (navbar_valid_key_ownership_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk)

add_executable (poetry_line_wrap_test
  EXCLUDE_FROM_ALL
  poetry_line_wrap_test.c
  )

target_compile_definitions (poetry_line_wrap_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_link_libraries (poetry_line_wrap_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (navbar_picker_focus_test
  EXCLUDE_FROM_ALL
  navbar_picker_focus_test.c
  )

target_compile_definitions (navbar_picker_focus_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_link_libraries (navbar_picker_focus_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (navbar_picker_entry_test
  EXCLUDE_FROM_ALL
  navbar_picker_entry_test.c
  ${PROJECT_SOURCE_DIR}/src/main/picker_entry.c
  )

target_include_directories (navbar_picker_entry_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Gtk
  )

# real keyboard input through the X server (XTest), under xvfb
find_library (PICKER_TEST_X11_LIB X11)
find_library (PICKER_TEST_XTST_LIB Xtst)

target_link_libraries (navbar_picker_entry_test
  PRIVATE
  PkgConfig::Gtk
  ${PICKER_TEST_X11_LIB}
  ${PICKER_TEST_XTST_LIB}
  )

add_executable (navbar_entry_reference_test
  EXCLUDE_FROM_ALL
  navbar_entry_reference_test.c
  ${PROJECT_SOURCE_DIR}/src/gtk/navbar_entry_reference.c
  )
target_include_directories (navbar_entry_reference_test PRIVATE
  ${PROJECT_SOURCE_DIR}/src/gtk
  PkgConfig::Core)
target_link_libraries (navbar_entry_reference_test PRIVATE
  PkgConfig::Core)

add_executable (reading_width_contract_test
  EXCLUDE_FROM_ALL
  reading_width_contract_test.c
  )

target_include_directories (reading_width_contract_test
  PRIVATE
  PkgConfig::Gtk
  )

target_link_libraries (reading_width_contract_test
  PRIVATE
  PkgConfig::Gtk
  )

add_executable (font_size_contract_test
  EXCLUDE_FROM_ALL
  font_size_contract_test.c
  ${PROJECT_SOURCE_DIR}/src/main/font_size.c
  )

target_compile_definitions (font_size_contract_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_include_directories (font_size_contract_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Core
  )

target_link_libraries (font_size_contract_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (wk_html_surface_test
  EXCLUDE_FROM_ALL
  wk_html_surface_test.c
  ${PROJECT_SOURCE_DIR}/src/webkit/wk-html-surface.c
  )

target_compile_definitions (wk_html_surface_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_include_directories (wk_html_surface_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Gtk
  )

target_link_libraries (wk_html_surface_test
  PRIVATE
  PkgConfig::Gtk
  )

target_include_directories (panel_load_state_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  )

add_executable (morfologia_test
  EXCLUDE_FROM_ALL
  morfologia_test.c
  ${PROJECT_SOURCE_DIR}/src/main/morfologia.c
  )

target_link_libraries (morfologia_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (glosa_test
  EXCLUDE_FROM_ALL
  glosa_test.c
  ${PROJECT_SOURCE_DIR}/src/main/glosa.c
  )

target_link_libraries (glosa_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (testimonios_test
  EXCLUDE_FROM_ALL
  testimonios_test.c
  ${PROJECT_SOURCE_DIR}/src/main/testimonios.c
  )

target_compile_definitions (testimonios_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_include_directories (testimonios_test
  PRIVATE
  PkgConfig::Core
  PkgConfig::Gnome
  )

target_link_libraries (testimonios_test
  PRIVATE
  PkgConfig::Core
  PkgConfig::Gnome
  )

add_executable (buscar_notas_test
  EXCLUDE_FROM_ALL
  buscar_notas_test.c
  ${PROJECT_SOURCE_DIR}/src/main/buscar_notas.c
  ${PROJECT_SOURCE_DIR}/src/main/busqueda_tildes.c
  )

target_link_libraries (buscar_notas_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (quoted_heading_test
  EXCLUDE_FROM_ALL
  quoted_heading_test.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  )

add_executable (content_resolver_test
  EXCLUDE_FROM_ALL
  content_resolver_test.cc
  ${PROJECT_SOURCE_DIR}/src/backend/content_resolver.cc
  ${PROJECT_SOURCE_DIR}/src/backend/content_availability.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  )

target_include_directories (content_resolver_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Core
  PkgConfig::Sword
  )

target_link_libraries (content_resolver_test
  PRIVATE
  PkgConfig::Core
  PkgConfig::Sword
  )

target_include_directories (quoted_heading_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
  PkgConfig::Core
  )

target_link_libraries (quoted_heading_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (bible_types_test
  EXCLUDE_FROM_ALL
  bible_types_test.cc
  )

target_include_directories (bible_types_test
  PRIVATE
  PkgConfig::Core
  )

target_link_libraries (bible_types_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (bible_backend_contract_test
  EXCLUDE_FROM_ALL
  bible_backend_contract_test.cc
  bible_backend_contract.cc
  strong_backend_contract.cc
  fake_bible_backend.cc
  )

target_include_directories (bible_backend_contract_test
  PRIVATE
  PkgConfig::Core
  )

target_link_libraries (bible_backend_contract_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (strong_id_test EXCLUDE_FROM_ALL
  strong_id_test.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_strong_lexicon.cc)
target_include_directories (strong_id_test PRIVATE PkgConfig::Core PkgConfig::Sqlite3)
target_link_libraries (strong_id_test PRIVATE PkgConfig::Core PkgConfig::Sqlite3)

add_executable (strong_interaction_test EXCLUDE_FROM_ALL
  strong_interaction_test.cc
  fake_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/main/strong_interaction.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  )
target_include_directories (strong_interaction_test PRIVATE PkgConfig::Core)
target_link_libraries (strong_interaction_test PRIVATE PkgConfig::Core)

add_executable (strong_ui_smoke_test EXCLUDE_FROM_ALL
  strong_ui_smoke_test.cc
  fake_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/main/strong_interaction.cc
  ${PROJECT_SOURCE_DIR}/src/main/strong_ui.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  )
target_include_directories (strong_ui_smoke_test PRIVATE PkgConfig::Core PkgConfig::Gtk)
target_link_libraries (strong_ui_smoke_test PRIVATE PkgConfig::Core PkgConfig::Gtk)

add_executable (sqlite_strong_ui_smoke_test EXCLUDE_FROM_ALL
  sqlite_strong_ui_smoke_test.cc
  ${PROJECT_SOURCE_DIR}/src/main/strong_interaction.cc
  ${PROJECT_SOURCE_DIR}/src/main/strong_ui.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_module_writer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  )
target_include_directories (sqlite_strong_ui_smoke_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Sqlite3)
target_link_libraries (sqlite_strong_ui_smoke_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Sqlite3)
target_sources(sqlite_strong_ui_smoke_test PRIVATE
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc)

add_executable (sqlite_bible_backend_test
  EXCLUDE_FROM_ALL
  sqlite_bible_backend_test.cc
  bible_backend_contract.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_module_writer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  )

target_compile_definitions (sqlite_bible_backend_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_include_directories (sqlite_bible_backend_test
  PRIVATE
  PkgConfig::Core
  PkgConfig::Sqlite3
  )

target_link_libraries (sqlite_bible_backend_test
  PRIVATE
  PkgConfig::Core
  PkgConfig::Sqlite3
  )
target_sources(sqlite_bible_backend_test PRIVATE
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc)

add_executable (sqlite_module_writer_morphology_test EXCLUDE_FROM_ALL
  sqlite_module_writer_morphology_test.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_module_writer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc)
target_include_directories (sqlite_module_writer_morphology_test PRIVATE
  PkgConfig::Core PkgConfig::Sqlite3)
target_link_libraries (sqlite_module_writer_morphology_test PRIVATE
  PkgConfig::Core PkgConfig::Sqlite3)

add_executable (sqlite_bible_backend_benchmark
  EXCLUDE_FROM_ALL
  sqlite_bible_backend_benchmark.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
	${PROJECT_SOURCE_DIR}/src/main/strong_interaction.cc
  )

target_include_directories (sqlite_bible_backend_benchmark
  PRIVATE
  PkgConfig::Core
  PkgConfig::Sqlite3
  )

target_link_libraries (sqlite_bible_backend_benchmark
  PRIVATE
  PkgConfig::Core
  PkgConfig::Sqlite3
  )
target_sources(sqlite_bible_backend_benchmark PRIVATE
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc)

add_executable (enriched_content_benchmark EXCLUDE_FROM_ALL
  enriched_content_benchmark.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc)
target_include_directories (enriched_content_benchmark PRIVATE PkgConfig::Core PkgConfig::Sqlite3)
target_link_libraries (enriched_content_benchmark PRIVATE PkgConfig::Core PkgConfig::Sqlite3)
target_sources(enriched_content_benchmark PRIVATE
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc)

add_executable (osis_importer_test EXCLUDE_FROM_ALL
  osis_importer_test.cc
  ${PROJECT_SOURCE_DIR}/src/backend/osis_importer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_module_writer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc)
target_sources(osis_importer_test PRIVATE ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc)
target_compile_definitions (osis_importer_test PRIVATE SRCDIR="${PROJECT_SOURCE_DIR}")
target_include_directories (osis_importer_test PRIVATE PkgConfig::Core PkgConfig::Sqlite3 PkgConfig::XML2)
target_link_libraries (osis_importer_test PRIVATE PkgConfig::Core PkgConfig::Sqlite3 PkgConfig::XML2)

add_executable (osis_semantic_equivalence_test EXCLUDE_FROM_ALL
  osis_semantic_equivalence_test.cc
  neutral_module_comparator.cc
  ${PROJECT_SOURCE_DIR}/src/backend/osis_importer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/usfm_importer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_module_writer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc)
target_compile_definitions (osis_semantic_equivalence_test PRIVATE SRCDIR="${PROJECT_SOURCE_DIR}")
target_include_directories (osis_semantic_equivalence_test PRIVATE
  PkgConfig::Core PkgConfig::Sqlite3 PkgConfig::XML2)
target_link_libraries (osis_semantic_equivalence_test PRIVATE
  PkgConfig::Core PkgConfig::Sqlite3 PkgConfig::XML2)
target_sources(osis_semantic_equivalence_test PRIVATE ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc)

add_executable (osis_operational_test EXCLUDE_FROM_ALL
  osis_operational_test.cc
  ${PROJECT_SOURCE_DIR}/src/backend/osis_importer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_module_writer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc)
target_include_directories (osis_operational_test PRIVATE
  PkgConfig::Core PkgConfig::Sqlite3 PkgConfig::XML2)
target_link_libraries (osis_operational_test PRIVATE
  PkgConfig::Core PkgConfig::Sqlite3 PkgConfig::XML2)
target_sources(osis_operational_test PRIVATE ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc)

add_executable (osis_property_test EXCLUDE_FROM_ALL
  osis_property_test.cc
  neutral_module_comparator.cc
  ${PROJECT_SOURCE_DIR}/src/backend/osis_importer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_module_writer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc)
target_include_directories (osis_property_test PRIVATE
  PkgConfig::Core PkgConfig::Sqlite3 PkgConfig::XML2)
target_link_libraries (osis_property_test PRIVATE
  PkgConfig::Core PkgConfig::Sqlite3 PkgConfig::XML2)
target_sources(osis_property_test PRIVATE ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc)

add_executable (sqlite_strong_profile EXCLUDE_FROM_ALL
  sqlite_strong_profile.cc
  )

target_include_directories (sqlite_strong_profile
  PRIVATE
  PkgConfig::Sqlite3
  )

target_link_libraries (sqlite_strong_profile
  PRIVATE
  PkgConfig::Sqlite3
  )

add_executable (usfm_importer_test
  EXCLUDE_FROM_ALL
  usfm_importer_test.cc
  strong_backend_contract.cc
  ${PROJECT_SOURCE_DIR}/src/backend/usfm_importer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_module_writer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  )
target_compile_definitions (usfm_importer_test PRIVATE SRCDIR="${PROJECT_SOURCE_DIR}")
target_include_directories (usfm_importer_test PRIVATE PkgConfig::Core PkgConfig::Sqlite3)
target_link_libraries (usfm_importer_test PRIVATE PkgConfig::Core PkgConfig::Sqlite3)
target_sources(usfm_importer_test PRIVATE ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc)

add_executable (morphology_audit_test EXCLUDE_FROM_ALL
  morphology_audit_test.cc
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc
  ${PROJECT_SOURCE_DIR}/src/backend/osis_importer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/usfm_importer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_module_writer.cc)
target_compile_definitions(morphology_audit_test PRIVATE SRCDIR="${PROJECT_SOURCE_DIR}")
target_include_directories(morphology_audit_test PRIVATE
  PkgConfig::Core PkgConfig::Sqlite3 PkgConfig::XML2)
target_link_libraries(morphology_audit_test PRIVATE
  PkgConfig::Core PkgConfig::Sqlite3 PkgConfig::XML2)

add_executable (morphology_integration_benchmark EXCLUDE_FROM_ALL
  morphology_integration_benchmark.cc
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc
  ${PROJECT_SOURCE_DIR}/src/backend/osis_importer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/usfm_importer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_module_writer.cc)
target_compile_definitions(morphology_integration_benchmark PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}")
target_include_directories(morphology_integration_benchmark PRIVATE
  PkgConfig::Core PkgConfig::Sqlite3 PkgConfig::XML2)
target_link_libraries(morphology_integration_benchmark PRIVATE
  PkgConfig::Core PkgConfig::Sqlite3 PkgConfig::XML2)

add_executable (sqlite_module_manager_test EXCLUDE_FROM_ALL
  sqlite_module_manager_test.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite_module_manager.cc
  ${PROJECT_SOURCE_DIR}/src/backend/usfm_importer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_module_writer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc)
target_include_directories (sqlite_module_manager_test PRIVATE PkgConfig::Core PkgConfig::Gnome PkgConfig::Sqlite3)
target_compile_definitions (sqlite_module_manager_test PRIVATE SRCDIR="${PROJECT_SOURCE_DIR}")
target_link_libraries (sqlite_module_manager_test PRIVATE PkgConfig::Core PkgConfig::Gnome PkgConfig::Sqlite3)
target_sources(sqlite_module_manager_test PRIVATE ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc)

add_executable (sqlite_backend_full_probe
  EXCLUDE_FROM_ALL
  sqlite_backend_full_probe.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  )
target_include_directories (sqlite_backend_full_probe PRIVATE PkgConfig::Core PkgConfig::Sqlite3)
target_link_libraries (sqlite_backend_full_probe PRIVATE PkgConfig::Core PkgConfig::Sqlite3)
target_sources(sqlite_backend_full_probe PRIVATE
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc)

add_executable (backend_comparison_benchmark EXCLUDE_FROM_ALL
  backend_comparison_benchmark.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword_main.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword/sword_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/main/busqueda_tildes.c
  )
target_compile_options (backend_comparison_benchmark PRIVATE -ffunction-sections)
target_link_options (backend_comparison_benchmark PRIVATE -Wl,--gc-sections)
target_include_directories (backend_comparison_benchmark PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword PkgConfig::Sqlite3)
target_link_libraries (backend_comparison_benchmark PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword PkgConfig::Sqlite3)
target_sources(backend_comparison_benchmark PRIVATE
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc)

add_executable (sword_backend_key_lifecycle_test EXCLUDE_FROM_ALL
  sword_backend_key_lifecycle_test.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword_main.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword/sword_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc
  ${PROJECT_SOURCE_DIR}/src/main/busqueda_tildes.c)

add_executable (versification_transition_test EXCLUDE_FROM_ALL
  versification_transition_test.cc
  ${PROJECT_SOURCE_DIR}/src/main/reference_transition.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword_main.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword/sword_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc
  ${PROJECT_SOURCE_DIR}/src/main/busqueda_tildes.c)
target_compile_options (versification_transition_test PRIVATE
  -ffunction-sections)
target_link_options (versification_transition_test PRIVATE
  -Wl,--gc-sections)
target_include_directories (versification_transition_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)
target_link_libraries (versification_transition_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)

add_executable (verse_navigation_sword_test EXCLUDE_FROM_ALL
  verse_navigation_sword_test.cc
  ${PROJECT_SOURCE_DIR}/src/main/verse_navigation.cc
  ${PROJECT_SOURCE_DIR}/src/backend/content_resolver.cc
  ${PROJECT_SOURCE_DIR}/src/backend/content_availability.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword_main.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword/sword_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc
  ${PROJECT_SOURCE_DIR}/src/main/busqueda_tildes.c)
target_compile_options (verse_navigation_sword_test PRIVATE
  -ffunction-sections)
target_link_options (verse_navigation_sword_test PRIVATE
  -Wl,--gc-sections)
target_include_directories (verse_navigation_sword_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)
target_link_libraries (verse_navigation_sword_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)

add_executable (note_action_test
  EXCLUDE_FROM_ALL
  note_action_test.c
  ${PROJECT_SOURCE_DIR}/src/main/note_action.c
  )

target_include_directories (note_action_test
  PRIVATE
  PkgConfig::Core
  )

target_link_libraries (note_action_test
  PRIVATE
  PkgConfig::Core
  )

if (BUILD_TESTING AND UNIX)
  add_test (
    NAME note_action_test
    COMMAND $<TARGET_FILE:note_action_test>
    )
endif ()

add_executable (author_commentary_panel_test
  EXCLUDE_FROM_ALL
  author_commentary_panel_test.c
  )

target_compile_definitions (author_commentary_panel_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_link_libraries (author_commentary_panel_test
  PRIVATE
  PkgConfig::Core
  )

if (BUILD_TESTING AND UNIX)
  add_test (
    NAME author_commentary_panel_test
    COMMAND $<TARGET_FILE:author_commentary_panel_test>
    )
endif ()

add_executable (author_commentary_content_test EXCLUDE_FROM_ALL
  author_commentary_content_test.cc
  ${PROJECT_SOURCE_DIR}/src/main/reference_transition.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword_main.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword/sword_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc
  ${PROJECT_SOURCE_DIR}/src/main/busqueda_tildes.c)
target_compile_options (author_commentary_content_test PRIVATE
  -ffunction-sections)
target_link_options (author_commentary_content_test PRIVATE
  -Wl,--gc-sections)
target_include_directories (author_commentary_content_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)
target_link_libraries (author_commentary_content_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)

if (BUILD_TESTING AND UNIX)
  add_test (
    NAME author_commentary_content_test
    COMMAND $<TARGET_FILE:author_commentary_content_test>
    )
endif ()

# TORRES-1835-BASELINE-101: the foundation of the 1832-1835 edition --
# manifest provenance, checksum-verified acquisition, the neutral model and
# the parser fail-safe that keeps an unrecognised boundary out of the
# previous verse. Pure python over in-memory fixtures; never touches the
# network or ~/.sword.
find_program (PYTHON3_EXECUTABLE NAMES python3)
if (BUILD_TESTING AND UNIX AND PYTHON3_EXECUTABLE)
  add_test (
    NAME torresamat1835_numeral_review_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_numeral_review.py
    )
  set_tests_properties (torresamat1835_numeral_review_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
  # La cola de rótulos que parecían ausentes: qué se vio en la imagen y
  # cómo se representa un rótulo impreso que el OCR no dejó.
  add_test (
    NAME torresamat1835_missing_headings_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_missing_headings.py
    )
  set_tests_properties (torresamat1835_missing_headings_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
  # El barrido que busca ese mismo daño en las 652 planas: rótulos
  # presentes cuya palabra de división el reconocimiento destrozó.
  add_test (
    NAME torresamat1835_corrupted_markers_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_corrupted_markers.py
    )
  set_tests_properties (torresamat1835_corrupted_markers_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
  # El rótulo estaba en el reconocimiento con la PALABRA rota («S A L M O
  # X L.»), así que ninguna lectura del texto podía verlo. Aquí vive la
  # recuperación de esas tres fronteras contra el facsímil.
  add_test (
    NAME torresamat1835_missing_boundaries_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_missing_boundaries.py
    )
  set_tests_properties (torresamat1835_missing_boundaries_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
  # Casar la palabra de división no es ser un rótulo: una inscripción del
  # salmo llegó a aceptarse como Ps 1. Aquí vive esa regresión.
  add_test (
    NAME torresamat1835_heading_validity_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_heading_validity.py
    )
  set_tests_properties (torresamat1835_heading_validity_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
  add_test (
    NAME torresamat1835_chapter_claims_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_chapter_claims.py
    )
  set_tests_properties (torresamat1835_chapter_claims_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
  add_test (
    NAME torresamat1835_recovery_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_recovery.py
    )
  set_tests_properties (torresamat1835_recovery_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
  add_test (
    NAME torresamat1835_image_reviews_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_image_reviews.py
    )
  set_tests_properties (torresamat1835_image_reviews_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
  add_test (
    NAME torresamat1835_book_candidates_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_book_candidates.py
    )
  set_tests_properties (torresamat1835_book_candidates_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
  add_test (
    NAME torresamat1835_book_boundaries_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_book_boundaries.py
    )
  set_tests_properties (torresamat1835_book_boundaries_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
  add_test (
    NAME torresamat1835_divisions_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_divisions.py
    )
  set_tests_properties (torresamat1835_divisions_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
  add_test (
    NAME torresamat1835_structure_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_structure.py
    )
  set_tests_properties (torresamat1835_structure_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
  add_test (
    NAME torresamat1835_parser_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_parser.py
    )
  set_tests_properties (torresamat1835_parser_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
  add_test (
    NAME torresamat1835_sources_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_sources.py
    )
  set_tests_properties (torresamat1835_sources_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
  add_test (
    NAME torresamat1835_baseline_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat1835/test_baseline.py
    )
  set_tests_properties (torresamat1835_baseline_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat1835
    )
endif ()

# The import side of TORRES-PSALM-TITLES-101: the deterministic transform
# that marks a psalm inscription <seg type="x-psalm-title"> without moving
# its verse number. Pure python over in-memory fixtures; the SWORD
# round-trips inside it skip themselves when the tools are absent.
find_program (PYTHON3_EXECUTABLE NAMES python3)
if (BUILD_TESTING AND UNIX AND PYTHON3_EXECUTABLE)
  add_test (
    NAME torresamat_titulos_pipeline_test
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/torresamat/test_titulos.py
    )
  set_tests_properties (torresamat_titulos_pipeline_test PROPERTIES
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/scripts/torresamat
    )
endif ()

add_executable (psalm_title_render_test EXCLUDE_FROM_ALL
  psalm_title_render_test.cc
  ${PROJECT_SOURCE_DIR}/src/main/psalm_title.cc
  ${PROJECT_SOURCE_DIR}/src/backend/content_availability.cc
  )
target_compile_definitions (psalm_title_render_test PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}")
target_include_directories (psalm_title_render_test PRIVATE
  ${PROJECT_SOURCE_DIR}/src PkgConfig::Core)
target_link_libraries (psalm_title_render_test PRIVATE PkgConfig::Core)

if (BUILD_TESTING AND UNIX)
  add_test (
    NAME psalm_title_render_test
    COMMAND $<TARGET_FILE:psalm_title_render_test>
    )
endif ()

add_executable (torresamat_psalter_structure_test EXCLUDE_FROM_ALL
  torresamat_psalter_structure_test.cc
  ${PROJECT_SOURCE_DIR}/src/main/psalm_title.cc
  ${PROJECT_SOURCE_DIR}/src/main/reference_transition.cc
  ${PROJECT_SOURCE_DIR}/src/backend/content_availability.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword_main.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword/sword_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc
  ${PROJECT_SOURCE_DIR}/src/main/busqueda_tildes.c)
target_compile_options (torresamat_psalter_structure_test PRIVATE
  -ffunction-sections)
target_link_options (torresamat_psalter_structure_test PRIVATE
  -Wl,--gc-sections)
target_include_directories (torresamat_psalter_structure_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)
target_link_libraries (torresamat_psalter_structure_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)

if (BUILD_TESTING AND UNIX)
  add_test (
    NAME torresamat_psalter_structure_test
    COMMAND $<TARGET_FILE:torresamat_psalter_structure_test>
    )
endif ()

add_executable (uri_navigation_v11n_test EXCLUDE_FROM_ALL
  uri_navigation_v11n_test.cc
  ${PROJECT_SOURCE_DIR}/src/main/reference_transition.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword_main.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword/sword_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc
  ${PROJECT_SOURCE_DIR}/src/main/busqueda_tildes.c)
target_compile_options (uri_navigation_v11n_test PRIVATE -ffunction-sections)
target_link_options (uri_navigation_v11n_test PRIVATE -Wl,--gc-sections)
target_include_directories (uri_navigation_v11n_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)
target_link_libraries (uri_navigation_v11n_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)

if (BUILD_TESTING AND UNIX)
  add_test (
    NAME uri_navigation_v11n_test
    COMMAND $<TARGET_FILE:uri_navigation_v11n_test>
    )
endif ()

add_executable (author_commentary_header_test EXCLUDE_FROM_ALL
  author_commentary_header_test.cc
  ${PROJECT_SOURCE_DIR}/src/main/interlineal.cc
  ${PROJECT_SOURCE_DIR}/src/main/reference_transition.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword_main.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword/sword_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc
  ${PROJECT_SOURCE_DIR}/src/main/busqueda_tildes.c)
target_compile_options (author_commentary_header_test PRIVATE
  -ffunction-sections -fdata-sections)
target_link_options (author_commentary_header_test PRIVATE
  -Wl,--gc-sections)
target_include_directories (author_commentary_header_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)
target_link_libraries (author_commentary_header_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)

if (BUILD_TESTING AND UNIX)
  add_test (
    NAME author_commentary_header_test
    COMMAND $<TARGET_FILE:author_commentary_header_test>
    )
endif ()

add_executable (compare_canonical_body_test EXCLUDE_FROM_ALL
  compare_canonical_body_test.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword_main.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword/sword_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc
  ${PROJECT_SOURCE_DIR}/src/main/busqueda_tildes.c)
target_compile_options (compare_canonical_body_test PRIVATE
  -ffunction-sections)
target_link_options (compare_canonical_body_test PRIVATE
  -Wl,--gc-sections)
target_include_directories (compare_canonical_body_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)
target_link_libraries (compare_canonical_body_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)

if (BUILD_TESTING AND UNIX)
  add_test (
    NAME compare_canonical_body_test
    COMMAND $<TARGET_FILE:compare_canonical_body_test>
    )
endif ()

add_executable (content_resolver_sword_test EXCLUDE_FROM_ALL
  content_resolver_sword_test.cc
  ${PROJECT_SOURCE_DIR}/src/backend/content_resolver.cc
  ${PROJECT_SOURCE_DIR}/src/backend/content_availability.cc
  ${PROJECT_SOURCE_DIR}/src/backend/quoted_heading.cc
  ${PROJECT_SOURCE_DIR}/src/backend/source_quirks.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword_main.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sword/sword_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc
  ${PROJECT_SOURCE_DIR}/src/main/busqueda_tildes.c)
target_compile_options (content_resolver_sword_test PRIVATE
  -ffunction-sections)
target_link_options (content_resolver_sword_test PRIVATE
  -Wl,--gc-sections)
target_include_directories (content_resolver_sword_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)
target_link_libraries (content_resolver_sword_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)
target_compile_options (sword_backend_key_lifecycle_test PRIVATE
  -ffunction-sections)
target_link_options (sword_backend_key_lifecycle_test PRIVATE
  -Wl,--gc-sections)
target_include_directories (sword_backend_key_lifecycle_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)
target_link_libraries (sword_backend_key_lifecycle_test PRIVATE
  PkgConfig::Core PkgConfig::Gtk PkgConfig::Gnome PkgConfig::Soup
  PkgConfig::Sword)
# OSIS importer tests use the project's existing libxml2 dependency.
pkg_check_modules(XML2 REQUIRED IMPORTED_TARGET libxml-2.0)
