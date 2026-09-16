# Drives the real application through the clicked-note sequence and checks
# what the "Comentarios del autor" pane is actually painted with.
# See src/gtk/author_commentary_probe.c for the sequence and the checks.

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run_id)
set(runtime_dir "${RUNTIME_ROOT}/ac-probe-${run_id}")
foreach(sub home config data cache state)
  file(MAKE_DIRECTORY "${runtime_dir}/${sub}")
endforeach()

# The probe asserts against this edition's real notes. They live in the
# developer's SWORD library, exactly as author_commentary_content_test
# reads them; without it there is nothing to reproduce.
if(DEFINED ENV{SWORD_PATH})
  set(sword_path "$ENV{SWORD_PATH}")
else()
  set(sword_path "$ENV{HOME}/.sword")
endif()
if(NOT EXISTS "${sword_path}/mods.d")
  message("author_commentary_probe_skipped=no-sword-library")
  file(REMOVE_RECURSE "${runtime_dir}")
  return()
endif()

execute_process(
  COMMAND "${XVFB_RUN}" -a -s "-screen 0 1400x900x24"
          "${CMAKE_COMMAND}" -E env
          "HOME=${runtime_dir}/home"
          "SWORD_PATH=${sword_path}"
          "XDG_CONFIG_HOME=${runtime_dir}/config"
          "XDG_DATA_HOME=${runtime_dir}/data"
          "XDG_CACHE_HOME=${runtime_dir}/cache"
          "XDG_STATE_HOME=${runtime_dir}/state"
          "GDK_BACKEND=x11"
          "BIBLIA_ELIM_AC_PROBE=1"
          "${APP}"
  RESULT_VARIABLE app_status
  OUTPUT_VARIABLE app_stdout
  ERROR_VARIABLE app_stderr
  TIMEOUT 120)

set(output "${app_stdout}\n${app_stderr}")
file(REMOVE_RECURSE "${runtime_dir}")

# Same environment caveat the lifecycle smoke carries: a sandbox with no
# usable X11 socket is an unavailable test environment, not a result.
if(output MATCHES "Xvfb failed to start|Cannot establish any listening sockets|Unable to init server|xvfb-run:[^\n]*kill:")
  message("author_commentary_probe_skipped=no-usable-xvfb")
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.29)
    cmake_language(EXIT 77)
  endif()
  return()
endif()
if(output MATCHES "author_commentary_probe_skipped=")
  message("${output}")
  return()
endif()
if(NOT output MATCHES "author_commentary_probe_failures=0")
  message(FATAL_ERROR "Author commentary probe failed:\n${output}")
endif()
if(NOT app_status EQUAL 0)
  message(FATAL_ERROR "Author commentary probe exited ${app_status}:\n${output}")
endif()
message("${output}")
