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

# configure desktop related files


if (UNIX)

  add_custom_target(desktop)

  # Ministerios Elim launcher entry and its translated display labels.
  message(STATUS "Configuring biblia-elim.desktop file")
  if (GETTEXT_FOUND)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/biblia-elim.desktop
      COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --desktop
              --template=${CMAKE_CURRENT_SOURCE_DIR}/biblia-elim.desktop
              -d ${PROJECT_SOURCE_DIR}/po -o biblia-elim.desktop
      COMMAND ${DESKTOP_FILE_VALIDATE} biblia-elim.desktop
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Configuring biblia-elim.desktop file"
      )
    install (FILES ${CMAKE_CURRENT_BINARY_DIR}/biblia-elim.desktop
      DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
      COMPONENT data)
    add_custom_target(desktop-entry
      DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/biblia-elim.desktop)
    add_dependencies(desktop desktop-entry)
  else ()
    install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/biblia-elim.desktop
      DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
      COMPONENT data)
  endif ()

   # Icon
   # for use in the programs menu
   # TODO: on uninstall run gtk-update-icon-cache
   # install icon
  install (FILES ${PROJECT_SOURCE_DIR}/pixmaps/ministerios-elim.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    COMPONENT data
    )

  # run gtk-update-icon-cache to create a cache file for icon theme
  IF (POSTINST)
    # check that gtk-update-icon-cache is installed
    find_program (GTK_UPDATE_ICON_CACHE gtk-update-icon-cache)
    if (GTK_UPDATE_ICON_CACHE)
      # gtk-update-icon-cache was found
      message (STATUS "Installing gtk-update-icon-cache.")
      # run gtk-update-icon as a post-building command
      set(_update_icon_cache_cmd
	"${GTK_UPDATE_ICON_CACHE} -f -t ${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor")
      install(CODE
	"execute_process(
       COMMAND ${CMAKE_COMMAND} -E chdir . ${_update_icon_cache_cmd}
       )"
	)
    else (GTK_UPDATE_ICON_CACHE)
      # gtk-update-icon-cache was not found
      message(WARNING "gtk-update-icon-cache not found.
  Make sure to call ${GTK_UPDATE_ICON_CACHE} -f -t \"${CMAKE_INSTALL_DATADIR}/icons/hicolor\" after install and uninstall")
    endif (GTK_UPDATE_ICON_CACHE)
  endif (POSTINST)



  # Ministerios Elim AppStream metadata.
  # allows the user to look at screenshots and a long description of the application
  # before it is installed on a Linux system.
  message (STATUS "Configuring the AppData metadata file")
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/biblia-elim.metainfo.xml
    COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} --xml
            --template "${CMAKE_CURRENT_SOURCE_DIR}/biblia-elim.metainfo.xml"
            -d "${PROJECT_SOURCE_DIR}/po" -o biblia-elim.metainfo.xml
    COMMAND ${APPSTREAMCLI} validate --no-net biblia-elim.metainfo.xml > validate.timestamp
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Configuring Ministerios Elim AppStream metadata"
    )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/biblia-elim.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
    COMPONENT data
    )

  add_custom_target(appdata
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/biblia-elim.metainfo.xml
    )
  add_dependencies(desktop appdata)


  # unix man pages
  # TODO: make man pages translatable
  message (STATUS "Building man pages")
  configure_file (biblia-elim.1.in biblia-elim.1)
  install (
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/biblia-elim.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
    COMPONENT data
    )

  if (HAVE_DBUS)
    configure_file (biblia-elim-nav.1.in biblia-elim-nav.1)
    install (
      FILES
      ${CMAKE_CURRENT_BINARY_DIR}/biblia-elim-nav.1
      DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
      COMPONENT data
      )
  endif (HAVE_DBUS)

  add_dependencies(biblia-elim desktop)

endif (UNIX)
