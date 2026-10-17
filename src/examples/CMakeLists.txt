# Biblia Elim build script
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

# building biblia-elim-nav

if (HAVE_DBUS)
  message (STATUS "Configuring biblia-elim-nav target")

  # Generate client-side D-Bus bindings using gdbus-codegen
  add_custom_command (
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ipc-gdbus.h
    ${CMAKE_CURRENT_BINARY_DIR}/ipc-gdbus.c

    COMMAND gdbus-codegen
    --interface-prefix org.bibliaelim.
    --generate-c-code ipc-gdbus
    --c-namespace BibliaElim
    ${CMAKE_CURRENT_SOURCE_DIR}/../gtk/ipc-interface.xml
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/../gtk/ipc-interface.xml

    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating D-Bus client bindings for examples"
    )

  add_executable (
    biblia-elim-nav
    biblia-elim-nav.c
    ${CMAKE_CURRENT_BINARY_DIR}/ipc-gdbus.c
    )


  # specify include directories to use when compiling
  target_include_directories (biblia-elim-nav
    PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    Threads::Threads
    PkgConfig::Core
    PkgConfig::Gnome
    )

  # specify libraries or flags to use when linking
  target_link_libraries(biblia-elim-nav
    PRIVATE
    Threads::Threads
    PkgConfig::Core
    PkgConfig::Gnome
    )


  # install biblia-elim-nav
  install (TARGETS biblia-elim-nav
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT binaries)

endif (HAVE_DBUS)
