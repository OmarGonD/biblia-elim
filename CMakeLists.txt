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

# the oldest stable cmake version we support
cmake_minimum_required (VERSION 3.11 FATAL_ERROR)
cmake_policy(VERSION 3.11)

### set project name (version set later)
project (biblia-elim C CXX)

include (CTest)

# set commonly used Xiphos variables:
# Xiphos website
set (XIPHOS_WEBSITE "https://github.com/OmarGonD/biblia-elim")

# Xiphos issues report website
set (XIPHOS_BUG_REPORT "https://github.com/OmarGonD/biblia-elim/issues")

# Xiphos release date, (read from 'ChangeLog')
# read last entry in ChangeLog
file(STRINGS ${PROJECT_SOURCE_DIR}/ChangeLog
  ChangeLog_lastentry REGEX "^20[0-9][0-9]-[0-9][0-9]" LIMIT_COUNT 1)
# get year
string(SUBSTRING ${ChangeLog_lastentry} 0 4 ChangeLog_year)
# get month's number
string(SUBSTRING ${ChangeLog_lastentry} 5 2 ChangeLog_month)
# transform month's number into month's name
list(APPEND month_names "XXX;Jan;Feb;Mar;Apr;May;Jun;Jul;Aug;Sep;Oct;Nov;Dec")
list(GET month_names ${ChangeLog_month} ChangeLog_month)
# generate release date string
string(CONFIGURE "@ChangeLog_month@ @ChangeLog_year@" XIPHOS_RELEASE_DATE)


# make sure it's an out-of-stream build
file(TO_CMAKE_PATH "${PROJECT_BINARY_DIR}/CMakeLists.txt" LOC_PATH)
if(EXISTS "${LOC_PATH}")
  message(FATAL_ERROR "You cannot build in the source directory (or any directory with a CMakeLists.txt file). Please make an out-of-stream build subdirectory.")
endif()


# tell cmake where its modules can be found in our project directory
list (APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
list (APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)

# parse the current version based on git tags, then cache a result on behalf
# of source tarballs that don't have the repo .git metadata
set(SOURCE_VERSION_CACHE "${PROJECT_SOURCE_DIR}/cmake/source_version.txt")
if(EXISTS "${PROJECT_SOURCE_DIR}/.git")
	find_package(Git)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --tag
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
		RESULT_VARIABLE GIT_EXIT_CODE
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if(GIT_EXIT_CODE EQUAL 0 AND GIT_DESCRIBE_VERSION)
		set(VERSION ${GIT_DESCRIBE_VERSION})
	else()
		file(STRINGS ${SOURCE_VERSION_CACHE} VERSION)
		message(STATUS "Using cached Biblia Elim version ${VERSION}")
	endif()
	string(REGEX REPLACE "^(([0-9]+\\.?)+)-([0-9]+)-.*$" "\\1.\\3" VERSION "${VERSION}")
	file(WRITE ${SOURCE_VERSION_CACHE} "${VERSION}")
	message(STATUS "Setting version to ${VERSION}")
else()
	file(STRINGS ${SOURCE_VERSION_CACHE} VERSION)
endif()

# configure project name and version
project (biblia-elim
	VERSION "${VERSION}")

# set install directory variables as defined by the GNU Coding Standards.
# e.g. CMAKE_INSTALL_DATAROOTDIR; CMAKE_INSTALL_FULL_BINDIR;
# CMAKE_INSTALL_FULL_DOCDIR...
include (GNUInstallDirs)

# set CMAKE options that the user can optionally select ON or OFF.
include (XiphosOptions)

# find needed tools for building Xiphos
include (XiphosBuildTools)

# find dependencies
include (XiphosDependencies)

# set flags
include (XiphosFlags)

# create config.h
include (XiphosConfig_h)

# First-run language selected by the package builder or installer.
set(BIBLIA_ELIM_INSTALL_LANGUAGE "system" CACHE STRING "First-run interface/devotional language")
set(_elim_languages system en_GB es fr ko_KR zh_CN zh_TW pt_BR pt)
if(NOT BIBLIA_ELIM_INSTALL_LANGUAGE IN_LIST _elim_languages)
  message(FATAL_ERROR "Unsupported BIBLIA_ELIM_INSTALL_LANGUAGE")
endif()
file(WRITE ${PROJECT_BINARY_DIR}/installation-language "${BIBLIA_ELIM_INSTALL_LANGUAGE}\n")
install(FILES ${PROJECT_BINARY_DIR}/installation-language
  DESTINATION ${CMAKE_INSTALL_DATADIR}/biblia-elim COMPONENT Devotional)

# build xiphos
add_subdirectory(win32)
add_subdirectory(src/backend)
add_subdirectory(src/editor)
add_subdirectory(src/main)
add_subdirectory(src/webkit)
add_subdirectory(src/xiphos_html)
add_subdirectory(src/gtk)

# build Biblia Elim's URL navigation helper
add_subdirectory(src/examples)

# pruebas (no entran en la compilación normal). Son de Linux (X11/XTest, xvfb):
# al compilar para Windows no se configuran.
if (NOT WIN32)
  add_subdirectory(tests)
endif ()

# build other stuff
add_subdirectory(pixmaps)
add_subdirectory(ui)
add_subdirectory(po)
if (NOT APPLE)
  add_subdirectory(desktop)
endif ()
add_subdirectory(doc)
add_subdirectory(help)

# create packages
add_subdirectory(cpack)

# report
include (XiphosReport)
# End
