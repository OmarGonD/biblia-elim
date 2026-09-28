# Xiphos build script
#
# Copyright (C) 2018 Xiphos Development Team
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Library General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <https://www.gnu.org/licenses/>.
#


# building main
add_library (main
  STATIC
  biblesync_glue.cc
  configs.cc
  display.cc
  notes_store.c
  note_value.c
  notes_exchange.c
  diccionario.cc
  export_passage.cc
  global_ops.cc
  import_andbible.cc
  import_modfile.cc
  interlineal.cc
  lists.cc
  lectura_sync.cc
  main.c
  mod_mgr.cc
  modulecache.cc
  module_dialogs.cc
  navbar_book.cc
  navbar_book_dialog.cc
  navbar.cc
  navbar_key.cc
  navbar_versekey.cc
  note_action.c
  reference_transition.cc
  reading_focus.c
  reading_window.c
  picker_entry.c
  wheel_scroll.c
  navigation_prefs.c
  navigation_prefs_settings.c
  verse_navigation.cc
  nube_palabras.cc
  nube_mayusculas.c
  planes_lectura.c
  pulpito.cc
  pulpito_pasos.c
  memorizacion.c
  busqueda_tildes.c
  buscar_notas.c
  morfologia.c
  glosa.c
  racha.c
  texto_verso.c
  tiempo_lectura.c
  recordatorio.c
  versiculo_dia.c
  parallel_view.cc
  psalm_title.cc
  prayerlists.cc
  previewer.cc
  search_dialog.cc
  search_sidebar.cc
  settings.c
  startup_profile.c
  startup_diagnostics.cc
  zoom_state.c
  font_size.c
  sidebar.cc
	strong_interaction.cc
	strong_lexicon_startup.cc
	strong_ui.cc
  sword.cc
  sword_treekey.cc
  tab_history.c
  testimonios.c
  url.cc
  xml.c
  )

# Add alias
add_library(Xiphos::Main ALIAS main)


# specify include directories to use when compiling
target_include_directories(main
  PRIVATE
  Threads::Threads
  PkgConfig::Core
  PkgConfig::Gtk
  PkgConfig::Gnome
  PkgConfig::Soup
  PkgConfig::Sword
  PkgConfig::Biblesync
  PkgConfig::Sqlite3
  )

# specify libraries or flags to use when linking
target_link_libraries(main
  PRIVATE
  m
  Threads::Threads
  PkgConfig::Core
  PkgConfig::Gtk
  PkgConfig::Gnome
  PkgConfig::Soup
  PkgConfig::Sword
  PkgConfig::Biblesync
  PkgConfig::Sqlite3
  )

IF (DBUS)
  target_include_directories (main
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
    )
ENDIF (DBUS)

if(WK_FOUND)
  target_compile_definitions(main
    PRIVATE
    PkgConfig::WK
    )
  target_link_libraries(main
    PRIVATE
    PkgConfig::WK
    )
endif()

# MORPH-109: generate the neutral Strong lexicon SQLite file from the
# bundled ui/strongs-elim.xml (Strong 1890 PD + Open Scriptures glosses,
# already used at runtime by the legacy interlinear path). Reproducible,
# build-time step -- mirrors how src/gtk/CMakeLists.txt turns
# ui/xiphos.gresource.xml into xiphos_resources.c: a generated artifact
# under the build directory, never committed to the source tree.
# main_bind_strong_lexicon() (strong_lexicon_startup.cc) looks for the
# result next to the running binary at startup, and degrades gracefully
# (no lexicon bound, today's bare-Strong-ID dialog behavior) if it is
# not there -- e.g. a build with Python unavailable.
find_program (PYTHON3_EXECUTABLE NAMES python3)
if (PYTHON3_EXECUTABLE)
  set (xiphos_strongs_xml ${PROJECT_SOURCE_DIR}/ui/strongs-elim.xml)
  set (xiphos_strongs_lexicon ${CMAKE_BINARY_DIR}/lexicon/strongs-elim.sqlite)
  add_custom_command (
    OUTPUT ${xiphos_strongs_lexicon}
    COMMAND ${PYTHON3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/scripts/generate_strongs_lexicon.py
            --xml ${xiphos_strongs_xml}
            --output ${xiphos_strongs_lexicon}
    DEPENDS ${xiphos_strongs_xml}
            ${PROJECT_SOURCE_DIR}/scripts/generate_strongs_lexicon.py
    COMMENT "Generating strongs-elim.sqlite lexicon from strongs-elim.xml"
    VERBATIM
    )
  add_custom_target (strongs_lexicon DEPENDS ${xiphos_strongs_lexicon})
  add_dependencies (main strongs_lexicon)
  target_compile_definitions (main PRIVATE
    XIPHOS_BUILD_STRONG_LEXICON="${xiphos_strongs_lexicon}")

  # Installed builds: alongside the other bundled data under SHARE_DIR
  # (see cmake/XiphosConfig_h.cmake), read back by the same startup
  # probe as a fallback when no build-tree copy is found.
  install (FILES ${xiphos_strongs_lexicon}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME})
else ()
  message (WARNING
    "python3 not found: Strong lexicon (MORPH-109) will not be generated; "
    "the word-detail dialog falls back to bare Strong IDs")
endif ()
