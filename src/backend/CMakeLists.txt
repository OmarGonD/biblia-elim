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

# build backend
pkg_check_modules(XML2 REQUIRED IMPORTED_TARGET libxml-2.0)

add_library (backend STATIC
  bible_backend.h
  bible_types.h
  bible_resources.h
  strong_id.cc
  strong_id.h
  morphology.cc
  morphology.h
  bible_lexicon.h
  sqlite/sqlite_strong_lexicon.cc
  sqlite/sqlite_strong_lexicon.h
  bible_book_map.cc
  bible_book_map.h
  gs_stringmgr.cpp
  module_manager.cc
  usfm_importer.cc
  usfm_importer.h
  osis_importer.cc
  osis_importer.h
  sqlite_module_manager.cc
  sqlite_module_manager.h
  sqlite/sqlite_bible_backend.cc
  sqlite/sqlite_bible_backend.h
  sqlite/sqlite_module_writer.cc
  sqlite/sqlite_module_writer.h
  sword_main.cc
  sword/sword_backend.cc
  sword/sword_backend.h
  )

add_executable (biblia-usfm-import
  EXCLUDE_FROM_ALL
  usfm_import_main.cc
  usfm_importer.cc
  usfm_importer.h
  bible_book_map.cc
  strong_id.cc
  morphology.cc
  bible_book_map.h
  sqlite/sqlite_module_writer.cc
  sqlite/sqlite_module_writer.h
  )
target_include_directories (biblia-usfm-import PRIVATE PkgConfig::Sqlite3)
target_link_libraries (biblia-usfm-import PRIVATE PkgConfig::Sqlite3)

add_executable (biblia-osis-import EXCLUDE_FROM_ALL
  osis_import_main.cc osis_importer.cc osis_importer.h
  bible_book_map.cc strong_id.cc morphology.cc
  sqlite/sqlite_module_writer.cc sqlite/sqlite_module_writer.h)
target_include_directories (biblia-osis-import PRIVATE PkgConfig::Core PkgConfig::Sqlite3 PkgConfig::XML2)
target_link_libraries (biblia-osis-import PRIVATE PkgConfig::Core PkgConfig::Sqlite3 PkgConfig::XML2)
# Add alias
add_library(Xiphos::Backend ALIAS backend)


# specify include directories to use when compiling
target_include_directories(backend
  PRIVATE
  Threads::Threads
  PkgConfig::Core
  PkgConfig::Gtk
  PkgConfig::Gnome
  PkgConfig::Soup
  PkgConfig::Sqlite3
  PkgConfig::XML2
  PkgConfig::Sword
  )

# specify libraries or flags to use when linking
target_link_libraries(backend
  PRIVATE
  Threads::Threads
  PkgConfig::Core
  PkgConfig::Gtk
  PkgConfig::Gnome
  PkgConfig::Soup
  PkgConfig::Sqlite3
  PkgConfig::XML2
  PkgConfig::Sword
  )

if(WK_FOUND)
  target_compile_definitions(backend
    PRIVATE
    PkgConfig::WK
    )
  target_link_libraries(backend
    PRIVATE
    PkgConfig::WK
    )
endif()
