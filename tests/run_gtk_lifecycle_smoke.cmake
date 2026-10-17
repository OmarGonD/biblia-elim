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
    "INSERT INTO verses(book_id, chapter, verse, text) VALUES(1, 1, 2, 'The waters were gathered together.'), (1, 2, 1, 'The heavens and the earth were finished.'), (1, 3, 1, 'The serpent was more subtle.'); INSERT INTO verses_fts(rowid, text) SELECT rowid, text FROM verses WHERE book_id = 1 AND (verse = 2 OR chapter IN (2, 3));"
    RESULT_VARIABLE verse_status)
  if(NOT verse_status EQUAL 0)
    message(FATAL_ERROR "Could not add the second Genesis verse to the smoke fixture")
  endif()
endforeach()

# Parallel headings must come from the SQLite content model.
foreach(smoke_module smoke other)
  execute_process(COMMAND "${SQLITE3}" "${module_dir}/${smoke_module}.sqlite"
    "CREATE TABLE IF NOT EXISTS headings(book_id INTEGER, chapter INTEGER, verse INTEGER, sequence INTEGER, text TEXT); INSERT OR REPLACE INTO headings VALUES(40,3,16,0,'SQLite parallel heading'); UPDATE metadata SET value='true' WHERE key='feature.headings';"
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

# Generic edition with a changed verse and a comment-only verse.
file(COPY_FILE "${module_dir}/smoke.sqlite" "${module_dir}/EditionFixture.sqlite")
execute_process(COMMAND "${SQLITE3}" "${module_dir}/EditionFixture.sqlite" "
 UPDATE metadata SET value='EditionFixture' WHERE key='module_id';
 INSERT INTO metadata VALUES('edition_format','biblia-elim-edition'),('edition_version','1'),
 ('project_id','EditionFixture'),('source_module_id','SpaRV'),('source_name','Original fixture'),
 ('source_license','Public Domain'),('source_fingerprint','fixture-original');
 CREATE TABLE edition_base_verses AS SELECT book_id,chapter,verse,text FROM verses;
 UPDATE verses SET text='Edited creation text.' WHERE book_id=1 AND chapter=1 AND verse=1;
 CREATE TABLE book_comments(book_id INTEGER PRIMARY KEY,comment TEXT NOT NULL,modified INTEGER NOT NULL);
 CREATE TABLE verse_comments(book_id INTEGER,chapter INTEGER,verse INTEGER,comment TEXT NOT NULL,modified INTEGER NOT NULL,PRIMARY KEY(book_id,chapter,verse));
 INSERT INTO verse_comments VALUES(1,1,2,'A study note title'||char(10)||'A short explanatory lead.'||char(10)||char(10)||'Comment only https://example.org/research',0);
 CREATE TABLE editorial_decisions(book_id INTEGER,chapter INTEGER,verse INTEGER,proposal TEXT NOT NULL,rationale TEXT NOT NULL,evidence TEXT NOT NULL,confidence TEXT NOT NULL,status TEXT NOT NULL,modified INTEGER NOT NULL,PRIMARY KEY(book_id,chapter,verse));
 INSERT INTO editorial_decisions VALUES(1,1,1,'Edited creation text.','Editorial rationale','https://example.org/research','Media','Pendiente de revisión',0);"
 RESULT_VARIABLE edition_status)
if(NOT edition_status EQUAL 0)
 message(FATAL_ERROR "Could not create editorial comparison fixture")
endif()

file(COPY_FILE "${module_dir}/EditionFixture.sqlite" "${module_dir}/EmptyEditionFixture.sqlite")
execute_process(COMMAND "${SQLITE3}" "${module_dir}/EmptyEditionFixture.sqlite" "
 UPDATE metadata SET value='EmptyEditionFixture' WHERE key IN ('module_id','project_id');
 UPDATE verses SET text=(SELECT b.text FROM edition_base_verses b WHERE b.book_id=verses.book_id AND b.chapter=verses.chapter AND b.verse=verses.verse);
 DELETE FROM verse_comments; DELETE FROM editorial_decisions;"
 RESULT_VARIABLE empty_edition_status)
if(NOT empty_edition_status EQUAL 0)
 message(FATAL_ERROR "Could not create empty editable Bible fixture")
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
          "LANGUAGE=es"
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
