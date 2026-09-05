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
  navbar_versekey.cc
  nube_palabras.cc
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
  prayerlists.cc
  previewer.cc
  search_dialog.cc
  search_sidebar.cc
  settings.c
  sidebar.cc
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
