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

# install win32 required files

# define common dirs
if (WIN32)
  # set icon file
  set(XIPHOS_MAIN_ICON "${CMAKE_SOURCE_DIR}/pixmaps/gs2-48x48.ico")
  # generate resource file
    configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/res/xiphos-winres.rc.in
    ${CMAKE_BINARY_DIR}/src/gtk/xiphos-winres.rc
  )
  # don't install in /usr
  if (CMAKE_INSTALL_PREFIX MATCHES "^/usr")
    set (CMAKE_INSTALL_PREFIX "install")
  endif (CMAKE_INSTALL_PREFIX MATCHES "^/usr")

  if (CMAKE_CROSSCOMPILING)
    # define MinGW dirs
    set (SYS_ROOT_BIN ${CMAKE_FIND_ROOT_PATH}/bin)
    set (SYS_ROOT_LIB ${CMAKE_FIND_ROOT_PATH}/lib)
    set (SYS_ROOT_SHARE ${CMAKE_FIND_ROOT_PATH}/share)
    set (SYS_ROOT_ETC ${CMAKE_FIND_ROOT_PATH}/etc)
  else (CMAKE_CROSSCOMPILING)
    # plain Windows©
    message (FATAL_ERROR "Windows© build is not yet implemented, please define SYS_ROOT_BIN, SYS_ROOT_LIB... variables")
  endif (CMAKE_CROSSCOMPILING)

  # add required files for cross-compiling
  # X11/keysymdef.h - use host system's copy rather than vendored one
  find_path(X11_INCLUDE_DIR X11/keysymdef.h
  PATHS /usr/include /usr/local/include
  NO_CMAKE_FIND_ROOT_PATH
  REQUIRED)

  # install biblesync.dll
  if (EXISTS ../biblesync/libbiblesync.dll)
    install(FILES
      ../biblesync/libbiblesync.dll
      DESTINATION ${CMAKE_INSTALL_BINDIR}
      COMPONENT binaries
      )
  elseif (EXISTS ${CMAKE_FIND_ROOT_PATH}/bin/libbiblesync.dll)
    install(FILES
      ${SYS_ROOT_BIN}/libbiblesync.dll
      DESTINATION ${CMAKE_INSTALL_BINDIR}
      COMPONENT binaries
      )
  endif (EXISTS ../biblesync/libbiblesync.dll)


  # Install the MinGW runtime files from the sysroot. Package versions change
  # DLL names, so keeping a hard-coded list here makes releases fragile.
  install(DIRECTORY ${SYS_ROOT_BIN}/
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT binaries
    FILES_MATCHING
    PATTERN "*.dll"
    PATTERN "*.exe"
  )


  # install required libraries
  install(DIRECTORY
    ${SYS_ROOT_LIB}/enchant
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT binaries
    )


  # install required shared files
  install(DIRECTORY
    ${SYS_ROOT_SHARE}/sword
    ${SYS_ROOT_SHARE}/enchant
    #  ${SYS_ROOT_SHARE}/gtkhtml-3.14
    DESTINATION ${CMAKE_INSTALL_DATADIR}
    COMPONENT data
    )


  # install required system conf files
  install(DIRECTORY
    ${SYS_ROOT_ETC}/fonts
    ${SYS_ROOT_ETC}/pango
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}
    COMPONENT data
    )


  # install win32 specific files
  install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/bin/sword.conf
    ${CMAKE_CURRENT_SOURCE_DIR}/bin/utils.bat
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT binaries
    )
  install(DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/etc
    ${CMAKE_CURRENT_SOURCE_DIR}/festival
    ${CMAKE_CURRENT_SOURCE_DIR}/share
    DESTINATION ${CMAKE_INSTALL_DATADIR}/..
    COMPONENT data
    )

  # install svg icon
  install(FILES
    ${PROJECT_SOURCE_DIR}/pixmaps/xiphos.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    COMPONENT data
    )

  # install icon theme (hicolor + adwaita, for GTK symbolic icons)
  install(DIRECTORY
    ${SYS_ROOT_SHARE}/icons
    DESTINATION ${CMAKE_INSTALL_DATADIR}
    COMPONENT data
    )

  # install gdk-pixbuf loaders (needed to render icons/pixbufs at runtime)
  install(DIRECTORY
    ${SYS_ROOT_LIB}/gdk-pixbuf-2.0
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    COMPONENT binaries
    )

  # install GSettings schemas (required by GtkFileChooserButton and others,
  # otherwise fatal GLib-GIO-ERROR "No GSettings schemas are installed")
  install(DIRECTORY
    ${SYS_ROOT_SHARE}/glib-2.0/schemas
    DESTINATION ${CMAKE_INSTALL_DATADIR}/glib-2.0
    COMPONENT data
    )

  # compile the schemas at build time (produces gschemas.compiled)
  find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas)
  install(CODE "
    execute_process(COMMAND \${GLIB_COMPILE_SCHEMAS} \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas\")
    "
    COMPONENT data
    )


  # update farsi gtk+ .mo file
  execute_process(
    COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} ${PROJECT_SOURCE_DIR}/win32/po/fa.po -o ${CMAKE_CURRENT_BINARY_DIR}/gtk20.mo
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    TIMEOUT 5
    )

  # install farsi .mo file
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/gtk20.mo
    DESTINATION ${CMAKE_INSTALL_DATADIR}/locale/fa/LC_MESSAGES/
    COMPONENT data
    )
endif(WIN32)
