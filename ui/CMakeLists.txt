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
# install ui files
install (FILES
  bookmarks.gtkbuilder
  folder.gtkbuilder
  editor_link_dialog.gtkbuilder
  export-dialog.gtkbuilder
  markverse.gtkbuilder
  module-manager.gtkbuilder
  navbar_versekey.gtkbuilder
  nube-palabras.gtkbuilder
  diccionario.gtkbuilder
  prefs.gtkbuilder
  search-dialog.gtkbuilder
  selector-prefs.gtkbuilder
  xi-export-bookmarks.gtkbuilder
  xi-menus.gtkbuilder
  xi-menus-popup.gtkbuilder
  xi-splash.gtkbuilder
  gtk_webedit.ui
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}
  COMPONENT data
  )
# common stuff
install (FILES
  editor_note.xml
  editor_studypad.xml
  arrow_down_box.png
  arrow_down.png
  arrow_up_box.png
  arrow_up.png
  biblesync-v1-30.png
  biblesync-v1-50.png
  biblesync_icon-16.png
  chat_icon-16.png
  dlg-un-16.png
  epiphany-bookmarks.png
  gnome-session.png
  gnome-sound-properties.png
  gnome-windows.png
  logo.png
  mailing_list_icon-16.png
  new_tab_button.png
  silk-edit-bookmarks.png
  sword3.png
  sword_icon-16.png
  sword.png
  translation_icon-16.png
  xiphos-x-16.png
  languages
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}
  COMPONENT data
  )
