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

# Create config.h

message (STATUS "Generating config.h")

# pixmaps dir
set (PACKAGE_PIXMAPS_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")

# locale dir
set (PACKAGE_LOCALE_DIR "${CMAKE_INSTALL_FULL_LOCALEDIR}")

# share dir
set (SHARE_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")

# textdomain
set (GETTEXT_PACKAGE "${PROJECT_NAME}")

# GTK4 has one native editor path.  The old WebKit/GTKHTML editors use
# removed GTK3 widgets and must not be selectable in a GTK4 build.
if (NOT GTKTVEDITOR)
  message (FATAL_ERROR
    "GTKTVEDITOR=OFF is not supported by the GTK4 build; use the native GtkTextView editor")
endif ()
set (USE_GTKTVeditor ON)
message (STATUS "Editor: GtkTextView (GTK4 native)")

# Gtk
set (USE_GTKBUILDER ON)

# select WebkitGtk
if (NOT WEBKIT1)
  set (USE_WEBKIT2 ON)
endif ()

# enable dBus
if (DBUS)
  set (HAVE_DBUS ON)
endif()


# i18n
include (CheckIncludeFiles)
check_include_files ("locale.h;libintl.h" ENABLE_NLS)

# strcasestr
include (CheckFunctionExists)
check_function_exists (strcasestr HAVE_STRCASESTR)


# generate config.h
set (CONFIG_H ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file (
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/config.h
  )
