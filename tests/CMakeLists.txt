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
  ${PROJECT_SOURCE_DIR}/src/gtk/panel_load_state.c
  )

target_include_directories (verse_navigation_readiness_test
  PRIVATE
  ${PROJECT_SOURCE_DIR}/src
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
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  ${PROJECT_SOURCE_DIR}/src/backend/morphology.cc
  ${PROJECT_SOURCE_DIR}/src/main/busqueda_tildes.c)
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
