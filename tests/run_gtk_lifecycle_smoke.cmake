string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run_id)
set(runtime_dir "${RUNTIME_ROOT}/gtk-lifecycle-${run_id}")
set(module_dir "${runtime_dir}/modules")
set(home_dir "${runtime_dir}/home")
set(config_dir "${runtime_dir}/config")
set(data_dir "${runtime_dir}/data")
set(cache_dir "${runtime_dir}/cache")
set(state_dir "${runtime_dir}/state")
file(MAKE_DIRECTORY "${module_dir}" "${home_dir}" "${config_dir}"
                    "${data_dir}" "${cache_dir}" "${state_dir}")

execute_process(
  COMMAND "${SQLITE3}" "${module_dir}/smoke.sqlite"
  INPUT_FILE "${FIXTURE}"
  RESULT_VARIABLE fixture_status
  ERROR_VARIABLE fixture_stderr)
if(NOT fixture_status EQUAL 0)
  file(REMOVE_RECURSE "${runtime_dir}")
  message(FATAL_ERROR "Could not create GTK smoke fixture: ${fixture_stderr}")
endif()

# A second Bible, so notes written in one can be checked from the other.
file(READ "${FIXTURE}" other_fixture)
string(REPLACE "'FakeBible'" "'OtherBible'" other_fixture "${other_fixture}")
file(WRITE "${runtime_dir}/other-bible.sql" "${other_fixture}")
execute_process(
  COMMAND "${SQLITE3}" "${module_dir}/other.sqlite"
  INPUT_FILE "${runtime_dir}/other-bible.sql"
  RESULT_VARIABLE fixture_status
  ERROR_VARIABLE fixture_stderr)
if(NOT fixture_status EQUAL 0)
  file(REMOVE_RECURSE "${runtime_dir}")
  message(FATAL_ERROR "Could not create second GTK smoke fixture: ${fixture_stderr}")
endif()

# A second verse where chapter panes find verses (Genesis is the only book
# the fixture numbers canonically), so the parallel page's focus band has
# somewhere to move to.
foreach(smoke_module smoke other)
  execute_process(COMMAND "${SQLITE3}" "${module_dir}/${smoke_module}.sqlite"
    "INSERT INTO verses(book_id, chapter, verse, text) VALUES(1, 1, 2, 'The waters were gathered together.'); INSERT INTO verses_fts(rowid, text) SELECT rowid, text FROM verses WHERE book_id = 1 AND chapter = 1 AND verse = 2;"
    RESULT_VARIABLE verse_status)
  if(NOT verse_status EQUAL 0)
    message(FATAL_ERROR "Could not add the second Genesis verse to the smoke fixture")
  endif()
endforeach()

# Parallel headings must come from the SQLite content model.
foreach(smoke_module smoke other)
  execute_process(COMMAND "${SQLITE3}" "${module_dir}/${smoke_module}.sqlite"
    "CREATE TABLE headings(book_id INTEGER, chapter INTEGER, verse INTEGER, sequence INTEGER, text TEXT); INSERT INTO headings VALUES(40,3,16,0,'SQLite parallel heading'); UPDATE metadata SET value='true' WHERE key='feature.headings';"
    RESULT_VARIABLE heading_status)
  if(NOT heading_status EQUAL 0)
    message(FATAL_ERROR "Could not add SQLite heading fixture")
  endif()
endforeach()

# Parallel rows: the second Bible's Genesis 1:1 runs longer and only the
# first gives 1:2 a heading, so lining the rows up takes space on both
# sides.
execute_process(COMMAND "${SQLITE3}" "${module_dir}/other.sqlite"
  "UPDATE verses SET text = 'In the beginning God created the heaven and the earth, and the earth was without form, and void, and darkness was upon the face of the deep, and the Spirit moved upon the face of the waters.' WHERE book_id = 1 AND chapter = 1 AND verse = 1;"
  RESULT_VARIABLE row_status)
if(NOT row_status EQUAL 0)
  message(FATAL_ERROR "Could not lengthen the second Bible's Genesis 1:1")
endif()
execute_process(COMMAND "${SQLITE3}" "${module_dir}/smoke.sqlite"
  "INSERT INTO headings VALUES(1,1,2,0,'The waters gathered');"
  RESULT_VARIABLE row_status)
if(NOT row_status EQUAL 0)
  message(FATAL_ERROR "Could not add a Genesis 1:2 heading to the first Bible")
endif()

execute_process(
  COMMAND "${XVFB_RUN}" -a -s "-screen 0 1280x800x24"
          "${CMAKE_COMMAND}" -E env
          "HOME=${home_dir}"
          "XDG_CONFIG_HOME=${config_dir}"
          "XDG_DATA_HOME=${data_dir}"
          "XDG_CACHE_HOME=${cache_dir}"
          "XDG_STATE_HOME=${state_dir}"
          "GDK_BACKEND=x11"
          "BIBLIA_ELIM_GTK_LIFECYCLE_SMOKE=1"
          "BIBLIA_ELIM_UI_LOAD_DEBUG=1"
          "${APP}" "--backend=sqlite:${module_dir}"
  RESULT_VARIABLE app_status
  OUTPUT_VARIABLE app_stdout
  ERROR_VARIABLE app_stderr
  TIMEOUT 35)

set(output "${app_stdout}\n${app_stderr}")

# Some developer sandboxes cannot create X11 sockets. That is an unavailable
# test environment, not an application result; normal CI runs the same command.
if(output MATCHES "Xvfb failed to start|Cannot establish any listening sockets|Unable to init server|xvfb-run:[^\n]*kill:")
  message("gtk_lifecycle_smoke_skipped=no-usable-xvfb")
  file(REMOVE_RECURSE "${runtime_dir}")
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.29)
    cmake_language(EXIT 77)
  endif()
  return()
endif()

if(NOT app_status EQUAL 0)
  message(FATAL_ERROR "GTK lifecycle smoke exited ${app_status}:\n${output}")
endif()
if(NOT IS_DIRECTORY "${config_dir}/xiphos")
  message(FATAL_ERROR "Fresh startup did not create ${config_dir}/xiphos:\n${output}")
endif()
if(output MATCHES "(Xiphos|Biblia Elim) can(not| not) create directory|cannot continue")
  message(FATAL_ERROR "Fatal profile-creation dialog was emitted:\n${output}")
endif()
if(NOT output MATCHES "gtk_lifecycle_smoke_failures=0")
  message(FATAL_ERROR "GTK lifecycle smoke did not complete:\n${output}")
endif()
foreach(surface bible-main bible-compare commentary sidebar-previewer
                lower-previewer)
  if(NOT output MATCHES "${surface} CREATE" OR
     NOT output MATCHES "${surface} SHOW" OR
     NOT output MATCHES "${surface} (RENDERER_)?MAP")
    message(FATAL_ERROR
      "Renderer ${surface} did not complete CREATE/SHOW/MAP:\n${output}")
  endif()
endforeach()
if(output MATCHES "(Gtk|Gdk)-(WARNING|CRITICAL|ERROR)|GTK_LIFECYCLE_SMOKE_DIAGNOSTIC|negative size allocation|unanchored")
  message(FATAL_ERROR "GTK lifecycle diagnostic detected:\n${output}")
endif()

message("${output}")
file(REMOVE_RECURSE "${runtime_dir}")
