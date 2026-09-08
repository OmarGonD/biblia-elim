# Biblia Elim - pruebas
#
# No entran en la compilación normal: se piden a mano.
#
#   cmake --build build --target pulpito_test && ./build/tests/pulpito_test

add_executable (pulpito_test
  EXCLUDE_FROM_ALL
  pulpito_test.c
  ${PROJECT_SOURCE_DIR}/src/main/pulpito_pasos.c
  )

target_link_libraries (pulpito_test
  PRIVATE
  PkgConfig::Core
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

add_executable (enriched_content_benchmark EXCLUDE_FROM_ALL
  enriched_content_benchmark.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc)
target_include_directories (enriched_content_benchmark PRIVATE PkgConfig::Core PkgConfig::Sqlite3)
target_link_libraries (enriched_content_benchmark PRIVATE PkgConfig::Core PkgConfig::Sqlite3)

add_executable (osis_importer_test EXCLUDE_FROM_ALL
  osis_importer_test.cc
  ${PROJECT_SOURCE_DIR}/src/backend/osis_importer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_module_writer.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc)
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

add_executable (sqlite_backend_full_probe
  EXCLUDE_FROM_ALL
  sqlite_backend_full_probe.cc
  ${PROJECT_SOURCE_DIR}/src/backend/sqlite/sqlite_bible_backend.cc
  ${PROJECT_SOURCE_DIR}/src/backend/bible_book_map.cc
  ${PROJECT_SOURCE_DIR}/src/backend/strong_id.cc
  )
target_include_directories (sqlite_backend_full_probe PRIVATE PkgConfig::Core PkgConfig::Sqlite3)
target_link_libraries (sqlite_backend_full_probe PRIVATE PkgConfig::Core PkgConfig::Sqlite3)

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
# OSIS importer tests use the project's existing libxml2 dependency.
pkg_check_modules(XML2 REQUIRED IMPORTED_TARGET libxml-2.0)
